/*
 * TEMP_TMP1075NDRLR.c
 *
 *  Version: 0.11 (2026-10-20)
 */
#include "TEMP_TMP1075NDRLR.h"
#include "ONE_Serial.h"

/* 진단 로그 — ONE_Serial.h 는 .h 가 아니라 여기(.c)에만 들여온다. 헤더에 넣으면 이
   드라이버를 쓰는 모든 프로젝트가 로깅 의존성을 떠안는다.
   oSerial_Log 는 런타임 no-op 이지만 포맷 문자열은 플래시에 남으므로 컴파일 타임 게이트를 둔다 */
#ifndef TMP1075_LOG_ENABLE
#define TMP1075_LOG_ENABLE   1
#endif

#if TMP1075_LOG_ENABLE
#define TMP1075_LOG(fmt, ...)   oSerial_Log("TMP", fmt, ##__VA_ARGS__)
#else
#define TMP1075_LOG(fmt, ...)   do{}while(0)
#endif

#define TMP1075_I2C_ADDRESS 	(0x48 << 1)

#define TMP1075_I2C_RECOVERY_THRESHOLD	5      /* HAL_ERROR 연속 누적 임계 */

oResult_t TMP1075_Recovery(TMP1075_t *pTMP1075)
{
	if(pTMP1075 == NULL || pTMP1075->pI2C == NULL){
		return RESULT_NULL;
	}

	TMP1075_LOG("Recovery - I2C re-init (errors=%u)",
	            (unsigned int)pTMP1075->State.CountOfI2CError);

	HAL_I2C_DeInit(pTMP1075->pI2C);
	if(HAL_I2C_Init(pTMP1075->pI2C) != HAL_OK){
		TMP1075_LOG("Recovery FAIL - HAL_I2C_Init");
		return RESULT_ERROR;
	}

	pTMP1075->State.CountOfI2CError = 0;
	return RESULT_OK;
}

static void TMP1075_OnHalError(TMP1075_t *pTMP1075)
{
	if(pTMP1075->State.CountOfI2CError < 0xFF){
		pTMP1075->State.CountOfI2CError++;
	}

	/* Read/Write 각 호출이 아니라 여기서만 남긴다 — 상위가 주기 폴링이라
	   버스 결함 시 트랜잭션 단위 로그는 링버퍼를 덮는다 */
	TMP1075_LOG("HalError count=%u HalErr=0x%08lX",
	            (unsigned int)pTMP1075->State.CountOfI2CError,
	            (unsigned long)(pTMP1075->pI2C ? HAL_I2C_GetError(pTMP1075->pI2C) : 0));

	if(pTMP1075->State.CountOfI2CError >= TMP1075_I2C_RECOVERY_THRESHOLD){
		TMP1075_Recovery(pTMP1075);
	}
}

oResult_t TMP1075_Write(TMP1075_t *pTMP1075, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	oResult_t result = RESULT_RUN;

	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}
	else if(pTMP1075->State.IsReadMode){
		return RESULT_BUSY;
	}
	else if(!pTMP1075->State.IsWriteMode && pTMP1075->State.ProcessStep){
		pTMP1075->State.ProcessStep = 0;
	}

	switch(pTMP1075->State.ProcessStep)
	{
		case 0:
			if(pTMP1075->pI2C->State == HAL_I2C_STATE_READY){
				pTMP1075->State.IsWriteMode = 1;
				pTMP1075->State.ProcessStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, 500) == HAL_OK){
				pTMP1075->State.CountOfI2CError = 0;
				result = RESULT_OK;
			}
			else{
				TMP1075_OnHalError(pTMP1075);
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.ProcessStep = 0;
		pTMP1075->State.IsWriteMode = 0;
	}

	return result;
}

oResult_t TMP1075_Read(TMP1075_t *pTMP1075, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	oResult_t result = RESULT_RUN;

	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}
	else if(pTMP1075->State.IsWriteMode){
		return RESULT_BUSY;
	}
	else if(!pTMP1075->State.IsReadMode && pTMP1075->State.ProcessStep){
		pTMP1075->State.ProcessStep = 0;
	}

	switch(pTMP1075->State.ProcessStep)
	{
		case 0:
			if(pTMP1075->pI2C->State == HAL_I2C_STATE_READY){
				pTMP1075->State.IsReadMode = 1;
				pTMP1075->State.ProcessStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 1:
			if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, 500) == HAL_OK){
				pTMP1075->State.CountOfI2CError = 0;
				result = RESULT_OK;
			}
			else{
				TMP1075_OnHalError(pTMP1075);
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.ProcessStep = 0;
		pTMP1075->State.IsReadMode = 0;
	}

	return result;
}

