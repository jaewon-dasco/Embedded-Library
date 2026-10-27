/*
 * oModbus_RS485.c
 *
 *  Version: 0.61 (2026-10-20)
 */
#include "string.h"
#include "ONE_Time.h"
#include "oModbus_RS485.h"

/* ============================================================================
 * Modbus CRC16 (poly 0xA001 reflected, init 0xFFFF) — OneLibrary CRC16은 CCITT라 별도
 * ========================================================================= */
uint16_t oModbus_CRC16(const uint8_t *pData, uint16_t Length)
{
	uint16_t crc = 0xFFFF;

	for(uint16_t i = 0; i < Length; i++){
		crc ^= (uint16_t)pData[i];
		for(uint8_t b = 0; b < 8; b++){
			if(crc & 0x0001){
				crc = (crc >> 1) ^ 0xA001;
			}
			else{
				crc >>= 1;
			}
		}
	}

	return crc;
}

/* ============================================================================
 * 초기화 — 외부 공유 핸들러 바인딩 + 콜백 등록 (UART·RS485·DMA 셋업 없음)
 *   UART 초기화(HAL_RS485Ex_Init·순환 RX DMA arm)는 핸들러 소유자(앱)가 수행.
 *   → 공유 시리얼(Modbus·Log·Calibration)의 UART 소유권을 앱으로 일원화.
 * ========================================================================= */
oResult_t oModbus_Init(oModbus_t *pModbus, oSerialHandler_t *pSerial, uint8_t SlaveAddress, const oModbus_Callbacks_t *pCallbacks)
{
	if(pModbus == NULL || pSerial == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL){
		return RESULT_NULL;
	}

	memset(pModbus, 0, sizeof(oModbus_t));

	pModbus->pSerial      = pSerial;   /* 외부 공유 핸들러 (UART·RS485·DMA·버퍼는 앱이 셋업) */
	pModbus->SlaveAddress = SlaveAddress;

	if(pCallbacks != NULL){
		pModbus->Callbacks = *pCallbacks;
	}

	pModbus->IsInitialized = 1;

	return RESULT_OK;
}

/* ============================================================================
 * 해제 — 수신 중단 + UART DeInit (재사용/재구성 대비)
 * ========================================================================= */
oResult_t oModbus_DeInit(oModbus_t *pModbus)
{
	if(pModbus == NULL || pModbus->pSerial == NULL || pModbus->pSerial->pUART == NULL){
		return RESULT_NULL;
	}

	HAL_UART_Abort(pModbus->pSerial->pUART);      /* 진행 중 TX/RX 중단 */
	HAL_UART_DeInit(pModbus->pSerial->pUART);

	pModbus->pSerial->IsRegistered = 0;
	pModbus->State.Len       = 0;
	pModbus->State.RxTail    = 0;
	pModbus->State.Receiving = 0;
	pModbus->IsInitialized   = 0;   /* UART 를 내렸으므로 Process 진입을 다시 막는다 */

	return RESULT_OK;
}

/* ============================================================================
 * 응답 송신 — CRC16 부가 후 oSerial DMA 송신 (브로드캐스트는 무응답)
 * ========================================================================= */
static void oModbus_Send(oModbus_t *pModbus, uint8_t *pFrame, uint16_t Length)
{
	uint16_t crc;

	/* 판정 기준은 응답 주소가 아니라 "요청이 브로드캐스트였는가" 다.
	   읽기 응답과 예외 응답은 resp[0] 에 자기 주소를 넣으므로 pFrame[0] 만 보면
	   브로드캐스트 요청에도 답해버린다 — 홀 감지된 개체가 둘이면 그대로 버스 충돌이다 */
	if(pFrame[0] == OMODBUS_BROADCAST_ADDR || pModbus->State.Frame[0] == OMODBUS_BROADCAST_ADDR){
		return;   /* 브로드캐스트 — 응답 금지 */
	}

	crc = oModbus_CRC16(pFrame, Length);
	pFrame[Length++] = (uint8_t)(crc & 0xFF);   /* CRC lo first */
	pFrame[Length++] = (uint8_t)(crc >> 8);

	if(oSerial_Write(pModbus->pSerial, (char *)pFrame, Length) == RESULT_OK){
		pModbus->Stat.TxFrames++;
	}
}

