/*
 * MEMS_IIS3DWBTR.c
 *
 *  Version: 0.4 (2026-10-20)
 */

#include "ONE_Common.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"
#include "MEMS_IIS3DWBTR.h"

#define IIS3DWB_SPI_RECOVERY_THRESHOLD	5
#define IIS3DWB_SPI_TIMEOUT				100		/* ms — bounded timeout (watchdog 호환) */

/* ============================================================
 * Recovery — SPI 페리페럴 DeInit/Init 으로 stuck 복구
 * ============================================================ */
oResult_t IIS3DWB_Recovery(IIS3DWB_t *pDev)
{
	if(pDev == NULL || pDev->pSPI == NULL){
		return RESULT_NULL;
	}

	oSerial_Log("IIS3DW", "Recovery | SPI reinit (err=%u)", (unsigned int)pDev->State.CountOfSpiError);

	HAL_SPI_DeInit(pDev->pSPI);
	if(HAL_SPI_Init(pDev->pSPI) != HAL_OK){
		oSerial_Log("IIS3DW", "Recovery fail | HAL_SPI_Init");
		return RESULT_ERROR;
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

static void IIS3DWB_OnHalError(IIS3DWB_t *pDev)
{
	if(pDev->State.CountOfSpiError < 0xFF){
		pDev->State.CountOfSpiError++;
	}
	if(pDev->State.CountOfSpiError >= IIS3DWB_SPI_RECOVERY_THRESHOLD){
		IIS3DWB_Recovery(pDev);
	}
}

/* ============================================================
 * SPI Low-Level Read/Write
 * ============================================================ */
static oResult_t IIS3DWB_WriteReg(IIS3DWB_t *pDev, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pDev->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIS3DWB_SPI_WRITE;

	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, pDev->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pDev->pSPI, &addr, 1, IIS3DWB_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS3DWB_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(HAL_SPI_Transmit(pDev->pSPI, pData, SizeOfData, IIS3DWB_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS3DWB_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

static oResult_t IIS3DWB_ReadReg(IIS3DWB_t *pDev, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pDev->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIS3DWB_SPI_READ;

	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, pDev->CS.ActiveLevel);
	}

	if(HAL_SPI_Transmit(pDev->pSPI, &addr, 1, IIS3DWB_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS3DWB_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(HAL_SPI_Receive(pDev->pSPI, pData, SizeOfData, IIS3DWB_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS3DWB_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

static oResult_t IIS3DWB_WriteRegByte(IIS3DWB_t *pDev, uint8_t RegAddress, uint8_t Data)
{
	return IIS3DWB_WriteReg(pDev, RegAddress, &Data, 1);
}

static oResult_t IIS3DWB_ReadRegByte(IIS3DWB_t *pDev, uint8_t RegAddress, uint8_t *pData)
{
	return IIS3DWB_ReadReg(pDev, RegAddress, pData, 1);
}

/* ============================================================
 * Public API
 * ============================================================ */
oResult_t IIS3DWB_WhoAmI(IIS3DWB_t *pDev, uint8_t *pId)
{
	if(pDev == NULL || pId == NULL){
		return RESULT_NULL;
	}
	return IIS3DWB_ReadRegByte(pDev, IIS3DWB_REG_WHO_AM_I, pId);
}

oResult_t IIS3DWB_GetStatus(IIS3DWB_t *pDev)
{
	return IIS3DWB_ReadRegByte(pDev, IIS3DWB_REG_STATUS, &pDev->Register.STATUS.Byte);
}

oResult_t IIS3DWB_SoftReset(IIS3DWB_t *pDev)
{
	uint8_t val = 0;
	uint32_t timer;
	oResult_t result;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	pDev->Register.CTRL3_C.Byte = 0;
	pDev->Register.CTRL3_C.SW_RESET = 1;
	pDev->Register.CTRL3_C.IF_INC = 1;   /* auto-increment 유지 */

	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL3_C, pDev->Register.CTRL3_C.Byte)) != RESULT_OK){
		oSerial_Log("IIS3DW", "SoftReset fail | CTRL3_C write");
		return result;
	}

	/* SW_RESET 비트가 자동 클리어될 때까지 대기 (50 ms 한도) */
	timer = oTMR_GetTick(TICKBASE_SYSTICK);
	while(!oTMR_Elapsed(&timer, 50, TICKBASE_SYSTICK)){
		if(IIS3DWB_ReadRegByte(pDev, IIS3DWB_REG_CTRL3_C, &val) == RESULT_OK){
			if(!(val & 0x01)){
				pDev->Register.CTRL3_C.SW_RESET = 0;	/* 셰도우도 내려야 SetConfig 가 재리셋을 걸지 않는다 */
				oSerial_Log("IIS3DW", "SoftReset done");
				return RESULT_OK;
			}
		}
	}

	oSerial_Log("IIS3DW", "SoftReset timeout | CTRL3_C=0x%02X", (unsigned int)val);
	return RESULT_ERROR;
}

oResult_t IIS3DWB_SetConfig(IIS3DWB_t *pDev)
{
	oResult_t result;

	/* CTRL3_C: BDU + IF_INC 강제 활성화 (안정성) */
	pDev->Register.CTRL3_C.IF_INC = 1;
	pDev->Register.CTRL3_C.BDU = 1;
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL3_C, pDev->Register.CTRL3_C.Byte)) != RESULT_OK){
		oSerial_Log("IIS3DW", "SetConfig fail | CTRL3_C");
		return result;
	}

	/* CTRL4_C */
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL4_C, pDev->Register.CTRL4_C.Byte)) != RESULT_OK){
		oSerial_Log("IIS3DW", "SetConfig fail | CTRL4_C");
		return result;
	}

	/* CTRL8_XL: Accel HPF/LPF2 cutoff + mode — CTRL1_XL 보다 먼저 써서
	 * ODR 기동 직후의 첫 샘플부터 필터 설정이 적용되게 한다 */
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL8_XL, pDev->Register.CTRL8_XL.Byte)) != RESULT_OK){
		oSerial_Log("IIS3DW", "SetConfig fail | CTRL8_XL");
		return result;
	}

	/* CTRL1_XL: Accel ODR + FS (마지막 — 여기서 변환 시작) */
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL1_XL, pDev->Register.CTRL1_XL.Byte)) != RESULT_OK){
		oSerial_Log("IIS3DW", "SetConfig fail | CTRL1_XL");
		return result;
	}

	oSerial_Log("IIS3DW", "SetConfig okay | CTRL1_XL=0x%02X CTRL3_C=0x%02X CTRL8_XL=0x%02X",
	            (unsigned int)pDev->Register.CTRL1_XL.Byte,
	            (unsigned int)pDev->Register.CTRL3_C.Byte,
	            (unsigned int)pDev->Register.CTRL8_XL.Byte);
	return RESULT_OK;
}

