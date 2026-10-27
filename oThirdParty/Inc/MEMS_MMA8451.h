/*
 * MEMS_MMA8451.h
 *
 *  Version: 0.2 (2026-10-14)
 *
 ************************************************************
 * MMA8451 (Freescale/NXP) 3-Axis 14-bit MEMS Accelerometer
 * - I2C only (this part has no SPI port)
 * - Fast mode 400kHz / Normal mode 100kHz
 * - 14-bit left-justified 2's complement output
 *
 * Source: Freescale MMA8451Q Data Sheet, Document Number MMA8451Q, Rev. 7.1, 05/2012
 *   Values below were confirmed against this document (nxp.com family).
 *   WHO_AM_I = 0x1A     Table 12 (register address map, 0x0D), read only.
 *   Sensitivity         Section 3.1 and Table 2: 4096 / 2048 / 1024 counts/g
 *                       for 2g / 4g / 8g (FS[1:0] = 00 / 01 / 10).
 *   14-bit alignment    Table 12 and the OUT_x_LSB register description: OUT_x_MSB
 *                       holds data bits [13:6] and OUT_x_LSB bits [7:2] hold the
 *                       6 LSBs, i.e. left-justified 2's complement with bits
 *                       [1:0] of the LSB register unused (arithmetic shift
 *                       right by 2 yields the 14-bit value).
 *   F_SETUP  (0x09)     FIFO setup and trigger configuration, Table 12.
 *   TRIG_CFG (0x0A)
 *
 * Sensitivity (14-bit):
 * +/-2g -> 4096 counts/g  (1 count = 244.141 uG)
 * +/-4g -> 2048 counts/g  (1 count = 488.281 uG)
 * +/-8g -> 1024 counts/g  (1 count = 976.563 uG)
 *
 * I2C device address, 8-bit form as passed to the STM32 HAL:
 *   SA0 = 0 -> 0x38      SA0 = 1 -> 0x3A      (7-bit: 0x1C / 0x1D)
 *
 * Access rules that constrain the driver (datasheet p.19 note 4, p.37):
 * - Every R/W register except CTRL_REG1.ACTIVE and CTRL_REG2.RST may be
 *   written only while the device is in STANDBY. Changing range, data
 *   rate or oversampling therefore needs ACTIVE=0 -> write -> ACTIVE=1.
 * - OUT_x_LSB can only be read in the access right after its own
 *   OUT_x_MSB. A random read of an LSB register returns nothing useful,
 *   so status and the three axes are fetched as one 7-byte burst.
 * - Auto-increment applies to I2C burst access only and is cleared by a
 *   STOP condition, which is why the burst above must not be split.
 * - Turn-on time from STANDBY to ACTIVE is 1.1/ODR and the first sample
 *   is imprecise, so the first samples are discarded.
 * - LNOISE caps the measurable signal at 4g even when 8g is selected.
 *
 * Porting note (MEMS_IIM42351):
 *   This driver deliberately mirrors MEMS_IIM42351 so the two can be
 *   swapped by renaming the prefix. One struct instance equals one
 *   sensor; several sensors on a shared bus are an array of instances,
 *   each with its own chip select. The function set, the struct layout
 *   (Register / Acceleration / Angle / OverSampling / State) and the
 *   Open sequence steps line up one to one. Differences that remain:
 *     - Open() takes I2C_HandleTypeDef* where IIM42351 takes SPI_HandleTypeDef*
 *     - DeviceAddress member (I2C only; IIM42351 selects purely by CS)
 *     - no Temperature member (this part has no temperature output)
 *   CS polarity follows IIM42351: CS.ActiveLevel holds the IDLE level and
 *   the driver drives !ActiveLevel while a transfer is in progress.
 *************************************************************
 */

#ifndef INC_MEMS_MMA8451_H_
#define INC_MEMS_MMA8451_H_

#define MEMS_MMA8451_VERSION                        0.3

#include "Mi_Main.h"

/* WHO_AM_I expected value */
#define MMA8451_WHO_AM_I_VALUE                      0x1A

