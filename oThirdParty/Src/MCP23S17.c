/*
 * MCP23S17.c
 *
 *  Version: 0.2 (2026-10-20)
 */

#include "MCP23S17.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"

/* ──────────────────────────────────────────────
 *  Internal: CS Control
 * ────────────────────────────────────────────── */

static void MCP23S17_CS_Low(MCP23S17_t *pDev)
{
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, pDev->CS.ActiveLevel);
	}
}

static void MCP23S17_CS_High(MCP23S17_t *pDev)
{
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}
}

/* ──────────────────────────────────────────────
 *  Internal: Pack per-pin Param to register bytes
 * ────────────────────────────────────────────── */

static void MCP23S17_PackParam(MCP23S17_Param_t *pParam, uint8_t *pDirA, uint8_t *pDirB, uint8_t *pPullUpA, uint8_t *pPullUpB)
{
	*pDirA = (pParam->Dir_GPA0 & 0x01)
		   | ((pParam->Dir_GPA1 & 0x01) << 1)
		   | ((pParam->Dir_GPA2 & 0x01) << 2)
		   | ((pParam->Dir_GPA3 & 0x01) << 3)
		   | ((pParam->Dir_GPA4 & 0x01) << 4)
		   | ((pParam->Dir_GPA5 & 0x01) << 5)
		   | ((pParam->Dir_GPA6 & 0x01) << 6)
		   | ((pParam->Dir_GPA7 & 0x01) << 7);

	*pDirB = (pParam->Dir_GPB0 & 0x01)
		   | ((pParam->Dir_GPB1 & 0x01) << 1)
		   | ((pParam->Dir_GPB2 & 0x01) << 2)
		   | ((pParam->Dir_GPB3 & 0x01) << 3)
		   | ((pParam->Dir_GPB4 & 0x01) << 4)
		   | ((pParam->Dir_GPB5 & 0x01) << 5)
		   | ((pParam->Dir_GPB6 & 0x01) << 6)
		   | ((pParam->Dir_GPB7 & 0x01) << 7);

	*pPullUpA = (pParam->PullUp_GPA0 & 0x01)
			  | ((pParam->PullUp_GPA1 & 0x01) << 1)
			  | ((pParam->PullUp_GPA2 & 0x01) << 2)
			  | ((pParam->PullUp_GPA3 & 0x01) << 3)
			  | ((pParam->PullUp_GPA4 & 0x01) << 4)
			  | ((pParam->PullUp_GPA5 & 0x01) << 5)
			  | ((pParam->PullUp_GPA6 & 0x01) << 6)
			  | ((pParam->PullUp_GPA7 & 0x01) << 7);

	*pPullUpB = (pParam->PullUp_GPB0 & 0x01)
			  | ((pParam->PullUp_GPB1 & 0x01) << 1)
			  | ((pParam->PullUp_GPB2 & 0x01) << 2)
			  | ((pParam->PullUp_GPB3 & 0x01) << 3)
			  | ((pParam->PullUp_GPB4 & 0x01) << 4)
			  | ((pParam->PullUp_GPB5 & 0x01) << 5)
			  | ((pParam->PullUp_GPB6 & 0x01) << 6)
			  | ((pParam->PullUp_GPB7 & 0x01) << 7);
}

/* ──────────────────────────────────────────────
 *  Internal: SPI Health Check
 * ────────────────────────────────────────────── */

/* SPI HAL State 확인.
 * - State == HAL_SPI_STATE_READY (RESET/BUSY/ERROR 아님)
 * - ErrorCode == NONE (MODF/OVR/FRE/CRC 아님)
 * SPE 비트는 HAL_SPI_Transmit/Receive 진입 시 자동 세팅되므로 여기서 검사하지 않음. */
static uint8_t MCP23S17_IsSpiReady(SPI_HandleTypeDef *pSPI)
{
	if(pSPI == NULL || pSPI->Instance == NULL){
		return 0;
	}
	if(pSPI->State != HAL_SPI_STATE_READY){
		return 0;
	}
	if(pSPI->ErrorCode != HAL_SPI_ERROR_NONE){
		return 0;
	}
	return 1;
}

