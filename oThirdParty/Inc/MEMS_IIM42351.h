/*
 * MEMS_IIM42351.h
 *
 *  Created on: Apr 10, 2026
 *      Author: JONE
 *
 ************************************************************
 * IIM-42351 (TDK InvenSense) 3-Axis SmartIndustrial Accelerometer
 * - 3-axis accelerometer (No gyroscope)
 * - SPI (up to 24MHz) / I2C / I3C interface
 * - 16-bit data output
 *
 * Accelerometer Sensitivity (LSB/g):
 * +/-2g  -> 16,384 LSB/g  (1 LSB = 61.035 uG)
 * +/-4g  ->  8,192 LSB/g  (1 LSB = 122.070 uG)
 * +/-8g  ->  4,096 LSB/g  (1 LSB = 244.141 uG)
 * +/-16g ->  2,048 LSB/g  (1 LSB = 488.281 uG)
 *
 * Temperature: Temp(C) = (TEMP_DATA / 132.48) + 25
 *
 * Note: Register-compatible with ICM-42688-P (accel subset)
 *************************************************************
 */

#ifndef INC_MEMS_IIM42351_H_
#define INC_MEMS_IIM42351_H_

#define MEMS_IIM42351_VERSION                        0.2
#include "Mi_Main.h"

/* WHO_AM_I expected value */
#define IIM42351_WHO_AM_I_VALUE                      0x6C

/* Data conversion macros */
#define IIM42351_DATACONVERT_TO_ACC_16BIT(msb, lsb)  ((int16_t)((uint16_t)(msb) << 8 | (uint16_t)(lsb)))

/* Accelerometer: raw -> uG (micro-G) */
#define IIM42351_ACC_TO_2G_uG(raw)                   ((double)(raw) * 61.035)
#define IIM42351_ACC_TO_4G_uG(raw)                   ((double)(raw) * 122.070)
#define IIM42351_ACC_TO_8G_uG(raw)                   ((double)(raw) * 244.141)
#define IIM42351_ACC_TO_16G_uG(raw)                  ((double)(raw) * 488.281)

/* Temperature: raw -> degree Celsius */
#define IIM42351_TEMP_TO_Degree(raw)                 ((double)(raw) / 132.48 + 25.0)

/* Accelerometer Full-Scale Range */
typedef enum{
	IIM42351_ACCEL_FS_16G  = 0b000,
	IIM42351_ACCEL_FS_8G   = 0b001,
	IIM42351_ACCEL_FS_4G   = 0b010,
	IIM42351_ACCEL_FS_2G   = 0b011,
}IIM42351_AccelFS_t;

/* Accelerometer ODR */
typedef enum{
	IIM42351_ACCEL_ODR_8KHZ      = 0b0011,
	IIM42351_ACCEL_ODR_4KHZ      = 0b0100,
	IIM42351_ACCEL_ODR_2KHZ      = 0b0101,
	IIM42351_ACCEL_ODR_1KHZ      = 0b0110,
	IIM42351_ACCEL_ODR_200HZ     = 0b0111,
	IIM42351_ACCEL_ODR_100HZ     = 0b1000,
	IIM42351_ACCEL_ODR_50HZ      = 0b1001,
	IIM42351_ACCEL_ODR_25HZ      = 0b1010,
	IIM42351_ACCEL_ODR_12_5HZ    = 0b1011,
	IIM42351_ACCEL_ODR_6_25HZ    = 0b1100,
	IIM42351_ACCEL_ODR_3_125HZ   = 0b1101,
	IIM42351_ACCEL_ODR_1_5625HZ  = 0b1110,
	IIM42351_ACCEL_ODR_500HZ     = 0b1111,
}IIM42351_AccelODR_t;

/* Power Mode for Accelerometer */
typedef enum{
	IIM42351_ACCEL_MODE_OFF   = 0b00,
	IIM42351_ACCEL_MODE_OFF2  = 0b01,
	IIM42351_ACCEL_MODE_LP    = 0b10,
	IIM42351_ACCEL_MODE_LN    = 0b11,
}IIM42351_AccelMode_t;

/* Accelerometer UI Filter Bandwidth */
typedef enum{
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_2   = 0,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_4   = 1,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_5   = 2,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_8   = 3,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_10  = 4,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_16  = 5,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_20  = 6,
	IIM42351_ACCEL_UI_FILT_BW_LPF_ODR_40  = 7,
}IIM42351_AccelUIFiltBW_t;

#pragma pack(1)
typedef struct{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;

	struct{
		uint8_t WHO_AM_I;     /* 0x75: WHO_AM_I (expected 0x6C) */

		union{
			struct{
				uint8_t AGC_RDY		: 1;
				uint8_t				: 1;
				uint8_t FIFO_FULL	: 1;
				uint8_t FIFO_THS	: 1;
				uint8_t DATA_RDY	: 1;
				uint8_t RESET_DONE	: 1;
				uint8_t				: 1;
				uint8_t UI_FSYNC	: 1;
			};

			uint8_t Byte;
		}INT_STATUS;          /* 0x2D */

		struct{
			int16_t TEMP;
			int16_t ACCEL_X;
			int16_t ACCEL_Y;
			int16_t ACCEL_Z;
		}Data;

		union{
			struct{
				IIM42351_AccelMode_t ACCEL_MODE	: 2;
				uint8_t							: 3;
				uint8_t TEMP_DIS				: 1;
				uint8_t							: 2;
			};

			uint8_t Byte;
		}PWR_MGMT0;           /* 0x4E */

		union{
			struct{
				IIM42351_AccelODR_t ACCEL_ODR	: 4;
				uint8_t							: 1;
				IIM42351_AccelFS_t  ACCEL_FS_SEL	: 3;
			};

			uint8_t Byte;
		}ACCEL_CONFIG0;       /* 0x50 */

		union{
			struct{
				uint8_t							: 4;
				IIM42351_AccelUIFiltBW_t ACCEL_UI_FILT_BW	: 4;
			};

			uint8_t Byte;
		}GYRO_ACCEL_CONFIG0;  /* 0x52 (Accel BW in upper nibble) */

		union{
			struct{
				uint8_t SPI_MODE			: 1;
				uint8_t SPI_AP_4WIRE		: 1;
				uint8_t						: 2;
				uint8_t SOFT_RESET_CONFIG	: 1;
				uint8_t						: 3;
			};

			uint8_t Byte;
		}DEVICE_CONFIG;       /* 0x11 */

	}Register;

	double Temperature;
	double DataRate;

	oVector3_t Acceleration;  /* uG */
	oRPY_t Angle;
	oRPY_t OverSampling;

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfOverSampling;
		uint8_t CountOfError;
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}IIM42351_t;
#pragma pack()

extern oResult_t IIM42351_GetStatus(IIM42351_t *pIIM);
extern oResult_t IIM42351_GetData(IIM42351_t *pIIM);
extern oResult_t IIM42351_GetRPY(IIM42351_t *pIIM, uint8_t Count, oRPY_t *pAngle);
extern oResult_t IIM42351_SetConfig(IIM42351_t *pIIM);
extern oResult_t IIM42351_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIM42351_t *pIIM);
extern oResult_t IIM42351_Close(IIM42351_t *pIIM);

#endif /* INC_MEMS_IIM42351_H_ */

/* History

2026-06-26 | v0.1
	- baseline (MEMS_IIM42351.h)

2026-10-20 | v0.2
	- CS 인자 값 전달 통일 - CS.Port == NULL 이면 CS 없음으로 보고 Open 에서 거부하지 않으며, CS 토글 지점은 Port NULL 이면 건너뜀
*/
