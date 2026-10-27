/*
 * MEMS_IIM42351.c
 *
 *  Created on: Apr 10, 2026
 *      Author: JONE
 *  Version: 0.2 (2026-10-20)
 *
 * IIM-42351: 3-Axis Accelerometer (No Gyroscope)
 * Register-compatible with ICM-42688-P (accel subset)
 */

#include "ONE_Common.h"
#include "Mi_Main.h"
#include "MEMS_IIM42351.h"

/* Register Addresses (Bank 0) */
#define IIM42351_REG_DEVICE_CONFIG			0x11
#define IIM42351_REG_INT_CONFIG				0x14
#define IIM42351_REG_TEMP_DATA1				0x1D
#define IIM42351_REG_ACCEL_DATA_X1			0x1F
#define IIM42351_REG_INT_STATUS				0x2D
#define IIM42351_REG_PWR_MGMT0				0x4E
#define IIM42351_REG_ACCEL_CONFIG0			0x50
#define IIM42351_REG_GYRO_ACCEL_CONFIG0		0x52
#define IIM42351_REG_WHO_AM_I				0x75
#define IIM42351_REG_BANK_SEL				0x76

/* SPI Read/Write bit */
#define IIM42351_SPI_READ					0x80
#define IIM42351_SPI_WRITE					0x00

/* Soft reset value */
#define IIM42351_SOFT_RESET					0x01

/* ============================================================
 * SPI Low-Level Read/Write
 * ============================================================ */
