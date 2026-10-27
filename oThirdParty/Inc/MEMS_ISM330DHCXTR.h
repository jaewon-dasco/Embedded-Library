/*
 * MEMS_ISM330DHCXTR.h
 *
 *  Created on: 2026-06-23
 *      Author: JONE
 *
 ************************************************************
 * ST ISM330DHCX 6-Axis MEMS IMU (산업용 condition monitoring)
 *  - 3축 가속도 + 3축 자이로 + 온도
 *  - SPI (≤10 MHz, 4-wire) / I2C (≤1 MHz) / MIPI I3C
 *  - 16-bit 데이터
 *  - WHO_AM_I = 0x6B
 *
 * Accelerometer Sensitivity:
 *   ±2 g  → 0.061 mg/LSB
 *   ±4 g  → 0.122 mg/LSB
 *   ±8 g  → 0.244 mg/LSB
 *   ±16 g → 0.488 mg/LSB
 *
 * Gyroscope Sensitivity:
 *   ±125 dps  → 4.375 mdps/LSB
 *   ±250 dps  → 8.75  mdps/LSB
 *   ±500 dps  → 17.50 mdps/LSB
 *   ±1000 dps → 35    mdps/LSB
 *   ±2000 dps → 70    mdps/LSB
 *   ±4000 dps → 140   mdps/LSB
 *
 * Zero-g level offset:  ±10 mg(typ) / ±65 mg(max) @25°C,  온도계수 ±0.1 mg/°C(typ)
 *   -> 기울기 환산 typ 0.57 도. IIS3DWB(±60 mg = 3.44 도)보다 6 배 정확해 기울기는 이 소자가 맡는다
 *
 * Angular rate zero-rate level:  ±1 dps(typ) / ±3 dps(max)
 *   온도계수 ±0.005 dps/°C(typ) / ±0.015(max)
 *   -> 40 °C 스윙이면 typ 0.2 dps, max 0.6 dps 로 바이어스 전체와 맞먹는다.
 *      정지 중 동적 바이어스 교정(ONE_IMU)이 선택이 아니라 필수인 근거다
 *
 * Temperature: T(°C) = TEMP_OUT / 256 + 25
 *
 * 출처: ST ISM330DHCX 데이터시트
 *************************************************************
 */

#ifndef INC_MEMS_ISM330DHCXTR_H_
#define INC_MEMS_ISM330DHCXTR_H_

#define MEMS_ISM330DHCXTR_VERSION               0.8

#include "ONE_Common.h"
#include "ONE_Signal.h"
#include "ONE_Math.h"
#include "main.h"

#define ISM330DHCX_WHO_AM_I_VALUE               0x6B

/* SPI 1st byte: bit7 = R/W (1=Read, 0=Write), bit6:0 = register address */
#define ISM330DHCX_SPI_READ                     0x80
#define ISM330DHCX_SPI_WRITE                    0x00

/* Data conversion */
#define ISM330DHCX_DATACONVERT_16BIT(msb, lsb)  ((int16_t)((uint16_t)(msb) << 8 | (uint16_t)(lsb)))

/* Accelerometer raw → µG (micro-g) */
#define ISM330DHCX_ACC_TO_2G_uG(raw)            ((double)(raw) * 61.0)
#define ISM330DHCX_ACC_TO_4G_uG(raw)            ((double)(raw) * 122.0)
#define ISM330DHCX_ACC_TO_8G_uG(raw)            ((double)(raw) * 244.0)
#define ISM330DHCX_ACC_TO_16G_uG(raw)           ((double)(raw) * 488.0)

/* Gyroscope raw → mdps */
#define ISM330DHCX_GYRO_TO_125DPS(raw)          ((double)(raw) * 4.375)
#define ISM330DHCX_GYRO_TO_250DPS(raw)          ((double)(raw) * 8.75)
#define ISM330DHCX_GYRO_TO_500DPS(raw)          ((double)(raw) * 17.50)
#define ISM330DHCX_GYRO_TO_1000DPS(raw)         ((double)(raw) * 35.0)
#define ISM330DHCX_GYRO_TO_2000DPS(raw)         ((double)(raw) * 70.0)
#define ISM330DHCX_GYRO_TO_4000DPS(raw)         ((double)(raw) * 140.0)