/* I2C device address, 8-bit form (datasheet Table 10, p.17) */
#define MMA8451_I2C_ADDRESS_SA0_LOW                 0x38
#define MMA8451_I2C_ADDRESS_SA0_HIGH                0x3A

/* Register addresses (datasheet Table 11, p.18) */
#define MMA8451_REG_STATUS                          0x00
#define MMA8451_REG_OUT_X_MSB                       0x01
#define MMA8451_REG_OUT_X_LSB                       0x02
#define MMA8451_REG_OUT_Y_MSB                       0x03
#define MMA8451_REG_OUT_Y_LSB                       0x04
#define MMA8451_REG_OUT_Z_MSB                       0x05
#define MMA8451_REG_OUT_Z_LSB                       0x06
#define MMA8451_REG_F_SETUP                         0x09  /* MMA8451 only (32-level FIFO) */
#define MMA8451_REG_TRIG_CFG                        0x0A  /* MMA8451 only (FIFO trigger) */
#define MMA8451_REG_SYSMOD                          0x0B
#define MMA8451_REG_INT_SOURCE                      0x0C
#define MMA8451_REG_WHO_AM_I                        0x0D
#define MMA8451_REG_XYZ_DATA_CFG                    0x0E
#define MMA8451_REG_HP_FILTER_CUTOFF                0x0F
#define MMA8451_REG_PL_STATUS                       0x10
#define MMA8451_REG_PL_CFG                          0x11
#define MMA8451_REG_PL_COUNT                        0x12
#define MMA8451_REG_PL_BF_ZCOMP                     0x13
#define MMA8451_REG_P_L_THS_REG                     0x14
#define MMA8451_REG_FF_MT_CFG                       0x15
#define MMA8451_REG_FF_MT_SRC                       0x16
#define MMA8451_REG_FF_MT_THS                       0x17
#define MMA8451_REG_FF_MT_COUNT                     0x18
#define MMA8451_REG_TRANSIENT_CFG                   0x1D
#define MMA8451_REG_TRANSIENT_SRC                   0x1E
#define MMA8451_REG_TRANSIENT_THS                   0x1F
#define MMA8451_REG_TRANSIENT_COUNT                 0x20
#define MMA8451_REG_PULSE_CFG                       0x21
#define MMA8451_REG_PULSE_SRC                       0x22
#define MMA8451_REG_PULSE_THSX                      0x23
#define MMA8451_REG_PULSE_THSY                      0x24
#define MMA8451_REG_PULSE_THSZ                      0x25
#define MMA8451_REG_PULSE_TMLT                      0x26
#define MMA8451_REG_PULSE_LTCY                      0x27
#define MMA8451_REG_PULSE_WIND                      0x28
#define MMA8451_REG_ASLP_COUNT                      0x29
#define MMA8451_REG_CTRL_REG1                       0x2A
#define MMA8451_REG_CTRL_REG2                       0x2B
#define MMA8451_REG_CTRL_REG3                       0x2C
#define MMA8451_REG_CTRL_REG4                       0x2D
#define MMA8451_REG_CTRL_REG5                       0x2E
#define MMA8451_REG_OFF_X                           0x2F
#define MMA8451_REG_OFF_Y                           0x30
#define MMA8451_REG_OFF_Z                           0x31

/* SYSMOD (0x0B) values */
#define MMA8451_SYSMOD_STANDBY                      0x00
#define MMA8451_SYSMOD_WAKE                         0x01
#define MMA8451_SYSMOD_SLEEP                        0x02

/* Software reset: CTRL_REG2 RST, self-clearing when the boot completes */
#define MMA8451_SOFT_RESET                          0x40

/* Offset correction (0x2F~0x31): 2's complement, 2mg per LSB, +/-256mg */
#define MMA8451_OFFSET_mg_PER_LSB                   2

/* A missing or mis-wired sensor must not stall the caller: every transfer
 * is bounded so the open sequence can mark that channel dead and move on */
