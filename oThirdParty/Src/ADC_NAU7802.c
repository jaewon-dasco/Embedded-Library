/*
 * ADC_NAU7802.c
 *
 *  Version: 0.9 (2026-08-07)
 */
#include "ONE_Time.h"
#include "ONE_Serial.h"
#include "ADC_NAU7802.h"

#ifdef __HAL_I2C_ENABLE

oResult_t NAU7802_WriteMemory(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Data)
{
	if(pDev == NULL || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Write(pDev->pI2C, NAU7802_ADDRESS, MemoryReg, 1, &Data, 1, 100) == HAL_OK){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_ReadMemory(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t *pData)
{
	if(pDev == NULL || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(HAL_I2C_Mem_Read(pDev->pI2C, NAU7802_ADDRESS, MemoryReg, 1, pData, 1, 100) == HAL_OK){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_ReadBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit)
{
	uint8_t Data = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) == RESULT_OK){
		if(Data & Bit){
			return RESULT_OK;
		}
		else{
			return RESULT_NULL;
		}
	}

	return RESULT_ERROR;
}

oResult_t NAU7802_WriteBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit)
{
	uint8_t Data = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	Data |= Bit;

	if(NAU7802_WriteMemory(pDev, MemoryReg, Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t NAU7802_ResetBit(NAU7802_t *pDev, uint16_t MemoryReg, uint8_t Bit, uint8_t Mask)
{
	uint8_t Data = 0;
	uint8_t CheckData = 0;

	if(NAU7802_ReadMemory(pDev, MemoryReg, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	Data &= (uint8_t)(~Mask);
	Data |= Bit;

	if(NAU7802_WriteMemory(pDev, MemoryReg, Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(NAU7802_ReadMemory(pDev, MemoryReg, &CheckData) != RESULT_OK){
		return RESULT_ERROR;
	}

	return Data == CheckData ? RESULT_OK : RESULT_ERROR;
}

/* NAU7802_SetVLDO
 * Input
 *   Voltage | Unit=1V
 */
oResult_t NAU7802_SetVLDO(NAU7802_t *pDev, NAU7802_VLDO_t VLDO)
{
	oResult_t result = RESULT_NULL;
	uint8_t Data = 0;
	double Voltage = 0;

	switch(VLDO)
	{
		default:
			VLDO = NAU7802_VLDO_4_5V;
		case NAU7802_VLDO_4_5V:
			Voltage = 4500;
			break;
		case NAU7802_VLDO_4_2V:
			Voltage = 4200;
			break;
		case NAU7802_VLDO_3_9V:
			Voltage = 3900;
			break;
		case NAU7802_VLDO_3_6V:
			Voltage = 3600;
			break;
		case NAU7802_VLDO_3_3V:
			Voltage = 3300;
			break;
		case NAU7802_VLDO_3_0V:
			Voltage = 3000;
			break;
		case NAU7802_VLDO_2_7V:
			Voltage = 2700;
			break;
		case NAU7802_VLDO_2_4V:
			Voltage = 2400;
			break;
	}

	if(pDev->VREF == Voltage){
		return RESULT_OK;
	}

	Data |= VLDO;

	if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL1, Data, NAU7802_CTRL1_VLDO_MASK)) != RESULT_OK){
		return result;
	}

	pDev->VREF = Voltage;
	pDev->Registers.VLDO = VLDO;
	pDev->IsNeedCalibration = 1;

	return RESULT_OK;
}

/* NAU7802_SetGain
 * Input
 *   Gain 	| ADC = Vin x Gain / VREF x ADCMax | Uint=1
 *   VREF	| REFP-REFN = VREF 	| Uint=1V
 */
oResult_t NAU7802_SetGain(NAU7802_t *pDev, NAU7802_Gain_t Gain)
{
	oResult_t result = RESULT_NULL;
	uint8_t Data = 0;
	double AmpGain =0;

	switch(Gain)
	{
		case NAU7802_GAIN_X1:	AmpGain = 1;	break;
		case NAU7802_GAIN_X2:	AmpGain = 2;	break;
		case NAU7802_GAIN_X4:	AmpGain = 4;	break;
		case NAU7802_GAIN_X8:	AmpGain = 8;	break;
		case NAU7802_GAIN_X16:	AmpGain = 16;	break;
		case NAU7802_GAIN_X32:	AmpGain = 32;	break;
		case NAU7802_GAIN_X64:	AmpGain = 64;	break;
		case NAU7802_GAIN_X128:	AmpGain = 128;	break;
		case NAU7802_GAIN_BYPASS: AmpGain = 1;	break;
		default: return RESULT_ERROR;
	}

	if(pDev->Registers.Gain == Gain){
		return RESULT_OK;
	}

	if(Gain == NAU7802_GAIN_BYPASS){
		/* PGA bypass: PGA_CTRL.BYPASS = 1 (CTRL1.GAIN은 변경 안 함) */
		if((result = NAU7802_WriteBit(pDev, NAU7802_REG_PGA_CTRL, NAU7802_PGA_BYPASS_ENABLE)) != RESULT_OK){
			return result;
		}
	}
	else{
		/* PGA active: 먼저 BYPASS 비트 클리어 (이전이 BYPASS였을 수 있음) → CTRL1.GAIN 설정 */
		if((result = NAU7802_ResetBit(pDev, NAU7802_REG_PGA_CTRL, 0, NAU7802_PGA_BYPASS_ENABLE)) != RESULT_OK){
			return result;
		}

		Data = (uint8_t)Gain & NAU7802_CTRL1_GAIN_MASK;
		if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL1, Data, NAU7802_CTRL1_GAIN_MASK)) != RESULT_OK){
			return result;
		}
	}

	pDev->AmpGain = AmpGain;
	pDev->Registers.Gain = Gain;
	pDev->IsNeedCalibration = 1;

	return RESULT_OK;
}

/* NAU7802_SetSampling
 * Input
 *   Channel 	| Input max voltage | Uint=1V
 *   VREF 		| REFP-REFN = VREF 	| Uint=1V
 */
oResult_t NAU7802_SetSampling(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate)
{
	oResult_t result = RESULT_NULL;
	uint8_t RegChannel = 0;
	uint8_t RegSamplingRate = 0;

	if(Channel >= 2){
		RegChannel = NAU7802_CTRL2_CHS_CH2;
	}

	RegSamplingRate = (SamplingRate & NAU7802_CTRL2_CRS_MASK);

	if(pDev->Registers.Channel == RegChannel && pDev->Registers.SamplingRate == RegSamplingRate){
		pDev->CurrentChannel = Channel;	/* 레지스터가 이미 일치 = 그 채널이 선택된 상태. 갱신을 건너뛰면 AnalogRead가 매번 채널 전환으로 판정해 flush를 무한 재장전한다 */
		return RESULT_OK;
	}

	if((result = NAU7802_ResetBit(pDev, NAU7802_REG_CTRL2, RegChannel|RegSamplingRate, NAU7802_CTRL2_CHS_MASK|NAU7802_CTRL2_CRS_MASK)) != RESULT_OK){
		return result;
	}

	pDev->Registers.Channel = RegChannel;
	pDev->Registers.SamplingRate = RegSamplingRate;
	pDev->CurrentChannel = Channel;
	pDev->IsNeedCalibration = 1;

	return RESULT_OK;
}

/* NAU7802_Calibrate
 *   내부 오프셋 교정(CALMOD=00) — PGA/ADC 입력을 칩 내부에서 단락해 자체 오프셋을 OCAL에 기록.
 *   현재 선택된 채널(CHS)만 교정 → gain·VLDO·채널·rate 변경 후 반드시 재실행.
 */
oResult_t NAU7802_Calibration(NAU7802_t *pDev)
{
	oResult_t result = RESULT_RUN;
	uint8_t Data = 0;

	if(!pDev->IsOpen || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	switch(pDev->CalStep)
	{
		case 0:	/* CALMOD=00(내부 오프셋) + CALS=1 시작 (CHS/CRS 보존) */
			if(NAU7802_ResetBit(pDev, NAU7802_REG_CTRL2, NAU7802_CTRL2_CALS, NAU7802_CTRL2_CALS|NAU7802_CTRL2_CALMOD_MASK) == RESULT_OK){
				pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);	/* case1 CALS 대기 상한 기준점 */
				pDev->CalStep++;
			}
			else{
				result = RESULT_ERROR;	/* 재시도·로그는 호출자(ADCRead) 세션 단위로 일원화 */
			}
			break;
		case 1:	/* CALS 자동 클리어 대기 → 완료 후 CAL_ERR 판정 */
			if(NAU7802_ReadMemory(pDev, NAU7802_REG_CTRL2, &Data) != RESULT_OK){
				result = RESULT_ERROR;
			}
			else if(!(Data & NAU7802_CTRL2_CALS)){
				result = (Data & NAU7802_CTRL2_CAL_ERR) ? RESULT_ERROR : RESULT_OK;
			}
			else if(oTMR_Elapsed(&pDev->Timer, NAU7802_CALS_TIMEOUT(pDev->Registers.SamplingRate), TICKBASE_SYSTICK)){
				pDev->IsTimeoutFail = 1;
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pDev->CalStep = 0;
	}

	return result;
}

/* NAU7802_Init
 * Input
 *   pI2C | HAL_I2C pointer
 *   VLDO | VLDO output voltage | Uint=1V
 */
oResult_t NAU7802_Init(NAU7802_t *pDev, I2C_HandleTypeDef *pI2C, NAU7802_VLDO_t VLDO)
{
	oResult_t result = RESULT_RUN;
	uint8_t Data = 0;

	if(pDev == NULL || pI2C == NULL){
		return RESULT_NULL;
	}

	if(!pDev->IsOpening && pDev->ProcessStep){
		pDev->ProcessStep = 0;
		pDev->IsOpening = 1;
	}

	switch(pDev->ProcessStep)
	{
		case 0:
			pDev->pI2C = pI2C;

			NAU7802_DeInit(pDev);
			pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
			pDev->ErrorCount = 0;
			pDev->ProcessStep++;
		case 1:
			if(oTMR_Trigger(&pDev->Timer, 10, 1, TICKBASE_SYSTICK)){
				if(pI2C->State != HAL_I2C_STATE_RESET){
					pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					pDev->ProcessStep++;
				}
				else{
					HAL_I2C_Init(pI2C);
				}
			}
			break;
		case 2:
			if(oTMR_Trigger(&pDev->Timer, 10, 1, TICKBASE_SYSTICK)){
				pDev->ProcessStep++;
			}
			else if(pDev->IsOpen){
				result = RESULT_OK;
			}
			break;
		case 3:
			if(NAU7802_ReadMemory(pDev, NAU7802_REG_CHIP_ID, &Data) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step3(ChipID) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 4:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_RR) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step4(RR set) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 5:
			if(NAU7802_ResetBit(pDev, NAU7802_REG_PUCTRL, 0, NAU7802_PUCTRL_RR) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step5(RR clear) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 6:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUD) == RESULT_OK){
				pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);	/* case7 PUR 대기 상한 기준점 */
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step6(PUD) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 7:
			switch(NAU7802_ReadBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUR))
			{
				default:	/* RESULT_NULL — PUR 미상승 */
					if(oTMR_Elapsed(&pDev->Timer, NAU7802_PUR_TIMEOUT, TICKBASE_SYSTICK)){
						oSerial_Log("NAU7802", "Init step7(PUR) TIMEOUT, abort");
						result = RESULT_ERROR;
					}
					break;
				case RESULT_OK:
					pDev->ProcessStep++;
					pDev->ErrorCount = 0;
					break;
				case RESULT_ERROR:
					if(pDev->ErrorCount++ > 3){
						oSerial_Log("NAU7802", "Init step7(PUR read) FAIL, abort");
						result = RESULT_ERROR;
					}
					break;
			}
			break;
		case 8:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_PUA|NAU7802_PUCTRL_CS|NAU7802_PUCTRL_AVDDS) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step8(PUA/CS/AVDDS) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 9:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_ADC_CTRL, 0x30) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step9(ADC_CTRL) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 10:
			if(NAU7802_WriteBit(pDev, NAU7802_REG_CTRL1, NAU7802_CTRL1_GAIN_X1) == RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step10(GAIN_X1) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		case 11:
			if(NAU7802_SetVLDO(pDev, VLDO) ==  RESULT_OK){
				pDev->ProcessStep++;
				pDev->ErrorCount = 0;
			}
			else if(pDev->ErrorCount++ > 3){
				oSerial_Log("NAU7802", "Init step11(VLDO) FAIL, abort");
				result = RESULT_ERROR;
			}
			break;
		default:
			result = RESULT_OK;
			pDev->IsOpen = 1;
			pDev->OpenTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			oSerial_Log("NAU7802", "Init OK VREF=%.0f Gain=0x%02X", pDev->VREF, (unsigned int)pDev->Registers.Gain);
			break;
	}

	if(result != RESULT_RUN){
		pDev->ProcessStep = 0;
		pDev->ErrorCount = 0;
		pDev->IsOpening = 0;
	}

	return result;
}