static void oModbus_SendException(oModbus_t *pModbus, uint8_t FunctionCode, oModbus_Exception_t Exception)
{
	uint8_t resp[5];

	resp[0] = pModbus->SlaveAddress;
	resp[1] = FunctionCode | 0x80;
	resp[2] = (uint8_t)Exception;

	pModbus->Stat.Exceptions++;
	oModbus_Send(pModbus, resp, 3);
}

/* ============================================================================
 * FC 디스패치 — 유효 프레임(주소·CRC 확인 완료) 처리
 * ========================================================================= */
static void oModbus_Dispatch(oModbus_t *pModbus)
{
	uint8_t *f   = pModbus->State.Frame;
	uint16_t len = pModbus->State.Len;
	uint8_t  fc  = f[1];
	uint8_t  resp[OMODBUS_ADU_MAXSIZE];
	oModbus_Exception_t exc;

	switch(fc)
	{
		case oModbus_FC_ReadHolding:
		case oModbus_FC_ReadInput:
		{
			oModbus_ReadReg_t cb = (fc == oModbus_FC_ReadInput) ? pModbus->Callbacks.ReadInput
			                                                    : pModbus->Callbacks.ReadHolding;
			uint16_t start = ((uint16_t)f[2] << 8) | f[3];
			uint16_t qty   = ((uint16_t)f[4] << 8) | f[5];
			uint16_t value;

			if(cb == NULL){
				oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalFunction);
				return;
			}
			if(qty < 1 || qty > OMODBUS_MAX_READ_REGS){
				oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalValue);
				return;
			}

			resp[0] = pModbus->SlaveAddress;
			resp[1] = fc;
			resp[2] = (uint8_t)(qty * 2);

			for(uint16_t i = 0; i < qty; i++){
				if((exc = cb(start + i, &value)) != oModbus_Exception_None){
					oModbus_SendException(pModbus, fc, exc);
					return;
				}
				resp[3 + i * 2] = (uint8_t)(value >> 8);
				resp[4 + i * 2] = (uint8_t)(value & 0xFF);
			}

			oModbus_Send(pModbus, resp, 3 + qty * 2);
			break;
		}

		case oModbus_FC_WriteSingle:
		{
			uint16_t address = ((uint16_t)f[2] << 8) | f[3];
			uint16_t value   = ((uint16_t)f[4] << 8) | f[5];

			if(pModbus->Callbacks.WriteHolding == NULL){
				oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalFunction);
				return;
			}
			if((exc = pModbus->Callbacks.WriteHolding(address, value)) != oModbus_Exception_None){
				oModbus_SendException(pModbus, fc, exc);
				return;
			}

			memcpy(resp, f, 6);   /* 요청 에코 (addr·fc·addr·value) */
			oModbus_Send(pModbus, resp, 6);
			break;
		}

		case oModbus_FC_WriteMultiple:
		{
			uint16_t start     = ((uint16_t)f[2] << 8) | f[3];
			uint16_t qty       = ((uint16_t)f[4] << 8) | f[5];
			uint8_t  byteCount = f[6];

			if(pModbus->Callbacks.WriteHolding == NULL){
				oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalFunction);
				return;
			}
			if(qty < 1 || qty > OMODBUS_MAX_WRITE_REGS || byteCount != qty * 2 || len < (uint16_t)(9 + byteCount)){
				oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalValue);
				return;
			}

			for(uint16_t i = 0; i < qty; i++){
				uint16_t value = ((uint16_t)f[7 + i * 2] << 8) | f[8 + i * 2];
				if((exc = pModbus->Callbacks.WriteHolding(start + i, value)) != oModbus_Exception_None){
					oModbus_SendException(pModbus, fc, exc);
					return;
				}
			}

			resp[0] = pModbus->SlaveAddress;
			resp[1] = fc;
			resp[2] = f[2]; resp[3] = f[3];   /* start */
			resp[4] = f[4]; resp[5] = f[5];   /* qty */
			oModbus_Send(pModbus, resp, 6);
			break;
		}

		default:
			oModbus_SendException(pModbus, fc, oModbus_Exception_IllegalFunction);
			break;
	}
}