oResult_t IIS3DWB_GetData(IIS3DWB_t *pDev)
{
	oResult_t result;
	uint8_t rawData[8];	/* TEMP(2) + 0x22~0x27 미사용 구간은 별도 read */

	if(!pDev->State.IsRun){
		return RESULT_ERROR;
	}

	/* 온도(0x20-21)와 가속도(0x28-2D)는 자이로 자리(0x22~0x27)를 사이에 두고 떨어져 있어
	 * 한 번의 auto-increment read 로 묶지 않는다 */
	if((result = IIS3DWB_ReadReg(pDev, IIS3DWB_REG_OUT_TEMP_L, rawData, 2)) != RESULT_OK){
		return result;
	}
	if((result = IIS3DWB_ReadReg(pDev, IIS3DWB_REG_OUTX_L_A, &rawData[2], 6)) != RESULT_OK){
		return result;
	}

	/* ST sensor: little-endian (LSB first) */
	pDev->Register.Data.TEMP    = IIS3DWB_DATACONVERT_16BIT(rawData[1], rawData[0]);
	pDev->Register.Data.ACCEL_X = IIS3DWB_DATACONVERT_16BIT(rawData[3], rawData[2]);
	pDev->Register.Data.ACCEL_Y = IIS3DWB_DATACONVERT_16BIT(rawData[5], rawData[4]);
	pDev->Register.Data.ACCEL_Z = IIS3DWB_DATACONVERT_16BIT(rawData[7], rawData[6]);

	pDev->Temperature = IIS3DWB_TEMP_TO_Degree(pDev->Register.Data.TEMP);

	/* Acceleration → µG — FS에 따라 LSB 환산 */
	switch(pDev->Register.CTRL1_XL.FS_XL)
	{
		default:
		case IIS3DWB_XL_FS_2G:
			pDev->Acceleration.X = IIS3DWB_ACC_TO_2G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS3DWB_ACC_TO_2G_uG(pDev->Register.Data.ACCEL_Y);
			pDev->Acceleration.Z = IIS3DWB_ACC_TO_2G_uG(pDev->Register.Data.ACCEL_Z);
			break;
		case IIS3DWB_XL_FS_4G:
			pDev->Acceleration.X = IIS3DWB_ACC_TO_4G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS3DWB_ACC_TO_4G_uG(pDev->Register.Data.ACCEL_Y);
			pDev->Acceleration.Z = IIS3DWB_ACC_TO_4G_uG(pDev->Register.Data.ACCEL_Z);
			break;
		case IIS3DWB_XL_FS_8G:
			pDev->Acceleration.X = IIS3DWB_ACC_TO_8G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS3DWB_ACC_TO_8G_uG(pDev->Register.Data.ACCEL_Y);
			pDev->Acceleration.Z = IIS3DWB_ACC_TO_8G_uG(pDev->Register.Data.ACCEL_Z);
			break;
		case IIS3DWB_XL_FS_16G:
			pDev->Acceleration.X = IIS3DWB_ACC_TO_16G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS3DWB_ACC_TO_16G_uG(pDev->Register.Data.ACCEL_Y);
			pDev->Acceleration.Z = IIS3DWB_ACC_TO_16G_uG(pDev->Register.Data.ACCEL_Z);
			break;
	}

	return RESULT_OK;
}