/* NAU7802_DeInit */
oResult_t NAU7802_DeInit(NAU7802_t *pDev)
{
	pDev->ProcessStep = 0;
	pDev->CalStep = 0;
	pDev->ConversionErrorCount = 0;
	pDev->FlushRemain = 0;
	pDev->IsTimeoutFail = 0;
	pDev->IsNeedCalibration = 0;
	pDev->ReadData = 0;
	pDev->AmpGain = 0;
	pDev->CurrentChannel = 0;
	pDev->OpenTimestamp = 0;

	pDev->Registers.Channel = 0;
	pDev->Registers.Gain = 0;
	pDev->Registers.VLDO = 0;
	pDev->Registers.SamplingRate = 0;

	if(pDev->pI2C == NULL || !pDev->IsOpen){
		return RESULT_NULL;
	}

	HAL_I2C_DeInit(pDev->pI2C);

	pDev->pI2C = NULL;
	pDev->IsOpen = 0;

	oSerial_Log("NAU7802", "DeInit OK");

	return RESULT_OK;
}

oResult_t NAU7802_IsDataReady(NAU7802_t *pDev)
{
	uint8_t Data = 0;

	if(!pDev->IsOpen){
		return RESULT_ERROR;
	}

	if(NAU7802_ReadMemory(pDev, NAU7802_REG_PUCTRL, &Data) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(Data & NAU7802_PUCTRL_CR){
		return RESULT_OK;
	}

	return RESULT_NULL;
}

oResult_t NAU7802_ConversionStop(NAU7802_t *pDev)
{
	return NAU7802_ResetBit(pDev, NAU7802_REG_PUCTRL, 0, NAU7802_PUCTRL_CS);
}

/* NAU7802_ADCRead
 * Input
 *   Channel 		| Channel number 		| 1 or 2
 *   SamplingRate 	| Sampling frequency 	| Uint=1Hz
 *   pADC 			| ADC output pointer 	| 24bit data
 */
oResult_t NAU7802_ADCRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, int32_t *pADC)
{
	oResult_t result = RESULT_RUN;
	uint8_t Data = 0;

	if(!pDev->IsOpen || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	if(!pDev->IsADConversion && pDev->ProcessStep){
		pDev->ProcessStep = 0;
		pDev->IsADConversion = 1;
	}

	/* 사유 플래그는 호출 단위로 초기화 — 세션 단위로 두면 흡수된 타임아웃 이후의 I2C 오류까지 TIMEOUT으로 표기된다 */
	pDev->IsTimeoutFail = 0;

	switch(pDev->ProcessStep)
	{
		case 0:
			if((result = NAU7802_SetSampling(pDev, Channel, SamplingRate)) == RESULT_OK){
				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			break;
		case 1:	/* 설정 변경 시 내부 오프셋 재교정 (채널별 OCAL) */
			if(!pDev->IsNeedCalibration){
				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			else{
				switch((result = NAU7802_Calibration(pDev)))
				{
					case RESULT_OK:
						pDev->IsNeedCalibration = 0;
						pDev->ProcessStep++;
						result = RESULT_RUN;
						break;
					default:
						result = RESULT_RUN;
						break;
					case RESULT_ERROR:
						break;
				}
			}
			break;
		case 2:
			if((result = NAU7802_WriteBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_CS)) == RESULT_OK){
				pDev->Timer = oTMR_GetTick(TICKBASE_SYSTICK);	/* case3 CR 대기 상한 기준점 */
				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			break;
		case 3:
			switch(NAU7802_ReadBit(pDev, NAU7802_REG_PUCTRL, NAU7802_PUCTRL_CR))
			{
				case RESULT_OK:
					pDev->ProcessStep++;
					break;
				default:	/* RESULT_NULL — 변환 미완료 */
					if(oTMR_Elapsed(&pDev->Timer, NAU7802_CR_TIMEOUT(pDev->Registers.SamplingRate), TICKBASE_SYSTICK)){
						pDev->IsTimeoutFail = 1;
						result = RESULT_ERROR;
					}
					else{
						result = RESULT_RUN;
					}
					break;
				case RESULT_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 4:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B2, &Data)) == RESULT_OK){
				pDev->ReadData = (uint32_t)Data;
				pDev->ReadData <<= 8;

				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			else{
				break;
			}
		case 5:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B1, &Data)) == RESULT_OK){
				pDev->ReadData |= (uint32_t)Data;
				pDev->ReadData <<= 8;

				pDev->ProcessStep++;
				result = RESULT_RUN;
			}
			else{
				break;
			}
		case 6:
			if((result = NAU7802_ReadMemory(pDev, NAU7802_REG_ADC_B0, &Data)) == RESULT_OK){
				pDev->ReadData |= (uint32_t)Data;
				pDev->ReadData &= 0x00FFFFFF;

				if(pDev->ReadData > (NAU7802_ADC_MAXVALUE/2)){
					pDev->ReadData -= NAU7802_ADC_MAXVALUE;
				}

				*pADC = pDev->ReadData;
			}
			break;
	}

	/* 단계 오류는 같은 step 재시도로 흡수 — 세션 누적 한도를 넘겨야 최종 ERROR (단발 I2C 실패로 세션이
	 * 죽으면 상위가 매번 풀 재Init에 들어가 복구가 오히려 느려진다) */
	if(result == RESULT_ERROR){
		if(++pDev->ConversionErrorCount < NAU7802_CONVERSION_ERROR_MAX){
			result = RESULT_RUN;
		}
		else{
			oSerial_Log("NAU7802", "ADCRead FAIL at step%d (Ch=%d, errCnt=%d, %s)", (int)pDev->ProcessStep, (int)Channel, (int)pDev->ConversionErrorCount, pDev->IsTimeoutFail ? "TIMEOUT" : "I2C");
		}
	}

	if(result != RESULT_RUN){
		pDev->ProcessStep = 0;
		pDev->IsADConversion = 0;
		pDev->ConversionErrorCount = 0;
	}

	return result;
}