/* 조립 완료된 프레임 검증(주소·CRC) 후 디스패치 */
static void oModbus_HandleFrame(oModbus_t *pModbus)
{
	uint8_t *f   = pModbus->State.Frame;
	uint16_t len = pModbus->State.Len;
	uint16_t crcRx, crcCalc;

	if(len < 4){
		return;   /* 최소 프레임: 주소1 + FC1 + CRC2 */
	}
	if(f[0] != pModbus->SlaveAddress && f[0] != OMODBUS_BROADCAST_ADDR){
		return;   /* 내 주소 아님 */
	}

	crcRx   = ((uint16_t)f[len - 1] << 8) | f[len - 2];
	crcCalc = oModbus_CRC16(f, len - 2);
	if(crcRx != crcCalc){
		pModbus->Stat.CrcErrors++;
		return;
	}

	pModbus->Stat.RxFrames++;
	oModbus_Dispatch(pModbus);
}

/* ============================================================================
 * 매 tick 호출 — DMA 순환 링버퍼 소비 + T3.5 프레임 경계 판정 + 처리
 * ========================================================================= */
oResult_t oModbus_Process(oModbus_t *pModbus)
{
	UART_HandleTypeDef *pUART;
	uint16_t head;

	if(pModbus == NULL || pModbus->pSerial == NULL || pModbus->pSerial->pUART == NULL || pModbus->pSerial->pUART->hdmarx == NULL || !pModbus->IsInitialized){
		return RESULT_NULL;
	}
	pUART = pModbus->pSerial->pUART;

	/* 수신 오류 복구 — ORE/FE 등 발생 시 DMA 재시작 */
	if(pUART->ErrorCode & (HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE | HAL_UART_ERROR_NE | HAL_UART_ERROR_PE | HAL_UART_ERROR_DMA)){
		HAL_UART_AbortReceive(pUART);
	}
	if(pUART->RxState == HAL_UART_STATE_READY){
		HAL_UART_Receive_DMA(pUART, pModbus->pSerial->pRxBuffer, pModbus->pSerial->SizeOfRxBuffer);
		pModbus->State.RxTail    = 0;
		pModbus->State.Len       = 0;
		pModbus->State.Receiving = 0;
	}

	/* DMA write 포인터 (CNDTR 기반) 까지 신규 바이트 소비 (공유 RX 링) */
	head = (uint16_t)(pModbus->pSerial->SizeOfRxBuffer - ONE_DMA_GET_COUNTER(pUART->hdmarx));

	while(pModbus->State.RxTail != head){
		uint8_t byte = pModbus->pSerial->pRxBuffer[pModbus->State.RxTail];
		pModbus->State.RxTail = (uint16_t)((pModbus->State.RxTail + 1) % pModbus->pSerial->SizeOfRxBuffer);

		if(pModbus->State.Len < OMODBUS_ADU_MAXSIZE){
			pModbus->State.Frame[pModbus->State.Len++] = byte;
		}
		else{
			pModbus->State.Len = 0;   /* 오버런 — 프레임 폐기 */
		}

		pModbus->State.T35Timer  = oTMR_GetTick(TICKBASE_SYSTICK);
		pModbus->State.Receiving = 1;
	}

	/* T3.5 침묵 → 프레임 종료 */
	if(pModbus->State.Receiving && pModbus->State.Len > 0
	   && oTMR_Elapsed(&pModbus->State.T35Timer, OMODBUS_T35_MS, TICKBASE_SYSTICK)){
		oModbus_HandleFrame(pModbus);
		pModbus->State.Len       = 0;
		pModbus->State.Receiving = 0;
	}

	return RESULT_OK;
}