/* Temperature raw → °C */
#define ISM330DHCX_TEMP_TO_Degree(raw)          ((double)(raw) / 256.0 + 25.0)

/* ───────── Register Addresses ───────── */
#define ISM330DHCX_REG_FUNC_CFG_ACCESS          0x01
#define ISM330DHCX_REG_FIFO_CTRL1               0x07
#define ISM330DHCX_REG_FIFO_CTRL2               0x08
#define ISM330DHCX_REG_FIFO_CTRL3               0x09
#define ISM330DHCX_REG_FIFO_CTRL4               0x0A
#define ISM330DHCX_REG_WHO_AM_I                 0x0F
#define ISM330DHCX_REG_CTRL1_XL                 0x10
#define ISM330DHCX_REG_CTRL2_G                  0x11
#define ISM330DHCX_REG_CTRL3_C                  0x12
#define ISM330DHCX_REG_CTRL4_C                  0x13
#define ISM330DHCX_REG_CTRL5_C                  0x14
#define ISM330DHCX_REG_CTRL6_C                  0x15
#define ISM330DHCX_REG_CTRL7_G                  0x16
#define ISM330DHCX_REG_CTRL8_XL                 0x17
#define ISM330DHCX_REG_CTRL9_XL                 0x18
#define ISM330DHCX_REG_CTRL10_C                 0x19
#define ISM330DHCX_REG_STATUS                   0x1E
#define ISM330DHCX_REG_OUT_TEMP_L               0x20
#define ISM330DHCX_REG_OUTX_L_G                 0x22
#define ISM330DHCX_REG_OUTX_L_A                 0x28
#define ISM330DHCX_REG_FIFO_STATUS1             0x3A
#define ISM330DHCX_REG_FIFO_DATA_OUT_TAG        0x78

/* ───────── FIFO Mode (FIFO_CTRL4[2:0]) ───────── */
typedef enum{
	ISM330DHCX_FIFO_MODE_BYPASS          = 0,  /* FIFO 미사용 */
	ISM330DHCX_FIFO_MODE_FIFO            = 1,  /* 가득 차면 정지 */
	ISM330DHCX_FIFO_MODE_CONT_TO_FIFO    = 3,  /* Continuous → FIFO (이벤트 후 보존) */
	ISM330DHCX_FIFO_MODE_BYPASS_TO_CONT  = 4,
	ISM330DHCX_FIFO_MODE_CONTINUOUS      = 6,  /* 링버퍼 — 오래된 것 덮어쓰기 (PHM 권장) */
	ISM330DHCX_FIFO_MODE_BYPASS_TO_FIFO  = 7,
}ISM330DHCX_FifoMode_t;

/* ───────── Batch Data Rate (FIFO_CTRL3·CTRL4) ───────── */
typedef enum{
	ISM330DHCX_BDR_OFF      = 0b0000,
	ISM330DHCX_BDR_12_5HZ   = 0b0001,
	ISM330DHCX_BDR_26HZ     = 0b0010,
	ISM330DHCX_BDR_52HZ     = 0b0011,
	ISM330DHCX_BDR_104HZ    = 0b0100,
	ISM330DHCX_BDR_208HZ    = 0b0101,
	ISM330DHCX_BDR_416HZ    = 0b0110,
	ISM330DHCX_BDR_833HZ    = 0b0111,
	ISM330DHCX_BDR_1_66KHZ  = 0b1000,
	ISM330DHCX_BDR_3_33KHZ  = 0b1001,
	ISM330DHCX_BDR_6_66KHZ  = 0b1010,
}ISM330DHCX_BDR_t;

/* ───────── FIFO TAG (entry 식별, TAG byte의 bit[7:3]) ───────── */
#define ISM330DHCX_TAG_GYRO_NC                  0x01
#define ISM330DHCX_TAG_ACCEL_NC                 0x02
#define ISM330DHCX_TAG_TEMP                     0x03
#define ISM330DHCX_TAG_TIMESTAMP                0x04
#define ISM330DHCX_TAG_CFG_CHANGE               0x05
#define ISM330DHCX_TAG_ACCEL_COMP               0x06
#define ISM330DHCX_TAG_GYRO_COMP                0x07
#define ISM330DHCX_TAG_MLC                      0x12

#define ISM330DHCX_FIFO_ENTRY_SIZE              7  /* tag 1 byte + data 6 byte */
#define ISM330DHCX_FIFO_GET_TAG(x)              (((x) >> 3) & 0x1F)