/* NAU7802_AnalogRead
 * Input
 *   Channel 		| Channel number 					| 1 or 2
 *   pAnalog 		| Analog output pointer 			| Unit=mV
 */
oResult_t NAU7802_AnalogRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, double *pAnalog)
{
	int32_t ADC=0;
	oResult_t result = RESULT_RUN;

	if(!pDev->IsOpen || pDev->pI2C == NULL){
		return RESULT_ERROR;
	}

	/* 채널·설정 변경 후 정착 전 샘플 버림 — 호출당 1세션만 진행하고 RESULT_RUN 반환.
	 * 한 호출에서 전량을 돌면 세션 오류 한도·비트 대기 상한이 곱해져 메인 루프가 워치독 주기를 넘긴다. */
	if(pDev->FlushRemain == 0 && (pDev->CurrentChannel != Channel || pDev->IsNeedCalibration)){
		oSerial_Log("NAU7802", "AnalogRead channel switch %d->%d, flushing", (int)pDev->CurrentChannel, (int)Channel);
		pDev->FlushRemain = NAU7802_FLUSH_COUNT;
	}

	if(pDev->FlushRemain){
		if((result = NAU7802_ADCRead(pDev, Channel, SamplingRate, &ADC)) == RESULT_OK){
			pDev->FlushRemain--;
			result = RESULT_RUN;
		}

		return result;
	}

	if((result = NAU7802_ADCRead(pDev, Channel, SamplingRate, &ADC)) == RESULT_OK){
		*pAnalog = (double)(ADC) / (double)(NAU7802_ADC_MAXVALUE/2) * (pDev->VREF/2 / MATH_MAX(pDev->AmpGain, 1));

		if(Channel == 1){
			ADC = 0;
		}
	}

	return result;
}