static oResult_t IIM42351_WriteReg(IIM42351_t *pIIM, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pIIM->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIM42351_SPI_WRITE;

	if(pIIM->CS.Port != NULL){
		HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, !pIIM->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pIIM->pSPI, &addr, 1, HAL_MAX_DELAY) != HAL_OK){
		if(pIIM->CS.Port != NULL){
			HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(HAL_SPI_Transmit(pIIM->pSPI, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		if(pIIM->CS.Port != NULL){
			HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pIIM->CS.Port != NULL){
		HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t IIM42351_ReadReg(IIM42351_t *pIIM, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pIIM->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIM42351_SPI_READ;

	if(pIIM->CS.Port != NULL){
		HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, !pIIM->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pIIM->pSPI, &addr, 1, HAL_MAX_DELAY) != HAL_OK){
		if(pIIM->CS.Port != NULL){
			HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(HAL_SPI_Receive(pIIM->pSPI, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		if(pIIM->CS.Port != NULL){
			HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pIIM->CS.Port != NULL){
		HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t IIM42351_WriteRegByte(IIM42351_t *pIIM, uint8_t RegAddress, uint8_t Data)
{
	return IIM42351_WriteReg(pIIM, RegAddress, &Data, 1);
}

static oResult_t IIM42351_ReadRegByte(IIM42351_t *pIIM, uint8_t RegAddress, uint8_t *pData)
{
	return IIM42351_ReadReg(pIIM, RegAddress, pData, 1);
}

/* ============================================================
 * Status / Config / Data
 * ============================================================ */
oResult_t IIM42351_GetStatus(IIM42351_t *pIIM)
{
	return IIM42351_ReadRegByte(pIIM, IIM42351_REG_INT_STATUS, &pIIM->Register.INT_STATUS.Byte);
}

oResult_t IIM42351_SetConfig(IIM42351_t *pIIM)
{
	oResult_t result;

	/* Config registers BEFORE power enable (per original project sequence) */
	result = IIM42351_WriteRegByte(pIIM, IIM42351_REG_ACCEL_CONFIG0, pIIM->Register.ACCEL_CONFIG0.Byte);
	if(result != RESULT_OK) return result;

	result = IIM42351_WriteRegByte(pIIM, IIM42351_REG_GYRO_ACCEL_CONFIG0, pIIM->Register.GYRO_ACCEL_CONFIG0.Byte);
	if(result != RESULT_OK) return result;

	/* Power ON last */
	result = IIM42351_WriteRegByte(pIIM, IIM42351_REG_PWR_MGMT0, pIIM->Register.PWR_MGMT0.Byte);

	return result;
}

oResult_t IIM42351_GetData(IIM42351_t *pIIM)
{
	oResult_t result = RESULT_RUN;
	uint8_t rawData[8];

	if(!pIIM->State.IsRun){
		return RESULT_ERROR;
	}

	/* Read unconditionally (original does not gate on DATA_RDY) */
	if((result = IIM42351_ReadReg(pIIM, IIM42351_REG_TEMP_DATA1, rawData, 8)) == RESULT_OK){
		/* Big-endian: MSB first */
		pIIM->Register.Data.TEMP    = IIM42351_DATACONVERT_TO_ACC_16BIT(rawData[0], rawData[1]);
		pIIM->Register.Data.ACCEL_X = IIM42351_DATACONVERT_TO_ACC_16BIT(rawData[2], rawData[3]);
		pIIM->Register.Data.ACCEL_Y = IIM42351_DATACONVERT_TO_ACC_16BIT(rawData[4], rawData[5]);
		pIIM->Register.Data.ACCEL_Z = IIM42351_DATACONVERT_TO_ACC_16BIT(rawData[6], rawData[7]);

		/* Temperature */
		pIIM->Temperature = IIM42351_TEMP_TO_Degree(pIIM->Register.Data.TEMP);

		/* Acceleration (uG) */
		switch(pIIM->Register.ACCEL_CONFIG0.ACCEL_FS_SEL)
		{
			case IIM42351_ACCEL_FS_2G:
				pIIM->Acceleration.X = IIM42351_ACC_TO_2G_uG(pIIM->Register.Data.ACCEL_X);
				pIIM->Acceleration.Y = IIM42351_ACC_TO_2G_uG(pIIM->Register.Data.ACCEL_Y);
				pIIM->Acceleration.Z = IIM42351_ACC_TO_2G_uG(pIIM->Register.Data.ACCEL_Z);
				break;
			case IIM42351_ACCEL_FS_4G:
				pIIM->Acceleration.X = IIM42351_ACC_TO_4G_uG(pIIM->Register.Data.ACCEL_X);
				pIIM->Acceleration.Y = IIM42351_ACC_TO_4G_uG(pIIM->Register.Data.ACCEL_Y);
				pIIM->Acceleration.Z = IIM42351_ACC_TO_4G_uG(pIIM->Register.Data.ACCEL_Z);
				break;
			case IIM42351_ACCEL_FS_8G:
				pIIM->Acceleration.X = IIM42351_ACC_TO_8G_uG(pIIM->Register.Data.ACCEL_X);
				pIIM->Acceleration.Y = IIM42351_ACC_TO_8G_uG(pIIM->Register.Data.ACCEL_Y);
				pIIM->Acceleration.Z = IIM42351_ACC_TO_8G_uG(pIIM->Register.Data.ACCEL_Z);
				break;
			default:
			case IIM42351_ACCEL_FS_16G:
				pIIM->Acceleration.X = IIM42351_ACC_TO_16G_uG(pIIM->Register.Data.ACCEL_X);
				pIIM->Acceleration.Y = IIM42351_ACC_TO_16G_uG(pIIM->Register.Data.ACCEL_Y);
				pIIM->Acceleration.Z = IIM42351_ACC_TO_16G_uG(pIIM->Register.Data.ACCEL_Z);
				break;
		}
	}

	return result;
}

/* ============================================================
 * GetAngle - Oversampling + Angle Calculation
 * ============================================================ */
oResult_t IIM42351_GetRPY(IIM42351_t *pIIM, uint8_t Count, oRPY_t *pAngle)
{
	oResult_t result = RESULT_RUN;
	oRPY_t RPY;

	if(pIIM == NULL || Count <= 0){
		return RESULT_NULL;
	}

	switch(IIM42351_GetData(pIIM))
	{
		default:
			break;
		case RESULT_ERROR:
			pIIM->State.CountOfError++;
			break;
		case RESULT_OK:
			if(pIIM->State.CountOfOverSampling == 0){
				memset(&pIIM->OverSampling, 0, sizeof(pIIM->OverSampling));
			}

			RPY = oMath_AccelationToRPY(pIIM->Acceleration);

			pIIM->OverSampling.Pitch += RPY.Pitch;
			pIIM->OverSampling.Roll += RPY.Roll;
			pIIM->State.CountOfOverSampling++;
			break;
	}

	if(pIIM->State.CountOfError >= Count){
		result = RESULT_ERROR;
	}
	else if(pIIM->State.CountOfOverSampling >= Count){
		pIIM->Angle.Pitch = pIIM->OverSampling.Pitch / (double)pIIM->State.CountOfOverSampling;
		pIIM->Angle.Roll = pIIM->OverSampling.Roll / (double)pIIM->State.CountOfOverSampling;

		*pAngle = pIIM->Angle;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		pIIM->State.CountOfOverSampling = 0;
		pIIM->State.CountOfError = 0;
	}

	return result;
}

/* ============================================================
 * Open - Initialize IIM42351 via SPI
 * ============================================================ */
oResult_t IIM42351_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIM42351_t *pIIM)
{
	oResult_t result = RESULT_RUN;

	if(pSPI == NULL || pIIM == NULL){
		return RESULT_ERROR;
	}

	if(pIIM->State.IsRun){
		return RESULT_OK;
	}
	else if(!pIIM->State.IsBusy && pIIM->State.SequenceStep != 0){
		memset(&pIIM->State, 0, sizeof(pIIM->State));

		pIIM->State.SequenceStep = 0;
		pIIM->State.IsBusy = 1;
		pIIM->State.IsRun = 0;
	}

	switch(pIIM->State.SequenceStep)
	{
		case 0:
			pIIM->State.IsOpen = 0;

			/* Store SPI handle and CS pin */
			pIIM->pSPI = pSPI;
			pIIM->CS = CS;

			/* Init SPI if not running */
			if(pSPI->State == HAL_SPI_STATE_RESET){
				if(HAL_SPI_Init(pSPI) != HAL_OK){
					result = RESULT_ERROR;
					break;
				}
			}

			/* CS idle high */
			if(pIIM->CS.Port != NULL){
				HAL_GPIO_WritePin(pIIM->CS.Port, pIIM->CS.Pin, pIIM->CS.ActiveLevel);
			}
			pIIM->State.IsOpen = 1;
			pIIM->State.SequenceStep++;
			break;

		case 1:
			/* Soft reset */
			if(IIM42351_WriteRegByte(pIIM, IIM42351_REG_DEVICE_CONFIG, IIM42351_SOFT_RESET) == RESULT_OK){
				HAL_Delay(2);
				pIIM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 2:
			/* Clear reset bit */
			if(IIM42351_WriteRegByte(pIIM, IIM42351_REG_DEVICE_CONFIG, 0x00) == RESULT_OK){
				pIIM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 3:
			/* Read WHO_AM_I (read only, no value check) */
			if(IIM42351_ReadRegByte(pIIM, IIM42351_REG_WHO_AM_I, &pIIM->Register.WHO_AM_I) == RESULT_OK){
				pIIM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 4:
			/* Set default config: Accel +/-2g 1kHz, Low Noise mode */
			pIIM->Register.ACCEL_CONFIG0.ACCEL_FS_SEL = IIM42351_ACCEL_FS_2G;
			pIIM->Register.ACCEL_CONFIG0.ACCEL_ODR = IIM42351_ACCEL_ODR_1KHZ;

			pIIM->Register.GYRO_ACCEL_CONFIG0.ACCEL_UI_FILT_BW = IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_2;

			pIIM->Register.PWR_MGMT0.ACCEL_MODE = IIM42351_ACCEL_MODE_LN;
			pIIM->Register.PWR_MGMT0.TEMP_DIS = 0;

			pIIM->DataRate = 1000.0;
			pIIM->State.SequenceStep++;
			break;

		case 5:
			/* Apply config */
			if(IIM42351_SetConfig(pIIM) == RESULT_OK){
				HAL_Delay(5);
				pIIM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 6:
			/* Verify sensor ready (original uses FIFO_THS, not DATA_RDY) */
			if(IIM42351_GetStatus(pIIM) == RESULT_OK){
				if(pIIM->Register.INT_STATUS.FIFO_THS){
					result = RESULT_OK;
					pIIM->State.IsRun = 1;
				}
				else{
					result = RESULT_ERROR;
				}
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	if(result != RESULT_RUN){
		pIIM->State.SequenceStep = 0;
		pIIM->State.IsBusy = 0;
		if(result == RESULT_ERROR){
			pIIM->State.IsOpen = 0;
			pIIM->State.IsRun = 0;
		}
	}

	return result;
}

/* ============================================================
 * Close - Put IIM42351 to standby
 * ============================================================ */
oResult_t IIM42351_Close(IIM42351_t *pIIM)
{
	oResult_t result = RESULT_RUN;

	if(!pIIM->State.IsOpen){
		return RESULT_OK;
	}

	switch(pIIM->State.SequenceStep)
	{
		case 0:
			pIIM->State.IsBusy = 1;

			/* Turn off accel */
			pIIM->Register.PWR_MGMT0.ACCEL_MODE = IIM42351_ACCEL_MODE_OFF;
			pIIM->Register.PWR_MGMT0.TEMP_DIS = 1;
			pIIM->State.SequenceStep++;
			/* fall through */
		case 1:
			if(IIM42351_WriteRegByte(pIIM, IIM42351_REG_PWR_MGMT0, pIIM->Register.PWR_MGMT0.Byte) == RESULT_OK){
				pIIM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 2:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		memset(&pIIM->State, 0, sizeof(pIIM->State));
	}

	return result;
}