/* ============================================================
 * Open — non-blocking state machine
 *   0: SPI Init 확인 → CS 핀 idle
 *   1: WHO_AM_I 검증
 *   2: Soft Reset
 *   3: 기본 설정 적용 (CTRL1_XL/CTRL3_C/CTRL8_XL)
 *   4: IsRun=1 로 전환
 * ============================================================ */
oResult_t IIS3DWB_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIS3DWB_t *pDev)
{
	oResult_t result = RESULT_RUN;
	uint8_t id;

	if(pSPI == NULL || pDev == NULL){
		return RESULT_ERROR;
	}

	if(pDev->State.IsOpen && !pDev->State.IsBusy){
		return RESULT_OK;
	}
	else if(!pDev->State.IsBusy && pDev->State.SequenceStep != 0){
		memset(&pDev->State, 0, sizeof(pDev->State));
	}

	pDev->State.IsBusy = 1;

	switch(pDev->State.SequenceStep)
	{
		case 0:
			pDev->pSPI = pSPI;
			pDev->CS = CS;

			/* SPI 페리페럴 재초기화 — 전원 토글 후 stale state 제거 */
			oSerial_Log("IIS3DW", "Open start");

			HAL_SPI_DeInit(pDev->pSPI);
			if(HAL_SPI_Init(pDev->pSPI) != HAL_OK){
				oSerial_Log("IIS3DW", "Open fail | HAL_SPI_Init");
				result = RESULT_ERROR;
				break;
			}

			/* CS idle high (active low 가정) */
			if(pDev->CS.Port != NULL){
				HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
			}

			pDev->State.IsOpen = 1;
			pDev->State.SequenceStep++;
			break;

		case 1:
			if(IIS3DWB_WhoAmI(pDev, &id) != RESULT_OK){
				oSerial_Log("IIS3DW", "Open fail | WHO_AM_I read");
				result = RESULT_ERROR;
				break;
			}
			pDev->Register.WHO_AM_I = id;	/* 불일치 시에도 보존 — 호출부가 실측값을 로그로 낸다 */
			if(id != IIS3DWB_WHO_AM_I_VALUE){
				oSerial_Log("IIS3DW", "Open fail | WHO_AM_I=0x%02X (expect 0x%02X)",
				            (unsigned int)id, (unsigned int)IIS3DWB_WHO_AM_I_VALUE);
				result = RESULT_ERROR;	/* 칩 식별 실패 */
				break;
			}
			oSerial_Log("IIS3DW", "WHO_AM_I=0x%02X", (unsigned int)id);
			pDev->State.SequenceStep++;
			break;

		case 2:
			if(IIS3DWB_SoftReset(pDev) != RESULT_OK){
				result = RESULT_ERROR;
				break;
			}
			pDev->State.SequenceStep++;
			break;

		case 3:
			/* 기본 설정 — 호출 측이 별도 지정 안 하면 다음 값 사용 */
			if(pDev->Register.CTRL1_XL.Byte == 0){
				pDev->Register.CTRL1_XL.ODR_XL = IIS3DWB_XL_ODR_26_667K;
				pDev->Register.CTRL1_XL.FS_XL  = IIS3DWB_XL_FS_4G;
			}

			if(IIS3DWB_SetConfig(pDev) != RESULT_OK){
				result = RESULT_ERROR;
				break;
			}
			pDev->State.SequenceStep++;
			break;

		case 4:
			pDev->State.IsRun = 1;
			oSerial_Log("IIS3DW", "Open okay | ODR=%u FS=%u",
			            (unsigned int)pDev->Register.CTRL1_XL.ODR_XL,
			            (unsigned int)pDev->Register.CTRL1_XL.FS_XL);
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		pDev->State.SequenceStep = 0;
		pDev->State.IsBusy = 0;

		if(result == RESULT_ERROR){
			pDev->State.IsOpen = 0;
			pDev->State.IsRun = 0;
		}
	}

	return result;
}