/* ───────── Accelerometer ODR (CTRL1_XL[7:4]) ───────── */
typedef enum{
	ISM330DHCX_XL_ODR_OFF       = 0b0000,
	ISM330DHCX_XL_ODR_12_5HZ    = 0b0001,
	ISM330DHCX_XL_ODR_26HZ      = 0b0010,
	ISM330DHCX_XL_ODR_52HZ      = 0b0011,
	ISM330DHCX_XL_ODR_104HZ     = 0b0100,
	ISM330DHCX_XL_ODR_208HZ     = 0b0101,
	ISM330DHCX_XL_ODR_416HZ     = 0b0110,
	ISM330DHCX_XL_ODR_833HZ     = 0b0111,
	ISM330DHCX_XL_ODR_1_66KHZ   = 0b1000,
	ISM330DHCX_XL_ODR_3_33KHZ   = 0b1001,
	ISM330DHCX_XL_ODR_6_66KHZ   = 0b1010,
	ISM330DHCX_XL_ODR_1_6HZ_LP  = 0b1011,
}ISM330DHCX_AccelODR_t;

/* Accelerometer FS (CTRL1_XL[3:2]) — ST 특이 순서 주의 */
typedef enum{
	ISM330DHCX_XL_FS_2G   = 0b00,
	ISM330DHCX_XL_FS_16G  = 0b01,
	ISM330DHCX_XL_FS_4G   = 0b10,
	ISM330DHCX_XL_FS_8G   = 0b11,
}ISM330DHCX_AccelFS_t;

/* Accelerometer HPF/LPF2 cutoff (CTRL8_XL[7:5] HPCF_XL) — ODR 대비 분주 */
typedef enum{
	ISM330DHCX_HPCF_XL_ODR_4    = 0b000,  /* ODR/4   */
	ISM330DHCX_HPCF_XL_ODR_10   = 0b001,  /* ODR/10  */
	ISM330DHCX_HPCF_XL_ODR_20   = 0b010,  /* ODR/20  */
	ISM330DHCX_HPCF_XL_ODR_45   = 0b011,  /* ODR/45  */
	ISM330DHCX_HPCF_XL_ODR_100  = 0b100,  /* ODR/100 */
	ISM330DHCX_HPCF_XL_ODR_200  = 0b101,  /* ODR/200 */
	ISM330DHCX_HPCF_XL_ODR_400  = 0b110,  /* ODR/400 */
	ISM330DHCX_HPCF_XL_ODR_800  = 0b111,  /* ODR/800 — 최저 cutoff */
}ISM330DHCX_HpcfXL_t;

/* Gyroscope ODR (CTRL2_G[7:4]) */
typedef enum{
	ISM330DHCX_G_ODR_OFF      = 0b0000,
	ISM330DHCX_G_ODR_12_5HZ   = 0b0001,
	ISM330DHCX_G_ODR_26HZ     = 0b0010,
	ISM330DHCX_G_ODR_52HZ     = 0b0011,
	ISM330DHCX_G_ODR_104HZ    = 0b0100,
	ISM330DHCX_G_ODR_208HZ    = 0b0101,
	ISM330DHCX_G_ODR_416HZ    = 0b0110,
	ISM330DHCX_G_ODR_833HZ    = 0b0111,
	ISM330DHCX_G_ODR_1_66KHZ  = 0b1000,
	ISM330DHCX_G_ODR_3_33KHZ  = 0b1001,
	ISM330DHCX_G_ODR_6_66KHZ  = 0b1010,
}ISM330DHCX_GyroODR_t;

/* Gyroscope FS — CTRL2_G[3:1] 조합:
 *   FS_G[3:2] + FS_125 (bit 1)
 *   0b00x | FS_125=1 → ±125 dps
 *   0b00x | FS_125=0 → ±250 dps
 *   0b01x → ±500 dps
 *   0b10x → ±1000 dps
 *   0b11x → ±2000 dps
 *   별도 CTRL6_C.FS_4000=1 → ±4000 dps (FS_G·FS_125 무시)
 */
