/*
 * EEPROM_24LC16B.c
 *
 *  Created on: Dec 24, 2024
 *      Author: JONE
 *  Reference : https://github.com/nimaltd/ee24/blob/master/ee24.c
 */

#include "ONE_Common.h"
#include "ONE_Signal.h"
#include "Mi_Main.h"
#include "EEPROM_24LC.h"

#define	EEPROM_24LC_WP(state)			IO_WRITE_LOWSIDE(IO_PROT(DO_EEPROM_WP_GPIO_Port,DO_EEPROM_WP_Pin), state)
#define	EEPROM_24LC_CHIP_READ(chip)		((chip<<1 & 0x0E) | 0xA1)
#define	EEPROM_24LC_CHIP_WRITE(chip)	((chip<<1 & 0x0E) | 0xA0)

oResult_t EEPROM_24LC_Read(I2C_HandleTypeDef *pHandle, uint16_t Index, uint8_t *pData, uint32_t Size)
{
	static uint8_t MemReadSqStep = 0;
	static uint32_t MemReadSqTimer = 0;
	static uint8_t *pMemReadBuffer = NULL;
	uint8_t DevAddress = EEPROM_24LC_CHIP_READ(0);
	oResult_t result = RESULT_RUN;

	switch(MemReadSqStep)
	{
		default:
			MemReadSqStep = 0;
		case 0:
			if(Index+Size <= EEPROM_24LC_MAX_BYTESIZE && Size > 0 && pData != NULL){
				pMemReadBuffer = malloc(Size);
				if(pMemReadBuffer == NULL){
					result = RESULT_ERROR;
					break;
				}
				memset(pMemReadBuffer, 0, Size);

				oTMR_ResetNow(&MemReadSqTimer);
				EEPROM_24LC_WP(0);
				MemReadSqStep++;
			}
			else{
				result = RESULT_ERROR;
				break;
			}
		case 1:
			if(oTMR_Elapsed(&MemReadSqTimer, 10)){
				MemReadSqStep++;
			}
			break;
		case 2:
			switch(HAL_I2C_Mem_Read(pHandle, DevAddress, Index, EEPROM_24LC_MEMADD_SIZE, pMemReadBuffer, Size, 65535))
			{
				case HAL_OK:
					memcpy(pData, pMemReadBuffer, Size);
					oTMR_ResetNow(&MemReadSqTimer);
					result = RESULT_OK;
					break;
				default:
				case HAL_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
	}

	if(result != RESULT_RUN){
		if(pMemReadBuffer!= NULL){
			free(pMemReadBuffer);
			pMemReadBuffer = NULL;
		}

		EEPROM_24LC_WP(0);
		MemReadSqStep = 0;
	}

	return result;
}

oResult_t EEPROM_24LC_Write(I2C_HandleTypeDef *pHandle, uint16_t Index, uint8_t *pData, uint32_t Size)
{
	static uint8_t MemWriteSqStep = 0;
	static uint32_t MemWriteSqTimer = 0;
	static uint32_t WriteCount = 0;

	uint16_t MemAddress;
	uint32_t RemainingSize = 0;
	uint16_t WriteSize = 0;
	uint16_t PageRemaining = 0;
	uint8_t DevAddress = EEPROM_24LC_CHIP_WRITE(0);
	oResult_t result = RESULT_RUN;

	switch(MemWriteSqStep)
	{
		default:
			MemWriteSqStep = 0;
		case 0:
			if(Index+Size <= EEPROM_24LC_MAX_BYTESIZE && Size > 0 && pData != NULL){
				WriteCount = 0;
				MemWriteSqStep++;
				EEPROM_24LC_WP(1);
				oTMR_ResetNow(&MemWriteSqTimer);
			}
			else{
				result = RESULT_ERROR;
				break;
			}
		case 1:
			if(oTMR_Elapsed(&MemWriteSqTimer, 10)){
				MemWriteSqStep++;
			}
			break;
		case 2:
			MemAddress = Index+WriteCount;
			RemainingSize = Size - WriteCount;
			PageRemaining = EEPROM_24LC_PAGE_BYTESIZE - (MemAddress % EEPROM_24LC_PAGE_BYTESIZE);
			WriteSize = (uint16_t)(RemainingSize > PageRemaining ? PageRemaining : RemainingSize);

			switch(HAL_I2C_Mem_Write(pHandle, DevAddress, (uint16_t)(MemAddress), EEPROM_24LC_MEMADD_SIZE, (uint8_t *)(pData+WriteCount), WriteSize, 2000))
			{
				case HAL_OK:
					MemWriteSqStep++;
					WriteCount += WriteSize;
					oTMR_ResetNow(&MemWriteSqTimer);
					break;
				default:
				case HAL_ERROR:
					result = RESULT_ERROR;
					break;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MemWriteSqTimer, 10)){
				if(WriteCount < Size){
					MemWriteSqStep--;
				}
				else{
					result = RESULT_OK;
				}
			}
			break;
	}

	if(result != RESULT_RUN){
		EEPROM_24LC_WP(0);
		MemWriteSqStep = 0;
	}

	return result;
}
