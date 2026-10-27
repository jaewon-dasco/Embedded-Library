/*
 * MEMS_MMA8451.c
 *
 *  Version: 0.3 (2026-10-20)
 *
 * MMA8451: 3-Axis 14-bit Accelerometer (I2C)
 * Mirrors MEMS_IIM42351 so the two parts can be swapped by renaming the
 * prefix. Unlike IIM42351 the bus is I2C, so the chip select only picks
 * which sensor on the shared bus answers the transfer.
 */

#include "ONE_Common.h"
#include "Mi_Main.h"
#include "MEMS_MMA8451.h"

/* ============================================================
 * I2C Low-Level Read/Write
 * ============================================================ */
static oResult_t MMA8451_WriteReg(MMA8451_t *pMMA, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	if(!pMMA->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pMMA->CS.Port != NULL){
		HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, !pMMA->CS.ActiveLevel);
	}
	if(HAL_I2C_Mem_Write(pMMA->pI2C, pMMA->DeviceAddress, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, MMA8451_I2C_TIMEOUT_MS) != HAL_OK){
		if(pMMA->CS.Port != NULL){
			HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, pMMA->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pMMA->CS.Port != NULL){
		HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, pMMA->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t MMA8451_ReadReg(MMA8451_t *pMMA, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	if(!pMMA->State.IsOpen){
		return RESULT_ERROR;
	}

	if(pMMA->CS.Port != NULL){
		HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, !pMMA->CS.ActiveLevel);
	}
	if(HAL_I2C_Mem_Read(pMMA->pI2C, pMMA->DeviceAddress, RegAddress, I2C_MEMADD_SIZE_8BIT, pData, SizeOfData, MMA8451_I2C_TIMEOUT_MS) != HAL_OK){
		if(pMMA->CS.Port != NULL){
			HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, pMMA->CS.ActiveLevel);
		}
		return RESULT_ERROR;
	}
	if(pMMA->CS.Port != NULL){
		HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, pMMA->CS.ActiveLevel);
	}

	return RESULT_OK;
}

static oResult_t MMA8451_WriteRegByte(MMA8451_t *pMMA, uint8_t RegAddress, uint8_t Data)
{
	return MMA8451_WriteReg(pMMA, RegAddress, &Data, 1);
}

static oResult_t MMA8451_ReadRegByte(MMA8451_t *pMMA, uint8_t RegAddress, uint8_t *pData)
{
	return MMA8451_ReadReg(pMMA, RegAddress, pData, 1);
}

/* ============================================================
 * Status / Config / Data
 * ============================================================ */
oResult_t MMA8451_GetStatus(MMA8451_t *pMMA)
{
	return MMA8451_ReadRegByte(pMMA, MMA8451_REG_STATUS, &pMMA->Register.STATUS.Byte);
}

oResult_t MMA8451_SetConfig(MMA8451_t *pMMA)
{
	oResult_t result;

	/* Range, cutoff, data rate and oversampling accept writes in STANDBY
	 * only, so the device is parked there before any of them is sent */
	result = MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG1, 0x00);
	if(result != RESULT_OK) return result;

	result = MMA8451_WriteRegByte(pMMA, MMA8451_REG_XYZ_DATA_CFG, pMMA->Register.XYZ_DATA_CFG.Byte);
	if(result != RESULT_OK) return result;

	result = MMA8451_WriteRegByte(pMMA, MMA8451_REG_HP_FILTER_CUTOFF, pMMA->Register.HP_FILTER_CUTOFF.Byte);
	if(result != RESULT_OK) return result;

	result = MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG2, pMMA->Register.CTRL_REG2.Byte);
	if(result != RESULT_OK) return result;

	/* CTRL_REG1 last: it carries ACTIVE, so sampling starts only once the
	 * settings above are already in place */
	result = MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG1, pMMA->Register.CTRL_REG1.Byte);

	pMMA->State.CountOfWarmUp = 0;

	return result;
}