typedef enum{
	ISM330DHCX_G_FS_250DPS   = 0,
	ISM330DHCX_G_FS_500DPS   = 1,
	ISM330DHCX_G_FS_1000DPS  = 2,
	ISM330DHCX_G_FS_2000DPS  = 3,
	ISM330DHCX_G_FS_125DPS   = 4,  /* FS_125 bit set */
	ISM330DHCX_G_FS_4000DPS  = 5,  /* CTRL6_C.FS_4000 set */
}ISM330DHCX_GyroFS_t;

#pragma pack(1)
typedef struct{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;                                            /* CS 핀 (값 — ICM42688P 패턴) */

	struct{
		uint8_t WHO_AM_I;                                /* 0x0F: WHO_AM_I (expected 0x6B) */

		union{
			struct{
				uint8_t XLDA	: 1;                     /* Accel data ready */
				uint8_t GDA		: 1;                  /* Gyro data ready */
				uint8_t TDA		: 1;                  /* Temp data ready */
				uint8_t			: 5;
			};
			uint8_t Byte;
		}STATUS;                                         /* 0x1E */

		struct{
			int16_t TEMP;                                /* 0x20-21 */
			int16_t GYRO_X;                              /* 0x22-23 */
			int16_t GYRO_Y;                              /* 0x24-25 */
			int16_t GYRO_Z;                              /* 0x26-27 */
			int16_t ACCEL_X;                             /* 0x28-29 */
			int16_t ACCEL_Y;                             /* 0x2A-2B */
			int16_t ACCEL_Z;                             /* 0x2C-2D */
		}Data;

		union{
			struct{
				uint8_t					: 1;          /* [0] reserved — 0 */
				uint8_t               LPF2_XL_EN: 1;     /* [1] LPF2 경로 사용 (차단은 CTRL8_XL.HPCF_XL) */
				ISM330DHCX_AccelFS_t  FS_XL		: 2;  /* [3:2] full-scale */
				ISM330DHCX_AccelODR_t ODR_XL	: 4;     /* [7:4] ODR */
			};
			uint8_t Byte;
		}CTRL1_XL;                                       /* 0x10 */

		union{
			struct{
				uint8_t					: 1;
				uint8_t FS_125			: 1;           /* bit 1: enable ±125 dps */
				uint8_t FS_G			: 2;             /* [3:2] full-scale */
				ISM330DHCX_GyroODR_t ODR_G	: 4;       /* [7:4] ODR */
			};
			uint8_t Byte;
		}CTRL2_G;                                        /* 0x11 */

		union{
			struct{
				uint8_t SW_RESET		: 1;
				uint8_t					: 1;
				uint8_t IF_INC			: 1;           /* auto-increment (default 1) */
				uint8_t SIM				: 1;          /* SPI 3-wire (0=4-wire) */
				uint8_t PP_OD			: 1;            /* push-pull / open-drain INT */
				uint8_t H_LACTIVE		: 1;
				uint8_t BDU				: 1;          /* block data update */
				uint8_t BOOT			: 1;
			};
			uint8_t Byte;
		}CTRL3_C;                                        /* 0x12 */

		union{
			struct{
				uint8_t				: 1;
				uint8_t LPF1_SEL_G	: 1;
				uint8_t I2C_disable	: 1;
				uint8_t DRDY_MASK	: 1;
				uint8_t				: 1;
				uint8_t INT2_on_INT1	: 1;
				uint8_t SLEEP_G		: 1;
				uint8_t				: 1;
			};
			uint8_t Byte;
		}CTRL4_C;                                        /* 0x13 */

		union{
			struct{
				uint8_t FTYPE_XL	: 3;
				uint8_t USR_OFF_W	: 1;
				uint8_t XL_HM_MODE	: 1;               /* High-perf mode (0=on) */
				uint8_t DEN_MODE	: 3;
			};
			uint8_t Byte;
		}CTRL6_C;                                        /* 0x15 */

		union{
			struct{
				uint8_t LOW_PASS_ON_6D		: 1;
				uint8_t						: 1;
				uint8_t HP_SLOPE_XL_EN		: 1;       /* 1 = HPF, 0 = LPF2 */
				uint8_t FASTSETTL_MODE_XL	: 1;
				uint8_t HP_REF_MODE_XL		: 1;       /* 1 = auto reset DC reference */
				uint8_t HPCF_XL				: 3;      /* HPF/LPF2 cutoff: 0~7 (ODR/4 ~ ODR/800) */
			};
			uint8_t Byte;
		}CTRL8_XL;                                       /* 0x17 */

	}Register;

	double Temperature;
	double DataRate;

	oVector3_t Acceleration;                             /* µG */
	oVector3_t GyroRate;                                 /* mdps */

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfError;
		uint8_t CountOfSpiError;                         /* HAL_ERROR 누적 → recovery 트리거 */
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}ISM330DHCX_t;
#pragma pack()