/* ──────────────────────────────────────────────
 *  SPI Recovery (ATCommend oAT_UartRecovery 패턴)
 * ────────────────────────────────────────────── */

oResult_t MCP23S17_SpiRecovery(MCP23S17_t *pDev)
{
	static uint32_t RecoveryTimer;

	if(pDev == NULL || pDev->pSPI == NULL){
		return RESULT_NULL;
	}

	/* 정상 동작 중이면 통과 */
	if(MCP23S17_IsSpiReady(pDev->pSPI)){
		return RESULT_OK;
	}

	/* 비정상: 500ms throttle 후 재초기화 시도 */
	if(!oTMR_Trigger(&RecoveryTimer, 500, 1, TICKBASE_SYSTICK)){
		return RESULT_RUN;
	}

	/* 진행 중 전송 강제 종료 + 에러 플래그 클리어 */
	HAL_SPI_Abort(pDev->pSPI);

	if(HAL_SPI_DeInit(pDev->pSPI) != HAL_OK){
		oSerial_Log("MCP23S17", "SPI Recovery DeInit FAIL");
		return RESULT_FAULT;
	}
	if(HAL_SPI_Init(pDev->pSPI) != HAL_OK){
		oSerial_Log("MCP23S17", "SPI Recovery Init FAIL");
		return RESULT_FAULT;
	}

	/* CS 안전 상태로 복원 */
	MCP23S17_CS_High(pDev);

	oSerial_Log("MCP23S17", "SPI Recovery OK");
	return RESULT_RUN;
}

/* ──────────────────────────────────────────────
 *  Register Access
 * ────────────────────────────────────────────── */