#define MMA8451_I2C_TIMEOUT_MS                      10

/* First samples after STANDBY->ACTIVE are imprecise (Table 3 note 2, p.7) */
#define MMA8451_WARMUP_SAMPLE_COUNT                 3

/* Data conversion macros
 * 14-bit left justified: MSB holds [13:6], LSB holds [5:0] in bits 7:2 */
#define MMA8451_DATACONVERT_TO_ACC_14BIT(msb, lsb)  ((int16_t)(((uint16_t)(msb) << 8) | (uint16_t)(lsb)) >> 2)

/* Accelerometer: raw -> uG (micro-G) */
#define MMA8451_ACC_TO_2G_uG(raw)                   ((double)(raw) * 244.141)
#define MMA8451_ACC_TO_4G_uG(raw)                   ((double)(raw) * 488.281)
#define MMA8451_ACC_TO_8G_uG(raw)                   ((double)(raw) * 976.563)

/* Accelerometer Full-Scale Range, XYZ_DATA_CFG FS[1:0] (Table 16, p.22) */
typedef enum{
	MMA8451_ACCEL_FS_2G  = 0b00,
	MMA8451_ACCEL_FS_4G  = 0b01,
	MMA8451_ACCEL_FS_8G  = 0b10,
}MMA8451_AccelFS_t;

/* Accelerometer ODR, CTRL_REG1 DR[2:0] (Table 55, p.37) */
typedef enum{
	MMA8451_ACCEL_ODR_800HZ   = 0b000,
	MMA8451_ACCEL_ODR_400HZ   = 0b001,
	MMA8451_ACCEL_ODR_200HZ   = 0b010,
	MMA8451_ACCEL_ODR_100HZ   = 0b011,
	MMA8451_ACCEL_ODR_50HZ    = 0b100,
	MMA8451_ACCEL_ODR_12_5HZ  = 0b101,
	MMA8451_ACCEL_ODR_6_25HZ  = 0b110,
	MMA8451_ACCEL_ODR_1_56HZ  = 0b111,
}MMA8451_AccelODR_t;

/* Auto-WAKE sample rate in SLEEP, CTRL_REG1 ASLP_RATE[1:0] (Table 54) */
typedef enum{
	MMA8451_SLEEP_ODR_50HZ    = 0b00,
	MMA8451_SLEEP_ODR_12_5HZ  = 0b01,
	MMA8451_SLEEP_ODR_6_25HZ  = 0b10,
	MMA8451_SLEEP_ODR_1_56HZ  = 0b11,
}MMA8451_SleepODR_t;

/* Oversampling scheme, CTRL_REG2 MODS[1:0] / SMODS[1:0] (Table 58, p.38) */
typedef enum{
	MMA8451_ACCEL_MODE_NORMAL          = 0b00,
	MMA8451_ACCEL_MODE_LOWNOISE        = 0b01,
	MMA8451_ACCEL_MODE_HIGHRESOLUTION  = 0b10,
	MMA8451_ACCEL_MODE_LOWPOWER        = 0b11,
}MMA8451_AccelMode_t;

/* High-pass cutoff, HP_FILTER_CUTOFF SEL[1:0] (Table 18, p.23).
 * The resulting frequency depends on both ODR and the oversampling mode. */
typedef enum{
	MMA8451_HPF_CUTOFF_HIGHEST  = 0b00,
	MMA8451_HPF_CUTOFF_HIGH     = 0b01,
	MMA8451_HPF_CUTOFF_LOW      = 0b10,
	MMA8451_HPF_CUTOFF_LOWEST   = 0b11,
}MMA8451_HPFCutoff_t;

