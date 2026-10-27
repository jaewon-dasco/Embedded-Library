/*
 * MEMS_ICM42688P.h
 *
 *  Created on: Apr 10, 2026
 *      Author: JONE
 *
 ************************************************************
 * ICM-42688-P (TDK InvenSense) 6-Axis MEMS IMU
 * - 3-axis accelerometer + 3-axis gyroscope
 * - SPI (up to 24MHz) / I2C interface
 * - 16-bit data output
 *
 * Accelerometer Sensitivity (LSB/g):
 * +/-2g  -> 16,384 LSB/g  (1 LSB = 61.035 uG)
 * +/-4g  ->  8,192 LSB/g  (1 LSB = 122.070 uG)
 * +/-8g  ->  4,096 LSB/g  (1 LSB = 244.141 uG)
 * +/-16g ->  2,048 LSB/g  (1 LSB = 488.281 uG)
 *
 * Gyroscope Sensitivity (LSB/dps):
 * +/-250 dps  -> 131.072 LSB/dps
 * +/-500 dps  ->  65.536 LSB/dps
 * +/-1000 dps ->  32.768 LSB/dps
 * +/-2000 dps ->  16.384 LSB/dps
 *
 * Temperature: Temp(C) = (TEMP_DATA / 132.48) + 25
 *************************************************************
 */

#ifndef INC_MEMS_ICM42688P_H_
#define INC_MEMS_ICM42688P_H_

#define MEMS_ICM42688P_VERSION                        0.2
#include "Mi_Main.h"

/* WHO_AM_I expected values */
#define ICM42688P_WHO_AM_I_VALUE                      0x47
#define ICM42688V_WHO_AM_I_VALUE                      0x6C

/* Data conversion macros */
#define ICM42688P_DATACONVERT_TO_ACC_16BIT(msb, lsb)  ((int16_t)((uint16_t)(msb) << 8 | (uint16_t)(lsb)))

/* Accelerometer: raw -> uG (micro-G) */
#define ICM42688P_ACC_TO_2G_uG(raw)                   ((double)(raw) * 61.035)
#define ICM42688P_ACC_TO_4G_uG(raw)                   ((double)(raw) * 122.070)
#define ICM42688P_ACC_TO_8G_uG(raw)                   ((double)(raw) * 244.141)
#define ICM42688P_ACC_TO_16G_uG(raw)                  ((double)(raw) * 488.281)

/* Gyroscope: raw -> mdps (milli-degree per second) */
#define ICM42688P_GYRO_TO_250DPS(raw)                 ((double)(raw) * 7.6294)
#define ICM42688P_GYRO_TO_500DPS(raw)                 ((double)(raw) * 15.2588)
#define ICM42688P_GYRO_TO_1000DPS(raw)                ((double)(raw) * 30.5176)
#define ICM42688P_GYRO_TO_2000DPS(raw)                ((double)(raw) * 61.0352)

/* Temperature: raw -> degree Celsius */
#define ICM42688P_TEMP_TO_Degree(raw)                 ((double)(raw) / 132.48 + 25.0)

/* Accelerometer Full-Scale Range */
typedef enum{
	ICM42688P_ACCEL_FS_16G  = 0b000,
	ICM42688P_ACCEL_FS_8G   = 0b001,
	ICM42688P_ACCEL_FS_4G   = 0b010,
	ICM42688P_ACCEL_FS_2G   = 0b011,
}ICM42688P_AccelFS_t;

/* Gyroscope Full-Scale Range */
typedef enum{
	ICM42688P_GYRO_FS_2000DPS  = 0b000,
	ICM42688P_GYRO_FS_1000DPS  = 0b001,
	ICM42688P_GYRO_FS_500DPS   = 0b010,
	ICM42688P_GYRO_FS_250DPS   = 0b011,
}ICM42688P_GyroFS_t;

/* Accelerometer ODR */
typedef enum{
	ICM42688P_ACCEL_ODR_32KHZ     = 0b0001,
	ICM42688P_ACCEL_ODR_16KHZ     = 0b0010,
	ICM42688P_ACCEL_ODR_8KHZ      = 0b0011,
	ICM42688P_ACCEL_ODR_4KHZ      = 0b0100,
	ICM42688P_ACCEL_ODR_2KHZ      = 0b0101,
	ICM42688P_ACCEL_ODR_1KHZ      = 0b0110,
	ICM42688P_ACCEL_ODR_200HZ     = 0b0111,
	ICM42688P_ACCEL_ODR_100HZ     = 0b1000,
	ICM42688P_ACCEL_ODR_50HZ      = 0b1001,
	ICM42688P_ACCEL_ODR_25HZ      = 0b1010,
	ICM42688P_ACCEL_ODR_12_5HZ    = 0b1011,
	ICM42688P_ACCEL_ODR_6_25HZ    = 0b1100,
	ICM42688P_ACCEL_ODR_3_125HZ   = 0b1101,
	ICM42688P_ACCEL_ODR_1_5625HZ  = 0b1110,
	ICM42688P_ACCEL_ODR_500HZ     = 0b1111,
}ICM42688P_AccelODR_t;