oResult_t TMP1075_GetData(TMP1075_t *pTMP1075, uint8_t Count)
{
	oResult_t result = RESULT_RUN;

	if((result=TMP1075_Read(pTMP1075, TMP1075_REGADR_TEMP, (uint8_t *)&pTMP1075->Register.TEMP, sizeof(pTMP1075->Register.TEMP))) == RESULT_OK){

		if(pTMP1075->State.CountOfSampling == 0){
			pTMP1075->State.SumOfSampling = 0;
		}

		pTMP1075->State.SumOfSampling += TMP1075_TO_CELSIUS(pTMP1075->Register.TEMP);

		if(++pTMP1075->State.CountOfSampling >= Count){
			pTMP1075->Temperature = pTMP1075->State.SumOfSampling / (double)pTMP1075->State.CountOfSampling;
			pTMP1075->State.CountOfSampling = 0;
		}
		else{
			result = RESULT_RUN;
		}
	}

	return result;
}

oResult_t TMP1075_GetParameter(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pTMP1075->pI2C->State != HAL_I2C_STATE_READY){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_CFGR, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.CFGR, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_LLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.LLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_HLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.HLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_DIEID, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.DIEID, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t TMP1075_SetParameter(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pTMP1075->pI2C->State != HAL_I2C_STATE_READY){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_CFGR, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.CFGR, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_LLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.LLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pTMP1075->pI2C, TMP1075_I2C_ADDRESS, TMP1075_REGADR_HLIM, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&pTMP1075->Register.HLIM, 2, 10) != HAL_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t TMP1075_Open(I2C_HandleTypeDef *pI2C, TMP1075_t *pTMP1075)
{
	oResult_t result = RESULT_RUN;

	if(pI2C == NULL || pTMP1075 == NULL){
		return RESULT_ERROR;
	}

	if(pTMP1075->State.IsOpen && !pTMP1075->State.IsOpening){
		return RESULT_OK;
	}
	else if(!pTMP1075->State.IsOpening && pTMP1075->State.OpenStep != 0){
		memset(&pTMP1075->State, 0, sizeof(pTMP1075->State));

		pTMP1075->State.OpenStep = 0;
		pTMP1075->State.IsOpening = 1;
	}

	switch(pTMP1075->State.OpenStep)
	{
		case 0:
			HAL_I2C_DeInit(pI2C);
			pTMP1075->State.OpenStep++;
			break;
		case 1:
			if(pI2C->State != HAL_I2C_STATE_RESET){
				pTMP1075->pI2C = pI2C;
				pTMP1075->State.IsOpen = 1;
				pTMP1075->State.OpenStep++;
			}
			else if(HAL_I2C_Init(pI2C) != HAL_OK){
				TMP1075_LOG("Open step1 FAIL - HAL_I2C_Init");
				result = RESULT_ERROR;
			}
			break;
		case 2:
			if(TMP1075_GetParameter(pTMP1075) == RESULT_OK){
				pTMP1075->State.OpenStep++;
			}
			else{
				TMP1075_LOG("Open step2 FAIL - GetParameter (addr 0x%02X, I2CErr=%u)",
				            (unsigned int)(TMP1075_I2C_ADDRESS >> 1),
				            (unsigned int)pTMP1075->State.CountOfI2CError);
				result = RESULT_ERROR;
			}
			break;
		case 3:
			TMP1075_LOG("Open OK - DIEID=0x%04X LLIM=0x%04X HLIM=0x%04X",
			            (unsigned int)pTMP1075->Register.DIEID,
			            (unsigned int)pTMP1075->Register.LLIM,
			            (unsigned int)pTMP1075->Register.HLIM);
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		pTMP1075->State.OpenStep = 0;
		pTMP1075->State.IsOpening = 0;
	}

	return result;
}

oResult_t TMP1075_Close(TMP1075_t *pTMP1075)
{
	if(!pTMP1075->State.IsOpen){
		return RESULT_OK;
	}

	/* ⚠ 이 DeInit 은 I2C 페리페럴 전체를 내린다. HW1.4 는 EEPROM 이 같은 I2C1 에 물려
	   있어 여기서 EEPROM 도 함께 죽는다 — 동작은 그대로 두되 추적 가능하게 남긴다.
	   (증상: TMP1075_Close 이후 EEPROM 접근이 조용히 실패) */
	TMP1075_LOG("Close - HAL_I2C_DeInit on SHARED bus (EEPROM also affected)");

	HAL_I2C_DeInit(pTMP1075->pI2C);

	memset(&pTMP1075->State, 0, sizeof(pTMP1075->State));

	return RESULT_OK;
}
