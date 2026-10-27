/*
 * MEMS_ICM42688P.c
 *
 *  Created on: Apr 10, 2026
 *      Author: JONE
 *  Version: 0.2 (2026-10-20)
 */

#include "ONE_Common.h"
#include "Mi_Main.h"
#include "MEMS_ICM42688P.h"

/* Register addresses (Bank 0) */
#define ICM42688P_REG_DEVICE_CONFIG		0x11
#define ICM42688P_REG_INT_CONFIG		0x14
#define ICM42688P_REG_TEMP_DATA1		0x1D
#define ICM42688P_REG_ACCEL_DATA_X1		0x1F
#define ICM42688P_REG_GYRO_DATA_X1		0x25
#define ICM42688P_REG_INT_STATUS		0x2D
#define ICM42688P_REG_PWR_MGMT0		0x4E
#define ICM42688P_REG_GYRO_CONFIG0		0x4F
#define ICM42688P_REG_ACCEL_CONFIG0		0x50
#define ICM42688P_REG_GYRO_ACCEL_CONFIG0	0x52
#define ICM42688P_REG_WHO_AM_I			0x75
#define ICM42688P_REG_BANK_SEL			0x76

/* SPI Read/Write bit */
#define ICM42688P_SPI_READ				0x80
#define ICM42688P_SPI_WRITE				0x00

/* Soft reset value */
#define ICM42688P_SOFT_RESET			0x01

/* ============================================================
 * SPI Low-Level Read/Write
 * ============================================================ */