#pragma pack(1)
typedef struct{
	I2C_HandleTypeDef *pI2C;
	oIO_t CS;
	uint8_t DeviceAddress;                               /* 8-bit form; Open() defaults it to SA0_LOW */

	struct{
		uint8_t WHO_AM_I;                                /* 0x0D: WHO_AM_I (expected 0x1A) */

		union{
			struct{
				uint8_t XDR			: 1;
				uint8_t YDR			: 1;
				uint8_t ZDR			: 1;
				uint8_t ZYXDR		: 1;
				uint8_t XOW			: 1;
				uint8_t YOW			: 1;
				uint8_t ZOW			: 1;
				uint8_t ZYXOW		: 1;
			};

			uint8_t Byte;
		}STATUS;                                         /* 0x00 */

		struct{
			int16_t ACCEL_X;
			int16_t ACCEL_Y;
			int16_t ACCEL_Z;
		}Data;

		union{
			struct{
				MMA8451_AccelFS_t ACCEL_FS_SEL	: 2;   /* FS[1:0] */
				uint8_t							: 2;
				uint8_t HPF_OUT					: 1;
				uint8_t							: 3;
			};

			uint8_t Byte;
		}XYZ_DATA_CFG;                                   /* 0x0E */

		union{
			struct{
				MMA8451_HPFCutoff_t HPF_SEL		: 2;  /* SEL[1:0] */
				uint8_t							: 2;
				uint8_t Pulse_LPF_EN			: 1;
				uint8_t Pulse_HPF_BYP			: 1;
				uint8_t							: 2;
			};

			uint8_t Byte;
		}HP_FILTER_CUTOFF;                               /* 0x0F */

		union{
			struct{
				uint8_t ACTIVE					: 1;   /* 0=STANDBY 1=ACTIVE */
				uint8_t F_READ					: 1;   /* 1=MSB only, 8-bit data */
				uint8_t LNOISE					: 1;   /* 1=reduced noise, capped at 4g */
				MMA8451_AccelODR_t ACCEL_ODR	: 3;     /* DR[2:0] */
				MMA8451_SleepODR_t ASLP_RATE	: 2;
			};

			uint8_t Byte;
		}CTRL_REG1;                                      /* 0x2A */

		union{
			struct{
				MMA8451_AccelMode_t MODS		: 2;     /* ACTIVE oversampling */
				uint8_t SLPE					: 1;
				MMA8451_AccelMode_t SMODS		: 2;    /* SLEEP oversampling */
				uint8_t							: 1;
				uint8_t RST						: 1;  /* self-clearing after boot */
				uint8_t ST						: 1;
			};

			uint8_t Byte;
		}CTRL_REG2;                                      /* 0x2B */

	}Register;

	double DataRate;

	oVector3_t Acceleration;                             /* uG */
	oRPY_t Angle;
	oRPY_t OverSampling;

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfOverSampling;
		uint8_t CountOfError;
		uint8_t CountOfWarmUp;                           /* samples discarded after STANDBY->ACTIVE */
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}MMA8451_t;
#pragma pack()

extern oResult_t MMA8451_GetStatus(MMA8451_t *pMMA);
extern oResult_t MMA8451_GetData(MMA8451_t *pMMA);
extern oResult_t MMA8451_GetRPY(MMA8451_t *pMMA, uint8_t Count, oRPY_t *pAngle);
extern oResult_t MMA8451_SetConfig(MMA8451_t *pMMA);
extern oResult_t MMA8451_Open(I2C_HandleTypeDef *pI2C, oIO_t CS, MMA8451_t *pMMA);
extern oResult_t MMA8451_Close(MMA8451_t *pMMA);

/* History

2026-10-14 | v0.1
    - baseline, multi-channel chip select held inside one instance

2026-10-14 | v0.2
    - reshaped to match MEMS_IIM42351 one to one so the parts can be
      swapped by renaming the prefix: one instance per sensor, oIO_t CS,
      GetStatus/GetData/GetRPY/SetConfig/Open/Close, same Open sequence
    - header comments confirmed against MMA8451Q datasheet Rev. 7.1
      (no code change)

*/

#endif /* INC_MEMS_MMA8451_H_ 
2026-10-20 | v0.3
	- CS handed over by value; CS.Port == NULL means no CS line, Open no longer rejects it and every CS toggle is skipped for a NULL Port
*/