oResult_t MMA8451_GetData(MMA8451_t *pMMA)
{
	oResult_t result = RESULT_RUN;
	uint8_t rawData[7];

	if(!pMMA->State.IsRun){
		return RESULT_ERROR;
	}

	/* Status and the three axes in one burst: auto-increment is cleared by
	 * a STOP condition, and an LSB register only returns valid data in the
	 * access directly after its own MSB */
	if((result = MMA8451_ReadReg(pMMA, MMA8451_REG_STATUS, rawData, 7)) == RESULT_OK){
		pMMA->Register.STATUS.Byte = rawData[0];

		if(!pMMA->Register.STATUS.ZYXDR){
			return RESULT_RUN;
		}

		/* Samples taken within the turn-on time of 1.1/ODR are imprecise */
		if(pMMA->State.CountOfWarmUp < MMA8451_WARMUP_SAMPLE_COUNT){
			pMMA->State.CountOfWarmUp++;
			return RESULT_RUN;
		}

		/* Big-endian: MSB first */
		pMMA->Register.Data.ACCEL_X = MMA8451_DATACONVERT_TO_ACC_14BIT(rawData[1], rawData[2]);
		pMMA->Register.Data.ACCEL_Y = MMA8451_DATACONVERT_TO_ACC_14BIT(rawData[3], rawData[4]);
		pMMA->Register.Data.ACCEL_Z = MMA8451_DATACONVERT_TO_ACC_14BIT(rawData[5], rawData[6]);

		/* Acceleration (uG) */
		switch(pMMA->Register.XYZ_DATA_CFG.ACCEL_FS_SEL)
		{
			case MMA8451_ACCEL_FS_4G:
				pMMA->Acceleration.X = MMA8451_ACC_TO_4G_uG(pMMA->Register.Data.ACCEL_X);
				pMMA->Acceleration.Y = MMA8451_ACC_TO_4G_uG(pMMA->Register.Data.ACCEL_Y);
				pMMA->Acceleration.Z = MMA8451_ACC_TO_4G_uG(pMMA->Register.Data.ACCEL_Z);
				break;
			case MMA8451_ACCEL_FS_8G:
				pMMA->Acceleration.X = MMA8451_ACC_TO_8G_uG(pMMA->Register.Data.ACCEL_X);
				pMMA->Acceleration.Y = MMA8451_ACC_TO_8G_uG(pMMA->Register.Data.ACCEL_Y);
				pMMA->Acceleration.Z = MMA8451_ACC_TO_8G_uG(pMMA->Register.Data.ACCEL_Z);
				break;
			default:
			case MMA8451_ACCEL_FS_2G:
				pMMA->Acceleration.X = MMA8451_ACC_TO_2G_uG(pMMA->Register.Data.ACCEL_X);
				pMMA->Acceleration.Y = MMA8451_ACC_TO_2G_uG(pMMA->Register.Data.ACCEL_Y);
				pMMA->Acceleration.Z = MMA8451_ACC_TO_2G_uG(pMMA->Register.Data.ACCEL_Z);
				break;
		}
	}

	return result;
}

/* ============================================================
 * GetAngle - Oversampling + Angle Calculation
 * ============================================================ */
oResult_t MMA8451_GetRPY(MMA8451_t *pMMA, uint8_t Count, oRPY_t *pAngle)
{
	oResult_t result = RESULT_RUN;
	oRPY_t RPY;

	if(pMMA == NULL || Count <= 0){
		return RESULT_NULL;
	}

	switch(MMA8451_GetData(pMMA))
	{
		default:
			break;
		case RESULT_ERROR:
			pMMA->State.CountOfError++;
			break;
		case RESULT_OK:
			if(pMMA->State.CountOfOverSampling == 0){
				memset(&pMMA->OverSampling, 0, sizeof(pMMA->OverSampling));
			}

			RPY = oMath_AccelationToRPY(pMMA->Acceleration);

			pMMA->OverSampling.Pitch += RPY.Pitch;
			pMMA->OverSampling.Roll += RPY.Roll;
			pMMA->State.CountOfOverSampling++;
			break;
	}

	if(pMMA->State.CountOfError >= Count){
		result = RESULT_ERROR;
	}
	else if(pMMA->State.CountOfOverSampling >= Count){
		pMMA->Angle.Pitch = pMMA->OverSampling.Pitch / (double)pMMA->State.CountOfOverSampling;
		pMMA->Angle.Roll = pMMA->OverSampling.Roll / (double)pMMA->State.CountOfOverSampling;

		*pAngle = pMMA->Angle;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		pMMA->State.CountOfOverSampling = 0;
		pMMA->State.CountOfError = 0;
	}

	return result;
}

/* ============================================================
 * Open - Initialize MMA8451 via I2C
 * ============================================================ */