static oResult_t ICM42688P_WriteReg(ICM42688P_t *pICM, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pICM->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | ICM42688P_SPI_WRITE;

	if(pICM->CS.Port != NULL){
		HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, !pICM->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pICM->pSPI, &addr, 1, HAL_MAX_DELAY) != HAL_OK){
		if(pICM->CS.Port != NULL){
			HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(HAL_SPI_Transmit(pICM->pSPI, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		if(pICM->CS.Port != NULL){
			HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pICM->CS.Port != NULL){
		HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t ICM42688P_ReadReg(ICM42688P_t *pICM, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pICM->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | ICM42688P_SPI_READ;

	if(pICM->CS.Port != NULL){
		HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, !pICM->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pICM->pSPI, &addr, 1, HAL_MAX_DELAY) != HAL_OK){
		if(pICM->CS.Port != NULL){
			HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(HAL_SPI_Receive(pICM->pSPI, pData, SizeOfData, HAL_MAX_DELAY) != HAL_OK){
		if(pICM->CS.Port != NULL){
			HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pICM->CS.Port != NULL){
		HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t ICM42688P_WriteRegByte(ICM42688P_t *pICM, uint8_t RegAddress, uint8_t Data)
{
	return ICM42688P_WriteReg(pICM, RegAddress, &Data, 1);
}

static oResult_t ICM42688P_ReadRegByte(ICM42688P_t *pICM, uint8_t RegAddress, uint8_t *pData)
{
	return ICM42688P_ReadReg(pICM, RegAddress, pData, 1);
}

/* ============================================================
 * Status / Config / Data
 * ============================================================ */
oResult_t ICM42688P_GetStatus(ICM42688P_t *pICM)
{
	return ICM42688P_ReadRegByte(pICM, ICM42688P_REG_INT_STATUS, &pICM->Register.INT_STATUS.Byte);
}

oResult_t ICM42688P_SetConfig(ICM42688P_t *pICM)
{
	oResult_t result;

	/* Config registers BEFORE power enable (per original project sequence) */
	result = ICM42688P_WriteRegByte(pICM, ICM42688P_REG_ACCEL_CONFIG0, pICM->Register.ACCEL_CONFIG0.Byte);
	if(result != RESULT_OK) return result;

	result = ICM42688P_WriteRegByte(pICM, ICM42688P_REG_GYRO_CONFIG0, pICM->Register.GYRO_CONFIG0.Byte);
	if(result != RESULT_OK) return result;

	result = ICM42688P_WriteRegByte(pICM, ICM42688P_REG_GYRO_ACCEL_CONFIG0, pICM->Register.GYRO_ACCEL_CONFIG0.Byte);
	if(result != RESULT_OK) return result;

	/* Power ON last */
	result = ICM42688P_WriteRegByte(pICM, ICM42688P_REG_PWR_MGMT0, pICM->Register.PWR_MGMT0.Byte);

	return result;
}

oResult_t ICM42688P_GetData(ICM42688P_t *pICM)
{
	oResult_t result = RESULT_RUN;
	uint8_t rawData[14];

	if(!pICM->State.IsRun){
		return RESULT_ERROR;
	}

	/* Read unconditionally (original does not gate on DATA_RDY) */
	if((result = ICM42688P_ReadReg(pICM, ICM42688P_REG_TEMP_DATA1, rawData, 14)) == RESULT_OK){
		/* Big-endian: MSB first */
		pICM->Register.Data.TEMP    = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[0],  rawData[1]);
		pICM->Register.Data.ACCEL_X = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[2],  rawData[3]);
		pICM->Register.Data.ACCEL_Y = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[4],  rawData[5]);
		pICM->Register.Data.ACCEL_Z = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[6],  rawData[7]);
		pICM->Register.Data.GYRO_X  = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[8],  rawData[9]);
		pICM->Register.Data.GYRO_Y  = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[10], rawData[11]);
		pICM->Register.Data.GYRO_Z  = ICM42688P_DATACONVERT_TO_ACC_16BIT(rawData[12], rawData[13]);

		/* Temperature */
		pICM->Temperature = ICM42688P_TEMP_TO_Degree(pICM->Register.Data.TEMP);

		/* Acceleration (uG) */
		switch(pICM->Register.ACCEL_CONFIG0.ACCEL_FS_SEL)
		{
			case ICM42688P_ACCEL_FS_2G:
				pICM->Acceleration.X = ICM42688P_ACC_TO_2G_uG(pICM->Register.Data.ACCEL_X);
				pICM->Acceleration.Y = ICM42688P_ACC_TO_2G_uG(pICM->Register.Data.ACCEL_Y);
				pICM->Acceleration.Z = ICM42688P_ACC_TO_2G_uG(pICM->Register.Data.ACCEL_Z);
				break;
			case ICM42688P_ACCEL_FS_4G:
				pICM->Acceleration.X = ICM42688P_ACC_TO_4G_uG(pICM->Register.Data.ACCEL_X);
				pICM->Acceleration.Y = ICM42688P_ACC_TO_4G_uG(pICM->Register.Data.ACCEL_Y);
				pICM->Acceleration.Z = ICM42688P_ACC_TO_4G_uG(pICM->Register.Data.ACCEL_Z);
				break;
			case ICM42688P_ACCEL_FS_8G:
				pICM->Acceleration.X = ICM42688P_ACC_TO_8G_uG(pICM->Register.Data.ACCEL_X);
				pICM->Acceleration.Y = ICM42688P_ACC_TO_8G_uG(pICM->Register.Data.ACCEL_Y);
				pICM->Acceleration.Z = ICM42688P_ACC_TO_8G_uG(pICM->Register.Data.ACCEL_Z);
				break;
			default:
			case ICM42688P_ACCEL_FS_16G:
				pICM->Acceleration.X = ICM42688P_ACC_TO_16G_uG(pICM->Register.Data.ACCEL_X);
				pICM->Acceleration.Y = ICM42688P_ACC_TO_16G_uG(pICM->Register.Data.ACCEL_Y);
				pICM->Acceleration.Z = ICM42688P_ACC_TO_16G_uG(pICM->Register.Data.ACCEL_Z);
				break;
		}

		/* Gyroscope (mdps) */
		switch(pICM->Register.GYRO_CONFIG0.GYRO_FS_SEL)
		{
			case ICM42688P_GYRO_FS_250DPS:
				pICM->GyroRate.X = ICM42688P_GYRO_TO_250DPS(pICM->Register.Data.GYRO_X);
				pICM->GyroRate.Y = ICM42688P_GYRO_TO_250DPS(pICM->Register.Data.GYRO_Y);
				pICM->GyroRate.Z = ICM42688P_GYRO_TO_250DPS(pICM->Register.Data.GYRO_Z);
				break;
			case ICM42688P_GYRO_FS_500DPS:
				pICM->GyroRate.X = ICM42688P_GYRO_TO_500DPS(pICM->Register.Data.GYRO_X);
				pICM->GyroRate.Y = ICM42688P_GYRO_TO_500DPS(pICM->Register.Data.GYRO_Y);
				pICM->GyroRate.Z = ICM42688P_GYRO_TO_500DPS(pICM->Register.Data.GYRO_Z);
				break;
			case ICM42688P_GYRO_FS_1000DPS:
				pICM->GyroRate.X = ICM42688P_GYRO_TO_1000DPS(pICM->Register.Data.GYRO_X);
				pICM->GyroRate.Y = ICM42688P_GYRO_TO_1000DPS(pICM->Register.Data.GYRO_Y);
				pICM->GyroRate.Z = ICM42688P_GYRO_TO_1000DPS(pICM->Register.Data.GYRO_Z);
				break;
			default:
			case ICM42688P_GYRO_FS_2000DPS:
				pICM->GyroRate.X = ICM42688P_GYRO_TO_2000DPS(pICM->Register.Data.GYRO_X);
				pICM->GyroRate.Y = ICM42688P_GYRO_TO_2000DPS(pICM->Register.Data.GYRO_Y);
				pICM->GyroRate.Z = ICM42688P_GYRO_TO_2000DPS(pICM->Register.Data.GYRO_Z);
				break;
		}
	}

	return result;
}

/* ============================================================
 * GetAngle - Oversampling + Angle Calculation
 * ============================================================ */
oResult_t ICM42688P_GetRPY(ICM42688P_t *pICM, uint8_t Count, oRPY_t *pAngle)
{
	oResult_t result = RESULT_RUN;
	oRPY_t RPY;

	if(pICM == NULL || Count <= 0){
		return RESULT_NULL;
	}

	switch(ICM42688P_GetData(pICM))
	{
		default:
			break;
		case RESULT_ERROR:
			pICM->State.CountOfError++;
			break;
		case RESULT_OK:
			if(pICM->State.CountOfOverSampling == 0){
				memset(&pICM->OverSampling, 0, sizeof(pICM->OverSampling));
			}

			RPY = oMath_AccelationToRPY(pICM->Acceleration);

			pICM->OverSampling.Pitch += RPY.Pitch;
			pICM->OverSampling.Roll += RPY.Roll;
			pICM->State.CountOfOverSampling++;
			break;
	}

	if(pICM->State.CountOfError >= Count){
		result = RESULT_ERROR;
	}
	else if(pICM->State.CountOfOverSampling >= Count){
		pICM->Angle.Pitch = pICM->OverSampling.Pitch / (double)pICM->State.CountOfOverSampling;
		pICM->Angle.Roll = pICM->OverSampling.Roll / (double)pICM->State.CountOfOverSampling;

		*pAngle = pICM->Angle;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		pICM->State.CountOfOverSampling = 0;
		pICM->State.CountOfError = 0;
	}

	return result;
}

/* ============================================================
 * Open - Initialize ICM42688P via SPI
 * ============================================================ */
oResult_t ICM42688P_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, ICM42688P_t *pICM)
{
	oResult_t result = RESULT_RUN;

	if(pSPI == NULL || pICM == NULL){
		return RESULT_ERROR;
	}

	if(pICM->State.IsRun){
		return RESULT_OK;
	}
	else if(!pICM->State.IsBusy && pICM->State.SequenceStep != 0){
		memset(&pICM->State, 0, sizeof(pICM->State));

		pICM->State.SequenceStep = 0;
		pICM->State.IsBusy = 1;
		pICM->State.IsRun = 0;
	}

	switch(pICM->State.SequenceStep)
	{
		case 0:
			pICM->State.IsOpen = 0;

			/* Store SPI handle and CS pin */
			pICM->pSPI = pSPI;
			pICM->CS = CS;

			/* Init SPI if not running */
			if(pSPI->State == HAL_SPI_STATE_RESET){
				if(HAL_SPI_Init(pSPI) != HAL_OK){
					result = RESULT_ERROR;
					break;
				}
			}

			/* CS idle high */
			if(pICM->CS.Port != NULL){
				HAL_GPIO_WritePin(pICM->CS.Port, pICM->CS.Pin, pICM->CS.ActiveLevel);
			}
			pICM->State.IsOpen = 1;
			pICM->State.SequenceStep++;
			break;

		case 1:
			/* Soft reset */
			if(ICM42688P_WriteRegByte(pICM, ICM42688P_REG_DEVICE_CONFIG, ICM42688P_SOFT_RESET) == RESULT_OK){
				HAL_Delay(2);
				pICM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 2:
			/* Clear reset bit */
			if(ICM42688P_WriteRegByte(pICM, ICM42688P_REG_DEVICE_CONFIG, 0x00) == RESULT_OK){
				pICM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 3:
			/* Read WHO_AM_I (read only, no value check) */
			if(ICM42688P_ReadRegByte(pICM, ICM42688P_REG_WHO_AM_I, &pICM->Register.WHO_AM_I) == RESULT_OK){
				pICM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 4:
			/* Set default config: Accel +/-2g 1kHz, Gyro OFF, Low Noise mode */
			pICM->Register.ACCEL_CONFIG0.ACCEL_FS_SEL = ICM42688P_ACCEL_FS_2G;
			pICM->Register.ACCEL_CONFIG0.ACCEL_ODR = ICM42688P_ACCEL_ODR_1KHZ;

			pICM->Register.GYRO_CONFIG0.GYRO_FS_SEL = ICM42688P_GYRO_FS_2000DPS;
			pICM->Register.GYRO_CONFIG0.GYRO_ODR = ICM42688P_GYRO_ODR_1KHZ;

			pICM->Register.GYRO_ACCEL_CONFIG0.ACCEL_UI_FILT_BW = ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_2;
			pICM->Register.GYRO_ACCEL_CONFIG0.GYRO_UI_FILT_BW = ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_2;

			pICM->Register.PWR_MGMT0.ACCEL_MODE = ICM42688P_ACCEL_MODE_LN;
			pICM->Register.PWR_MGMT0.GYRO_MODE = ICM42688P_GYRO_MODE_OFF;
			pICM->Register.PWR_MGMT0.TEMP_DIS = 0;
			pICM->Register.PWR_MGMT0.GYRO_IDLE = 0;

			pICM->DataRate = 1000.0;
			pICM->State.SequenceStep++;
			break;

		case 5:
			/* Apply config */
			if(ICM42688P_SetConfig(pICM) == RESULT_OK){
				HAL_Delay(5);
				pICM->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 6:
			/* Verify sensor ready (original uses FIFO_THS, not DATA_RDY) */
			if(ICM42688P_GetStatus(pICM) == RESULT_OK){
				if(pICM->Register.INT_STATUS.FIFO_THS){
					result = RESULT_OK;
					pICM->State.IsRun = 1;
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
		pICM->State.SequenceStep = 0;
		pICM->State.IsBusy = 0;
		if(result == RESULT_ERROR){
			pICM->State.IsOpen = 0;
			pICM->State.IsRun = 0;
		}
	}

	return result;
}

/* ============================================================
 * Close - Put ICM42688P to standby
 * ============================================================ */
oResult_t ICM42688P_Close(ICM42688P_t *pICM)
{
	oResult_t result = RESULT_RUN;

	if(!pICM->State.IsOpen){
		return RESULT_OK;
	}

	switch(pICM->State.SequenceStep)
	{
		case 0:
			pICM->State.IsBusy = 1;

			/* Turn off accel and gyro */
			pICM->Register.PWR_MGMT0.ACCEL_MODE = ICM42688P_ACCEL_MODE_OFF;
			pICM->Register.PWR_MGMT0.GYRO_MODE = ICM42688P_GYRO_MODE_OFF;
			pICM->Register.PWR_MGMT0.TEMP_DIS = 1;
			pICM->State.SequenceStep++;
			/* fall through */
		case 1:
			if(ICM42688P_WriteRegByte(pICM, ICM42688P_REG_PWR_MGMT0, pICM->Register.PWR_MGMT0.Byte) == RESULT_OK){
				pICM->State.SequenceStep++;
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
		memset(&pICM->State, 0, sizeof(pICM->State));
	}

	return result;
}