oResult_t NAU7802_AnalogOverSampling(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, uint8_t SamplingCount, double *pAnalog)
{
	double ReadAnalog = 0;
	oResult_t result = RESULT_RUN;

	if(SamplingCount == 0 || pAnalog == NULL){
		pDev->OverSampling.ReadCount = 0;
		return RESULT_NULL;
	}

	if(!pDev->OverSampling.ReadCount){
		pDev->OverSampling.SumOfAnalog = 0;
	}

	switch(NAU7802_AnalogRead(pDev, Channel, SamplingRate, &ReadAnalog))
	{
		default:
			break;
		case RESULT_ERROR:
			if(++pDev->ErrorCount >= 3){
				oSerial_Log("NAU7802", "OverSampling Ch%d FAIL after 3 errors (n=%d/%d)", (int)Channel, (int)pDev->OverSampling.ReadCount, (int)SamplingCount);
				result = RESULT_ERROR;
			}
			break;
		case RESULT_OK:
			pDev->ErrorCount = 0;
			pDev->OverSampling.SumOfAnalog += ReadAnalog;
			pDev->OverSampling.ReadCount++;

			oSerial_Log("NAU7802", "OverSampling Ch%d Sample[%d/%d]=%.4f", (int)Channel, (int)pDev->OverSampling.ReadCount, (int)SamplingCount, ReadAnalog);

			if(pDev->OverSampling.ReadCount >= SamplingCount){
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		if(pDev->OverSampling.ReadCount > 0){
			*pAnalog = pDev->OverSampling.SumOfAnalog/(double)pDev->OverSampling.ReadCount;
		}

		if(result == RESULT_OK){
			oSerial_Log("NAU7802", "OverSampling Ch%d DONE avg=%.4f (n=%d)", (int)Channel, *pAnalog, (int)pDev->OverSampling.ReadCount);
		}

		pDev->OverSampling.ReadCount = 0;
	}

	return result;
}
#endif