/* Gyroscope ODR */
typedef enum{
	ICM42688P_GYRO_ODR_32KHZ   = 0b0001,
	ICM42688P_GYRO_ODR_16KHZ   = 0b0010,
	ICM42688P_GYRO_ODR_8KHZ    = 0b0011,
	ICM42688P_GYRO_ODR_4KHZ    = 0b0100,
	ICM42688P_GYRO_ODR_2KHZ    = 0b0101,
	ICM42688P_GYRO_ODR_1KHZ    = 0b0110,
	ICM42688P_GYRO_ODR_200HZ   = 0b0111,
	ICM42688P_GYRO_ODR_100HZ   = 0b1000,
	ICM42688P_GYRO_ODR_50HZ    = 0b1001,
	ICM42688P_GYRO_ODR_25HZ    = 0b1010,
	ICM42688P_GYRO_ODR_12_5HZ  = 0b1011,
	ICM42688P_GYRO_ODR_500HZ   = 0b1111,
}ICM42688P_GyroODR_t;

/* Power Mode for Accelerometer */
typedef enum{
	ICM42688P_ACCEL_MODE_OFF   = 0b00,
	ICM42688P_ACCEL_MODE_OFF2  = 0b01,
	ICM42688P_ACCEL_MODE_LP    = 0b10,
	ICM42688P_ACCEL_MODE_LN    = 0b11,
}ICM42688P_AccelMode_t;

/* Power Mode for Gyroscope */
typedef enum{
	ICM42688P_GYRO_MODE_OFF      = 0b00,
	ICM42688P_GYRO_MODE_STANDBY  = 0b01,
	ICM42688P_GYRO_MODE_LN       = 0b11,
}ICM42688P_GyroMode_t;

/* Accelerometer Anti-Alias Filter Bandwidth */
typedef enum{
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_2   = 0,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_4   = 1,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_5   = 2,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_8   = 3,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_10  = 4,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_16  = 5,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_20  = 6,
	ICM42688P_ACCEL_UI_FILT_BW_LPF_ODR_40  = 7,
}ICM42688P_AccelUIFiltBW_t;

/* Gyroscope UI Filter Bandwidth */
typedef enum{
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_2   = 0,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_4   = 1,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_5   = 2,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_8   = 3,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_10  = 4,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_16  = 5,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_20  = 6,
	ICM42688P_GYRO_UI_FILT_BW_LPF_ODR_40  = 7,
}ICM42688P_GyroUIFiltBW_t;

#pragma pack(1)
typedef struct{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;

	struct{
		uint8_t WHO_AM_I;     /* 0x75: WHO_AM_I (expected 0x47 or 0x6C) */

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
			int16_t GYRO_X;
			int16_t GYRO_Y;
			int16_t GYRO_Z;
		}Data;

		union{
			struct{
				uint8_t 			: 2;
				uint8_t INT1_DRIVE	: 1;
				uint8_t INT1_POLARITY	: 1;
				uint8_t INT1_MODE	: 1;
				uint8_t INT2_DRIVE	: 1;
				uint8_t INT2_POLARITY	: 1;
				uint8_t INT2_MODE	: 1;
			};

			uint8_t Byte;
		}INT_CONFIG;          /* 0x14 */

		union{
			struct{
				ICM42688P_AccelMode_t ACCEL_MODE	: 2;
				ICM42688P_GyroMode_t  GYRO_MODE		: 2;
				uint8_t GYRO_IDLE				: 1;
				uint8_t TEMP_DIS				: 1;
				uint8_t							: 2;
			};

			uint8_t Byte;
		}PWR_MGMT0;           /* 0x4E */

		union{
			struct{
				ICM42688P_GyroODR_t GYRO_ODR	: 4;
				uint8_t							: 1;
				ICM42688P_GyroFS_t  GYRO_FS_SEL	: 3;
			};

			uint8_t Byte;
		}GYRO_CONFIG0;        /* 0x4F */

		union{
			struct{
				ICM42688P_AccelODR_t ACCEL_ODR	: 4;
				uint8_t							: 1;
				ICM42688P_AccelFS_t  ACCEL_FS_SEL	: 3;
			};

			uint8_t Byte;
		}ACCEL_CONFIG0;       /* 0x50 */

		union{
			struct{
				ICM42688P_GyroUIFiltBW_t  GYRO_UI_FILT_BW	: 4;
				ICM42688P_AccelUIFiltBW_t ACCEL_UI_FILT_BW	: 4;
			};

			uint8_t Byte;
		}GYRO_ACCEL_CONFIG0;  /* 0x52 */

		union{
			struct{
				uint8_t SPI_MODE		: 1;
				uint8_t SPI_AP_4WIRE	: 1;
				uint8_t					: 2;
				uint8_t SOFT_RESET_CONFIG	: 1;
				uint8_t					: 3;
			};

			uint8_t Byte;
		}DEVICE_CONFIG;       /* 0x11 */

	}Register;

	double Temperature;
	double DataRate;

	oVector3_t Acceleration;  /* uG */
	oVector3_t GyroRate;      /* mdps */
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

}ICM42688P_t;
#pragma pack()

extern oResult_t ICM42688P_GetStatus(ICM42688P_t *pICM);
extern oResult_t ICM42688P_GetData(ICM42688P_t *pICM);
extern oResult_t ICM42688P_GetRPY(ICM42688P_t *pICM, uint8_t Count, oRPY_t *pAngle);
extern oResult_t ICM42688P_SetConfig(ICM42688P_t *pICM);
extern oResult_t ICM42688P_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, ICM42688P_t *pICM);
extern oResult_t ICM42688P_Close(ICM42688P_t *pICM);

#endif /* INC_MEMS_ICM42688P_H_ */

/* History

2026-06-26 | v0.1
	- baseline (MEMS_ICM42688P.h)

2026-10-20 | v0.2
	- CS 인자 값 전달 통일 - CS.Port == NULL 이면 CS 없음으로 보고 Open 에서 거부하지 않으며, CS 토글 지점은 Port NULL 이면 건너뜀
*/