oResult_t MCP23S17_WriteRegister(MCP23S17_t *pDev, uint8_t Reg, uint8_t Data)
{
	uint8_t txBuf[3];

	if(pDev == NULL || pDev->pSPI == NULL){
		return RESULT_NULL;
	}

	/* SPI 비정상 시 자동 복구 시도 후 차단 */
	if(!MCP23S17_IsSpiReady(pDev->pSPI)){
		MCP23S17_SpiRecovery(pDev);
		return RESULT_ERROR;
	}

	txBuf[0] = MCP23S17_CTRL_WRITE(pDev->HWAddress);
	txBuf[1] = Reg;
	txBuf[2] = Data;

	MCP23S17_CS_Low(pDev);
	HAL_StatusTypeDef hal = HAL_SPI_Transmit(pDev->pSPI, txBuf, 3, MCP23S17_SPI_TIMEOUT);
	MCP23S17_CS_High(pDev);

	if(hal != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t MCP23S17_ReadRegister(MCP23S17_t *pDev, uint8_t Reg, uint8_t *pData)
{
	uint8_t txBuf[3];
	uint8_t rxBuf[3];

	if(pDev == NULL || pDev->pSPI == NULL || pData == NULL){
		return RESULT_NULL;
	}

	/* SPI 비정상 시 자동 복구 시도 후 차단 */
	if(!MCP23S17_IsSpiReady(pDev->pSPI)){
		MCP23S17_SpiRecovery(pDev);
		return RESULT_ERROR;
	}

	txBuf[0] = MCP23S17_CTRL_READ(pDev->HWAddress);
	txBuf[1] = Reg;
	txBuf[2] = 0x00;

	MCP23S17_CS_Low(pDev);
	HAL_StatusTypeDef hal = HAL_SPI_TransmitReceive(pDev->pSPI, txBuf, rxBuf, 3, MCP23S17_SPI_TIMEOUT);
	MCP23S17_CS_High(pDev);

	if(hal != HAL_OK){
		return RESULT_ERROR;
	}

	*pData = rxBuf[2];
	return RESULT_OK;
}

/* ──────────────────────────────────────────────
 *  Reset
 * ────────────────────────────────────────────── */

oResult_t MCP23S17_Reset(MCP23S17_t *pDev)
{
	static uint8_t ResetStep = 0;
	static uint32_t ResetTimer = 0;

	if(pDev == NULL || pDev->pRST == NULL){
		ResetStep = 0;
		return RESULT_ERROR;
	}

	oResult_t result = RESULT_RUN;

	switch(ResetStep)
	{
		case 0:
			// RST Low (active)
			HAL_GPIO_WritePin(pDev->pRST->Port, pDev->pRST->Pin, GPIO_PIN_RESET);
			ResetTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			ResetStep++;
			break;
		case 1:
			// Hold low for 1ms (datasheet: min 1us)
			if(oTMR_Elapsed(&ResetTimer, 1, TICKBASE_SYSTICK)){
				HAL_GPIO_WritePin(pDev->pRST->Port, pDev->pRST->Pin, GPIO_PIN_SET);
				ResetTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				ResetStep++;
			}
			break;
		case 2:
			// Wait 5ms for stabilization
			if(oTMR_Elapsed(&ResetTimer, 5, TICKBASE_SYSTICK)){
				oSerial_Log("MCP23S17", "Reset OK");
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		ResetStep = 0;
	}

	return result;
}

/* ──────────────────────────────────────────────
 *  Init / DeInit
 * ────────────────────────────────────────────── */

/**
 * @brief  MCP23S17 초기화 (HW Reset → IOCON → Param 적용)
 * @param  pDev    : 디바이스 구조체 포인터
 * @param  pSPI    : HAL SPI 핸들 포인터
 * @param  CS      : CS 핀 (oIO_t 값), Port == NULL 이면 CS 없음, ActiveLevel = 핀 활성 레벨
 * @param  pRST    : Reset 핀 (oIO_t*), NULL이면 HW Reset 생략
 * @param  HWAddr  : 하드웨어 주소 0~7 (A2,A1,A0)
 * @param  pParam  : 초기 설정 (MCP23S17_Param_t*)
 *                   - Dir_GPAx/Dir_GPBx    : 핀별 방향 (1=Input, 0=Output)
 *                   - PullUp_GPAx/PullUp_GPBx : 핀별 풀업 (1=Enable, 0=Disable)
 * @retval RESULT_RUN(진행중), RESULT_OK(완료), RESULT_FAULT(실패)
 */
oResult_t MCP23S17_Init(MCP23S17_t *pDev, SPI_HandleTypeDef *pSPI, oIO_t CS, oIO_t *pRST, uint8_t HWAddr, MCP23S17_Param_t *pParam)
{
	static uint8_t InitStep = 0;
	static uint32_t InitTimer = 0;
	static MCP23S17_Param_t *pInitParam;

	if(pDev == NULL || pSPI == NULL || pParam == NULL){
		InitStep = 0;
		return RESULT_ERROR;
	}

	oResult_t result = RESULT_RUN;

	switch(InitStep)
	{
		case 0:
			// Fault 상태면 DeInit 후 재초기화
			if(pDev->IsFault){
				oSerial_Log("MCP23S17", "Fault detected (code=%d val=0x%02X), DeInit first", pDev->FaultCode, pDev->FaultValue);
				return RESULT_FAULT;
			}

			if(HAL_SPI_DeInit(pSPI) != HAL_OK){
				oSerial_Log("MCP23S17", "SPI DeInit FAIL");
				return RESULT_FAULT;
			}

			oSerial_Log("MCP23S17", "Init Start (Addr=%d)", HWAddr);

			// Store handles
			pDev->pSPI = pSPI;
			pDev->CS = CS;
			pDev->pRST = pRST;
			pDev->HWAddress = HWAddr & 0x07;
			pDev->IsOpen = 0;
			pInitParam = pParam;

			// SPI Init (BaudRatePrescaler는 CubeMX 설정 유지)
			pSPI->Init.Mode = SPI_MODE_MASTER;
			pSPI->Init.Direction = SPI_DIRECTION_2LINES;
			pSPI->Init.DataSize = SPI_DATASIZE_8BIT;
			pSPI->Init.CLKPolarity = SPI_POLARITY_LOW;
			pSPI->Init.CLKPhase = SPI_PHASE_1EDGE;
			pSPI->Init.NSS = SPI_NSS_SOFT;
			pSPI->Init.FirstBit = SPI_FIRSTBIT_MSB;
			pSPI->Init.TIMode = SPI_TIMODE_DISABLE;
			pSPI->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
			pSPI->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

			if(HAL_SPI_Init(pSPI) != HAL_OK){
				oSerial_Log("MCP23S17", "SPI Init FAIL");
				pDev->IsFault = 1;
				pDev->FaultCode = 1;
				result = RESULT_FAULT;
				break;
			}

			// Cache = MCP23S17 reset defaults (IODIR=0xFF, GPPU=0x00)
			pDev->CacheDirA = 0xFF;
			pDev->CacheDirB = 0xFF;
			pDev->CachePullUpA = 0x00;
			pDev->CachePullUpB = 0x00;
			pDev->CacheOlatA = 0x00;
			pDev->CacheOlatB = 0x00;

			// CS High (inactive)
			MCP23S17_CS_High(pDev);

			if(pRST != NULL){
				// Start hardware reset (active)
				HAL_GPIO_WritePin(pDev->pRST->Port, pDev->pRST->Pin, pDev->pRST->ActiveLevel);
				InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				InitStep = 1;
			}
			else{
				// No reset pin, skip to IOCON
				InitStep = 3;
			}
			break;

		case 1:
			// Hold RST active 1ms
			if(oTMR_Elapsed(&InitTimer, 1, TICKBASE_SYSTICK)){
				HAL_GPIO_WritePin(pDev->pRST->Port, pDev->pRST->Pin, !pDev->pRST->ActiveLevel);
				InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				InitStep++;
			}
			break;

		case 2:
			// Wait 5ms stabilization after reset
			if(oTMR_Elapsed(&InitTimer, 5, TICKBASE_SYSTICK)){
				InitStep++;
			}
			break;

		case 3:
		{
			// IOCON: HAEN=1 (hardware address enable), SEQOP=0 (sequential), MIRROR=0
			// SPI ready 확인 후 직접 HAL 호출로 실패 원인 분리
			uint8_t spiReady = MCP23S17_IsSpiReady(pSPI);
			uint32_t cr1 = pSPI->Instance ? pSPI->Instance->CR1 : 0;
			uint32_t state = pSPI->State;
			uint32_t errcode = pSPI->ErrorCode;

			if(!spiReady){
				oSerial_Log("MCP23S17", "Init IOCON FAIL: SPI not ready CR1=0x%04lX St=%lu Err=0x%lX",
					cr1, state, errcode);
				pDev->IsFault = 1;
				pDev->FaultCode = 2;
				pDev->FaultValue = 0xA0;  // SPI not ready
				result = RESULT_FAULT;
				break;
			}

			uint8_t txBuf[3] = {
				MCP23S17_CTRL_WRITE(pDev->HWAddress),
				MCP23S17_REG_IOCON,
				MCP23S17_IOCON_HAEN
			};
			MCP23S17_CS_Low(pDev);
			HAL_StatusTypeDef hal = HAL_SPI_Transmit(pSPI, txBuf, 3, MCP23S17_SPI_TIMEOUT);
			MCP23S17_CS_High(pDev);

			if(hal != HAL_OK){
				oSerial_Log("MCP23S17", "Init IOCON FAIL: HAL=%d Err=0x%lX", hal, pSPI->ErrorCode);
				pDev->IsFault = 1;
				pDev->FaultCode = 2;
				pDev->FaultValue = (uint8_t)(0xB0 | (hal & 0x0F));  // HAL error
				result = RESULT_FAULT;
				break;
			}
			InitStep++;
			break;
		}

		case 4:
		{
			// Verify: read back IOCON
			uint8_t iocon = 0;
			if(MCP23S17_ReadRegister(pDev, MCP23S17_REG_IOCON, &iocon) != RESULT_OK){
				oSerial_Log("MCP23S17", "Init Verify FAIL");
				pDev->IsFault = 1;
				pDev->FaultCode = 3;
				result = RESULT_FAULT;
				break;
			}

			if((iocon & MCP23S17_IOCON_HAEN) == 0){
				oSerial_Log("MCP23S17", "Init HAEN not set (0x%02X)", iocon);
				pDev->IsFault = 1;
				pDev->FaultCode = 4;
				pDev->FaultValue = iocon;
				result = RESULT_FAULT;
				break;
			}

			// IsOpen=1 → SetParameter로 Dir/PullUp 일괄 적용
			pDev->IsOpen = 1;

			if(MCP23S17_SetParameter(pDev, pInitParam) != RESULT_OK){
				oSerial_Log("MCP23S17", "Init SetParameter FAIL");
				pDev->IsOpen = 0;
				pDev->IsFault = 1;
				pDev->FaultCode = 5;
				result = RESULT_FAULT;
				break;
			}

			oSerial_Log("MCP23S17", "Init OK IOCON=0x%02X", iocon);
			result = RESULT_OK;
			break;
		}
	}

	if(result != RESULT_RUN){
		InitStep = 0;
	}

	return result;
}

oResult_t MCP23S17_DeInit(MCP23S17_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(pDev->IsOpen){
		// Reset all outputs to safe state (all input)
		MCP23S17_WriteRegister(pDev, MCP23S17_REG_IODIRA, 0xFF);
		MCP23S17_WriteRegister(pDev, MCP23S17_REG_IODIRB, 0xFF);
		MCP23S17_WriteRegister(pDev, MCP23S17_REG_OLATA, 0x00);
		MCP23S17_WriteRegister(pDev, MCP23S17_REG_OLATB, 0x00);
	}

	// SPI DeInit
	if(pDev->pSPI != NULL){
		HAL_SPI_DeInit(pDev->pSPI);
	}

	pDev->IsFault = 0;
	pDev->IsOpen = 0;
	oSerial_Log("MCP23S17", "DeInit OK");

	return RESULT_OK;
}

/* ──────────────────────────────────────────────
 *  Configuration
 * ────────────────────────────────────────────── */

/**
 * @brief  Parameter 구조체로 Direction + PullUp 일괄 설정
 * @param  pDev   : 디바이스 구조체 포인터
 * @param  pParam : 설정 구조체 (MCP23S17_Param_t*)
 *                  - Dir_GPAx/Dir_GPBx    : 핀별 방향 (1=Input, 0=Output)
 *                  - PullUp_GPAx/PullUp_GPBx : 핀별 풀업 (1=Enable, 0=Disable)
 * @retval RESULT_OK(성공), RESULT_ERROR(실패)
 */
oResult_t MCP23S17_SetParameter(MCP23S17_t *pDev, MCP23S17_Param_t *pParam)
{
	if(pDev == NULL || !pDev->IsOpen || pParam == NULL){
		return RESULT_NULL;
	}

	uint8_t dirA, dirB, pullUpA, pullUpB;
	MCP23S17_PackParam(pParam, &dirA, &dirB, &pullUpA, &pullUpB);

	if(dirA != pDev->CacheDirA){
		if(MCP23S17_WriteRegister(pDev, MCP23S17_REG_IODIRA, dirA) != RESULT_OK) return RESULT_ERROR;
		pDev->CacheDirA = dirA;
	}
	if(dirB != pDev->CacheDirB){
		if(MCP23S17_WriteRegister(pDev, MCP23S17_REG_IODIRB, dirB) != RESULT_OK) return RESULT_ERROR;
		pDev->CacheDirB = dirB;
	}
	if(pullUpA != pDev->CachePullUpA){
		if(MCP23S17_WriteRegister(pDev, MCP23S17_REG_GPPUA, pullUpA) != RESULT_OK) return RESULT_ERROR;
		pDev->CachePullUpA = pullUpA;
	}
	if(pullUpB != pDev->CachePullUpB){
		if(MCP23S17_WriteRegister(pDev, MCP23S17_REG_GPPUB, pullUpB) != RESULT_OK) return RESULT_ERROR;
		pDev->CachePullUpB = pullUpB;
	}

	return RESULT_OK;
}

/* ──────────────────────────────────────────────
 *  GPIO Read / Write
 * ────────────────────────────────────────────── */

oResult_t MCP23S17_ReadPort(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pData)
{
	if(pDev == NULL || !pDev->IsOpen || pData == NULL){
		return RESULT_NULL;
	}

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_GPIOA : MCP23S17_REG_GPIOB;
	return MCP23S17_ReadRegister(pDev, reg, pData);
}

oResult_t MCP23S17_WritePort(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Data)
{
	if(pDev == NULL || !pDev->IsOpen){
		return RESULT_NULL;
	}

	uint8_t *pCache = (Port == MCP23S17_PORTA) ? &pDev->CacheOlatA : &pDev->CacheOlatB;

	if(*pCache == Data){
		return RESULT_OK;
	}

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_OLATA : MCP23S17_REG_OLATB;
	oResult_t result = MCP23S17_WriteRegister(pDev, reg, Data);

	if(result == RESULT_OK){
		*pCache = Data;
	}

	return result;
}

oResult_t MCP23S17_ReadPin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin, uint8_t *pState)
{
	if(pDev == NULL || !pDev->IsOpen || Pin > 7 || pState == NULL){
		return RESULT_NULL;
	}

	uint8_t portVal = 0;

	if(MCP23S17_ReadPort(pDev, Port, &portVal) != RESULT_OK){
		return RESULT_ERROR;
	}

	*pState = (portVal >> Pin) & 0x01;
	return RESULT_OK;
}

oResult_t MCP23S17_WritePin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin, uint8_t State)
{
	if(pDev == NULL || !pDev->IsOpen || Pin > 7){
		return RESULT_NULL;
	}

	uint8_t *pCache = (Port == MCP23S17_PORTA) ? &pDev->CacheOlatA : &pDev->CacheOlatB;
	uint8_t newVal = *pCache;

	if(State){
		newVal |= (1 << Pin);
	}
	else{
		newVal &= ~(1 << Pin);
	}

	if(newVal == *pCache){
		return RESULT_OK;
	}

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_OLATA : MCP23S17_REG_OLATB;
	oResult_t result = MCP23S17_WriteRegister(pDev, reg, newVal);

	if(result == RESULT_OK){
		*pCache = newVal;
	}

	return result;
}

oResult_t MCP23S17_TogglePin(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Pin)
{
	if(pDev == NULL || !pDev->IsOpen || Pin > 7){
		return RESULT_NULL;
	}

	uint8_t *pCache = (Port == MCP23S17_PORTA) ? &pDev->CacheOlatA : &pDev->CacheOlatB;
	uint8_t newVal = *pCache ^ (1 << Pin);

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_OLATA : MCP23S17_REG_OLATB;
	oResult_t result = MCP23S17_WriteRegister(pDev, reg, newVal);

	if(result == RESULT_OK){
		*pCache = newVal;
	}

	return result;
}

/* ──────────────────────────────────────────────
 *  Interrupt
 * ────────────────────────────────────────────── */

oResult_t MCP23S17_SetInterrupt(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t Mask, uint8_t Mode)
{
	if(pDev == NULL || !pDev->IsOpen){
		return RESULT_NULL;
	}

	uint8_t regEn  = (Port == MCP23S17_PORTA) ? MCP23S17_REG_GPINTENA : MCP23S17_REG_GPINTENB;
	uint8_t regCon = (Port == MCP23S17_PORTA) ? MCP23S17_REG_INTCONA  : MCP23S17_REG_INTCONB;

	// INTCON: 1=compare with DEFVAL, 0=compare with previous
	if(MCP23S17_WriteRegister(pDev, regCon, Mode) != RESULT_OK){
		return RESULT_ERROR;
	}

	// GPINTEN: enable interrupt mask
	return MCP23S17_WriteRegister(pDev, regEn, Mask);
}

oResult_t MCP23S17_GetInterruptFlag(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pFlag)
{
	if(pDev == NULL || !pDev->IsOpen || pFlag == NULL){
		return RESULT_NULL;
	}

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_INTFA : MCP23S17_REG_INTFB;
	return MCP23S17_ReadRegister(pDev, reg, pFlag);
}

oResult_t MCP23S17_GetInterruptCapture(MCP23S17_t *pDev, MCP23S17_Port_t Port, uint8_t *pCapture)
{
	if(pDev == NULL || !pDev->IsOpen || pCapture == NULL){
		return RESULT_NULL;
	}

	uint8_t reg = (Port == MCP23S17_PORTA) ? MCP23S17_REG_INTCAPA : MCP23S17_REG_INTCAPB;
	return MCP23S17_ReadRegister(pDev, reg, pCapture);
}