/* ============================================================
 * FIFO API
 * ============================================================ */

/* FIFO 모드·watermark·BDR 일괄 설정.
 *   Mode       : Continuous(6) 권장 — 링버퍼 동작
 *   Watermark  : 0~511 entry (각 7 byte)
 *   BDR_XL     : 26.667 kHz 만 유효
 */
oResult_t IIS3DWB_FIFO_Setup(IIS3DWB_t *pDev, IIS3DWB_FifoMode_t Mode,
                             uint16_t Watermark, IIS3DWB_BDR_t BDR_XL)
{
	oResult_t result;
	uint8_t reg;

	if(pDev == NULL || !pDev->State.IsOpen){
		return RESULT_ERROR;
	}

	/* FIFO_CTRL1 : watermark[7:0] */
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_FIFO_CTRL1, Watermark & 0xFF)) != RESULT_OK){
		oSerial_Log("IIS3DW", "FIFO_Setup fail | FIFO_CTRL1");
		return result;
	}

	/* FIFO_CTRL2 : watermark[8] (bit 0), STOP_ON_WTM 등은 0 */
	reg = (Watermark >> 8) & 0x01;
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_FIFO_CTRL2, reg)) != RESULT_OK){
		oSerial_Log("IIS3DW", "FIFO_Setup fail | FIFO_CTRL2");
		return result;
	}

	/* FIFO_CTRL3 : BDR_XL[3:0] — 자이로가 없어 상위 니블은 0 */
	reg = BDR_XL & 0x0F;
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_FIFO_CTRL3, reg)) != RESULT_OK){
		oSerial_Log("IIS3DW", "FIFO_Setup fail | FIFO_CTRL3");
		return result;
	}

	/* FIFO_CTRL4 : ODR_T_BATCH[5:4]=0 (temp off), DEC_TS[7:6]=0, FIFO_MODE[2:0] */
	reg = Mode & 0x07;
	if((result = IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_FIFO_CTRL4, reg)) != RESULT_OK){
		oSerial_Log("IIS3DW", "FIFO_Setup fail | FIFO_CTRL4");
		return result;
	}

	oSerial_Log("IIS3DW", "FIFO_Setup okay | mode=%u watermark=%u BDR=%u",
	            (unsigned int)Mode, (unsigned int)Watermark, (unsigned int)BDR_XL);
	return RESULT_OK;
}

/* FIFO_STATUS1/2 → unread entry 수 (0~511) */
oResult_t IIS3DWB_FIFO_GetUnreadCount(IIS3DWB_t *pDev, uint16_t *pCount)
{
	uint8_t status[2];
	oResult_t result;

	if(pDev == NULL || pCount == NULL){
		return RESULT_NULL;
	}

	if((result = IIS3DWB_ReadReg(pDev, IIS3DWB_REG_FIFO_STATUS1, status, 2)) != RESULT_OK){
		return result;
	}

	*pCount = ((uint16_t)(status[1] & 0x03) << 8) | status[0];
	return RESULT_OK;
}

/* FIFO_DATA_OUT_TAG (0x78) 부터 EntryCount × 7 byte 일괄 read.
 *   호출자가 충분한 buffer 제공 필수 (EntryCount × 7 byte).
 */