extern oResult_t ISM330DHCX_WhoAmI(ISM330DHCX_t *pDev, uint8_t *pId);
extern oResult_t ISM330DHCX_GetStatus(ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_GetData(ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_SetConfig(ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_SoftReset(ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_Close(ISM330DHCX_t *pDev);
extern oResult_t ISM330DHCX_Recovery(ISM330DHCX_t *pDev);

/* FIFO API */
extern oResult_t ISM330DHCX_FIFO_Setup(ISM330DHCX_t *pDev, ISM330DHCX_FifoMode_t Mode,
                                       uint16_t Watermark, ISM330DHCX_BDR_t BDR_XL, ISM330DHCX_BDR_t BDR_G);
extern oResult_t ISM330DHCX_FIFO_GetUnreadCount(ISM330DHCX_t *pDev, uint16_t *pCount);
extern oResult_t ISM330DHCX_FIFO_ReadBurst(ISM330DHCX_t *pDev, uint8_t *pBuf, uint16_t EntryCount);
extern oResult_t ISM330DHCX_FIFO_EnableINT1(ISM330DHCX_t *pDev, uint8_t EnableThreshold, uint8_t EnableOverrun);
extern double    ISM330DHCX_FIFO_ConvertAccel(ISM330DHCX_t *pDev, const uint8_t *pData6, double *pAccelXYZ);

#endif /* INC_MEMS_ISM330DHCXTR_H_ */

/* History

2026-06-26 | v0.1
	- baseline (MEMS_ISM330DHCXTR.h)

2026-09-16 | v0.2
	- SoftReset 후 CTRL3_C.SW_RESET 셰도우 클리어 (.c) — SetConfig 의 CTRL3_C 쓰기가
	  소프트리셋을 재발동시켜 직후 CTRL8_XL(HPCF_XL 등) 설정이 소실되던 결함

2026-09-16 | v0.3
	- 주요 구간 oSerial_Log 추가 (.c, 태그 "ISM330") — IIS3DWB 와 동일 정책.
	  Open·SoftReset·SetConfig·FIFO_Setup·Close·Recovery 만 로그하고 레지스터 접근
	  함수와 SoftReset 폴링 루프는 제외 (FIFO 배출 경로 범람·리셋 대기 예산 잠식 방지)

2026-09-16 | v0.4
	- Attitude(oIMU_6DOF_t) 필드와 ISM330DHCX_UpdateAttitude 추가

2026-09-17 | v0.5
	- v0.4 의 Attitude 필드·UpdateAttitude 철회. 본 구조체는 pack(1) 이라 double 멤버가
	  비정렬로 놓이고, 그 주소를 oMath_IMU_6DOF 에 넘기면 Cortex-M33 에서 폴트가 난다
	  (실기 확인). 융합 상태는 호출부가 정렬된 변수로 보유하고 Acceleration·GyroRate 를
	  값으로 넘긴다 — 값 전달은 컴파일러가 패킹을 알고 처리하므로 안전하다

2026-09-17 | v0.6
	- 데이터시트 규격을 헤더에 기재 — 가속도 zero-g offset ±10/±65 mg, 자이로 ZRO ±1/±3 dps
	  및 각 온도계수. 기울기를 본 소자가 맡는 근거와 동적 바이어스 교정 필요성의 근거다

2026-09-22 | v0.7
	- CTRL1_XL.LPF2_XL_EN(bit1) 노출. 비트 위치는 IIS3DWB(DS12569 대조 완료)와 공유하는
	  LSM6DS 계열 레지스터 맵 기준 — 본 소자 데이터시트로는 미대조다.
	  실기 확인법: 켜면 가속도 잡음이 눈에 띄게 줄어야 한다

2026-10-20 | v0.8
	- CS 인자 값 전달 통일 - CS.Port == NULL 이면 CS 없음으로 보고 Open 에서 거부하지 않으며, CS 토글 지점은 Port NULL 이면 건너뜀
*/