oResult_t MMA8451_Open(I2C_HandleTypeDef *pI2C, oIO_t CS, MMA8451_t *pMMA)
{
	oResult_t result = RESULT_RUN;
	uint8_t sysmod;

	if(pI2C == NULL || pMMA == NULL){
		return RESULT_ERROR;
	}

	if(pMMA->State.IsRun){
		return RESULT_OK;
	}
	else if(!pMMA->State.IsBusy && pMMA->State.SequenceStep != 0){
		memset(&pMMA->State, 0, sizeof(pMMA->State));

		pMMA->State.SequenceStep = 0;
		pMMA->State.IsBusy = 1;
		pMMA->State.IsRun = 0;
	}

	switch(pMMA->State.SequenceStep)
	{
		case 0:
			pMMA->State.IsOpen = 0;

			/* Store I2C handle and CS pin */
			pMMA->pI2C = pI2C;
			pMMA->CS = CS;

			if(pMMA->DeviceAddress == 0){
				pMMA->DeviceAddress = MMA8451_I2C_ADDRESS_SA0_LOW;
			}

			/* Init I2C if not running */
			if(pI2C->State == HAL_I2C_STATE_RESET){
				if(HAL_I2C_Init(pI2C) != HAL_OK){
					result = RESULT_ERROR;
					break;
				}
			}

			/* CS idle */
			if(pMMA->CS.Port != NULL){
				HAL_GPIO_WritePin(pMMA->CS.Port, pMMA->CS.Pin, pMMA->CS.ActiveLevel);
			}
			pMMA->State.IsOpen = 1;
			pMMA->State.SequenceStep++;
			break;

		case 1:
			/* Soft reset. RST is one of the only two bits accepted outside
			 * STANDBY, so it can be issued whatever state the part is in */
			if(MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG2, MMA8451_SOFT_RESET) == RESULT_OK){
				HAL_Delay(2);
				pMMA->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 2:
			/* RST deasserts itself once the boot finishes; the register is
			 * written back to a known state before it is configured */
			if(MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG2, 0x00) == RESULT_OK){
				pMMA->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 3:
			/* Read WHO_AM_I (read only, no value check) */
			if(MMA8451_ReadRegByte(pMMA, MMA8451_REG_WHO_AM_I, &pMMA->Register.WHO_AM_I) == RESULT_OK){
				pMMA->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 4:
			/* Set default config: Accel +/-2g 50Hz, high resolution */
			pMMA->Register.XYZ_DATA_CFG.ACCEL_FS_SEL = MMA8451_ACCEL_FS_2G;
			pMMA->Register.XYZ_DATA_CFG.HPF_OUT = 0;

			pMMA->Register.HP_FILTER_CUTOFF.Byte = 0;

			pMMA->Register.CTRL_REG1.ACCEL_ODR = MMA8451_ACCEL_ODR_50HZ;
			pMMA->Register.CTRL_REG1.ASLP_RATE = MMA8451_SLEEP_ODR_50HZ;
			pMMA->Register.CTRL_REG1.LNOISE = 1;
			pMMA->Register.CTRL_REG1.F_READ = 0;
			pMMA->Register.CTRL_REG1.ACTIVE = 1;

			pMMA->Register.CTRL_REG2.MODS = MMA8451_ACCEL_MODE_HIGHRESOLUTION;
			pMMA->Register.CTRL_REG2.SMODS = MMA8451_ACCEL_MODE_HIGHRESOLUTION;
			pMMA->Register.CTRL_REG2.SLPE = 0;

			pMMA->DataRate = 50.0;
			pMMA->State.SequenceStep++;
			break;

		case 5:
			/* Apply config */
			if(MMA8451_SetConfig(pMMA) == RESULT_OK){
				HAL_Delay(5);
				pMMA->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;

		case 6:
			/* Verify sensor ready (the part must have left STANDBY) */
			if(MMA8451_ReadRegByte(pMMA, MMA8451_REG_SYSMOD, &sysmod) == RESULT_OK){
				if(sysmod != MMA8451_SYSMOD_STANDBY){
					result = RESULT_OK;
					pMMA->State.IsRun = 1;
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
		pMMA->State.SequenceStep = 0;
		pMMA->State.IsBusy = 0;
		if(result == RESULT_ERROR){
			pMMA->State.IsOpen = 0;
			pMMA->State.IsRun = 0;
		}
	}

	return result;
}

/* ============================================================
 * Close - Put MMA8451 to standby
 * ============================================================ */
oResult_t MMA8451_Close(MMA8451_t *pMMA)
{
	oResult_t result = RESULT_RUN;

	if(!pMMA->State.IsOpen){
		return RESULT_OK;
	}

	switch(pMMA->State.SequenceStep)
	{
		case 0:
			pMMA->State.IsBusy = 1;

			/* Turn off accel. The rest of the configuration is preserved by
			 * the device across the ACTIVE -> STANDBY transition */
			pMMA->Register.CTRL_REG1.ACTIVE = 0;
			pMMA->State.SequenceStep++;
			/* fall through */
		case 1:
			if(MMA8451_WriteRegByte(pMMA, MMA8451_REG_CTRL_REG1, pMMA->Register.CTRL_REG1.Byte) == RESULT_OK){
				pMMA->State.SequenceStep++;
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
		memset(&pMMA->State, 0, sizeof(pMMA->State));
	}

	return result;
}