oResult_t IIS3DWB_FIFO_ReadBurst(IIS3DWB_t *pDev, uint8_t *pBuf, uint16_t EntryCount)
{
	if(pDev == NULL || pBuf == NULL || EntryCount == 0){
		return RESULT_NULL;
	}

	return IIS3DWB_ReadReg(pDev, IIS3DWB_REG_FIFO_DATA_OUT_TAG, pBuf,
	                      (uint32_t)EntryCount * IIS3DWB_FIFO_ENTRY_SIZE);
}

/* INT1 핀에 FIFO_TH·FIFO_OVR 매핑. INT1_CTRL (0x0D). */
oResult_t IIS3DWB_FIFO_EnableINT1(IIS3DWB_t *pDev, uint8_t EnableThreshold, uint8_t EnableOverrun)
{
	uint8_t reg;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	reg = 0;
	if(EnableThreshold) reg |= (1 << 3);	/* FIFO_TH on INT1 */
	if(EnableOverrun)   reg |= (1 << 4);	/* FIFO_OVR on INT1 */

	oSerial_Log("IIS3DW", "FIFO INT1 | threshold=%u overrun=%u",
	            (unsigned int)(EnableThreshold ? 1 : 0), (unsigned int)(EnableOverrun ? 1 : 0));
	return IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_INT1_CTRL, reg);
}

/* FIFO entry data 6 byte (X_L/H, Y_L/H, Z_L/H) → 가속도 µG 변환.
 *   현재 CTRL1_XL.FS_XL 설정 기반 스케일.
 *   반환: 벡터합(√(X²+Y²+Z²)), pAccelXYZ 에 각 축 µG 채움 (NULL 허용).
 */
double IIS3DWB_FIFO_ConvertAccel(IIS3DWB_t *pDev, const uint8_t *pData6, double *pAccelXYZ)
{
	int16_t rawX, rawY, rawZ;
	double x, y, z;

	if(pDev == NULL || pData6 == NULL){
		return 0;
	}

	rawX = IIS3DWB_DATACONVERT_16BIT(pData6[1], pData6[0]);
	rawY = IIS3DWB_DATACONVERT_16BIT(pData6[3], pData6[2]);
	rawZ = IIS3DWB_DATACONVERT_16BIT(pData6[5], pData6[4]);

	switch(pDev->Register.CTRL1_XL.FS_XL)
	{
		default:
		case IIS3DWB_XL_FS_2G:
			x = IIS3DWB_ACC_TO_2G_uG(rawX);
			y = IIS3DWB_ACC_TO_2G_uG(rawY);
			z = IIS3DWB_ACC_TO_2G_uG(rawZ);
			break;
		case IIS3DWB_XL_FS_4G:
			x = IIS3DWB_ACC_TO_4G_uG(rawX);
			y = IIS3DWB_ACC_TO_4G_uG(rawY);
			z = IIS3DWB_ACC_TO_4G_uG(rawZ);
			break;
		case IIS3DWB_XL_FS_8G:
			x = IIS3DWB_ACC_TO_8G_uG(rawX);
			y = IIS3DWB_ACC_TO_8G_uG(rawY);
			z = IIS3DWB_ACC_TO_8G_uG(rawZ);
			break;
		case IIS3DWB_XL_FS_16G:
			x = IIS3DWB_ACC_TO_16G_uG(rawX);
			y = IIS3DWB_ACC_TO_16G_uG(rawY);
			z = IIS3DWB_ACC_TO_16G_uG(rawZ);
			break;
	}

	if(pAccelXYZ){
		pAccelXYZ[0] = x;
		pAccelXYZ[1] = y;
		pAccelXYZ[2] = z;
	}

	return sqrt(x*x + y*y + z*z);
}

/* ============================================================
 * Close — Accel 전원 OFF + IsOpen=0
 * ============================================================ */
oResult_t IIS3DWB_Close(IIS3DWB_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(!pDev->State.IsOpen){
		return RESULT_OK;
	}

	oSerial_Log("IIS3DW", "Close");

	/* ODR=0 으로 Accel power-down */
	pDev->Register.CTRL1_XL.ODR_XL = IIS3DWB_XL_ODR_OFF;
	IIS3DWB_WriteRegByte(pDev, IIS3DWB_REG_CTRL1_XL, pDev->Register.CTRL1_XL.Byte);

	/* CS idle high */
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}

	/* SPI 페리페럴 해제 — 전원 차단 전 호출 가정 */
	HAL_SPI_DeInit(pDev->pSPI);

	memset(&pDev->State, 0, sizeof(pDev->State));

	return RESULT_OK;
}
