/*
 * MEMS_IIS3DWBTR.h
 *
 *  Created on: 2026-09-16
 *      Author: JONE
 *
 ************************************************************
 * ST IIS3DWB 3-Axis 광대역 MEMS 가속도계 (진동 계측 전용)
 *  - 3축 가속도 + 온도 (자이로 없음)
 *  - SPI (≤10 MHz, 4-wire) / I2C (≤1 MHz)
 *  - 16-bit 데이터, 단일 ODR 26.667 kHz (6.3 kHz 대역폭)
 *  - WHO_AM_I = 0x7B
 *  - 3 kB FIFO (512 word) — ISM330DHCX 와 동일한 tag+6byte entry 포맷
 *
 * Accelerometer Sensitivity:
 *   ±2 g  → 0.061 mg/LSB
 *   ±4 g  → 0.122 mg/LSB
 *   ±8 g  → 0.244 mg/LSB
 *   ±16 g → 0.488 mg/LSB
 *
 *   감도 공차 ±2 %, 온도에 따른 감도 변화 ±1 %(typ) / ±2 %(max)
 *
 * Zero-g level offset: ±60 mg(typ) / ±180 mg(max) @25°C, 온도계수 ±1 mg/°C
 *   -> 기울기 환산 typ 3.44 도. 기울기 측정에 쓰면 안 된다 (ISM330 은 ±10 mg = 0.57 도).
 *      가속도 크기 게이팅의 1 g 기준도 이 소자로는 세울 수 없다
 *
 * 노이즈 밀도 75 µg/√Hz(typ) / 110(max), 소비전류 1.1 mA(typ) @26.667 kHz
 * ODR 정확도 ±1 %(typ) / ±2 %(max), 온도계수 ±0.03 %/°C
 * Temperature: T(°C) = TEMP_OUT / 256 + 25  (TSen 256 LSB/°C)
 *
 * 레지스터 맵·SPI 프로토콜은 같은 LSM6DS 계열인 ISM330DHCX 와 공통이다.
 * 계열 공통이 아닌 IIS3DWB 고유 항목:
 *   1) ODR 단일값 — CTRL1_XL[7:5] = XL_EN[2:0], 000=power-down / 101=enabled 만 유효
 *   2) 자이로 관련 레지스터(CTRL2_G·OUTX_L_G·BDR_GY) 부재
 *   3) CTRL1_XL bit1 = LPF2_XL_EN (0=1단 필터 출력, 1=LPF2 출력)
 *   4) CTRL1_XL bit4·bit0 은 반드시 0 이어야 한다
 *
 * 출처: ST DS12569 Rev 8 — WHO_AM_I·감도·FS 순서·비트 배치 전부 대조 완료
 *************************************************************
 */

#ifndef INC_MEMS_IIS3DWBTR_H_
#define INC_MEMS_IIS3DWBTR_H_

#define MEMS_IIS3DWBTR_VERSION               0.4

#include "ONE_Common.h"
#include "ONE_Signal.h"
#include "ONE_Math.h"
#include "main.h"

#define IIS3DWB_WHO_AM_I_VALUE               0x7B

/* SPI 1st byte: bit7 = R/W (1=Read, 0=Write), bit6:0 = register address */
#define IIS3DWB_SPI_READ                     0x80
#define IIS3DWB_SPI_WRITE                    0x00

/* Data conversion */
#define IIS3DWB_DATACONVERT_16BIT(msb, lsb)  ((int16_t)((uint16_t)(msb) << 8 | (uint16_t)(lsb)))

/* Accelerometer raw → µG (micro-g) */
#define IIS3DWB_ACC_TO_2G_uG(raw)            ((double)(raw) * 61.0)
#define IIS3DWB_ACC_TO_4G_uG(raw)            ((double)(raw) * 122.0)
#define IIS3DWB_ACC_TO_8G_uG(raw)            ((double)(raw) * 244.0)
#define IIS3DWB_ACC_TO_16G_uG(raw)           ((double)(raw) * 488.0)

/* Temperature raw → °C */
#define IIS3DWB_TEMP_TO_Degree(raw)          ((double)(raw) / 256.0 + 25.0)

/* 단일 출력 레이트 — FFT 분해능·샘플 수 계산에 쓰인다 */
#define IIS3DWB_ODR_HZ                       26667.0

/* ───────── Register Addresses ───────── */
#define IIS3DWB_REG_FUNC_CFG_ACCESS          0x01
#define IIS3DWB_REG_FIFO_CTRL1               0x07
#define IIS3DWB_REG_FIFO_CTRL2               0x08
#define IIS3DWB_REG_FIFO_CTRL3               0x09
#define IIS3DWB_REG_FIFO_CTRL4               0x0A
#define IIS3DWB_REG_INT1_CTRL                0x0D
#define IIS3DWB_REG_INT2_CTRL                0x0E
#define IIS3DWB_REG_WHO_AM_I                 0x0F
#define IIS3DWB_REG_CTRL1_XL                 0x10
#define IIS3DWB_REG_CTRL3_C                  0x12
#define IIS3DWB_REG_CTRL4_C                  0x13
#define IIS3DWB_REG_CTRL5_C                  0x14
#define IIS3DWB_REG_CTRL6_C                  0x15
#define IIS3DWB_REG_CTRL7_C                  0x16
#define IIS3DWB_REG_CTRL8_XL                 0x17
#define IIS3DWB_REG_CTRL10_C                 0x19
#define IIS3DWB_REG_STATUS                   0x1E
#define IIS3DWB_REG_OUT_TEMP_L               0x20
#define IIS3DWB_REG_OUTX_L_A                 0x28
#define IIS3DWB_REG_FIFO_STATUS1             0x3A
#define IIS3DWB_REG_FIFO_DATA_OUT_TAG        0x78

/* ───────── FIFO Mode (FIFO_CTRL4[2:0]) ───────── */
typedef enum{
	IIS3DWB_FIFO_MODE_BYPASS          = 0,  /* FIFO 미사용 */
	IIS3DWB_FIFO_MODE_FIFO            = 1,  /* 가득 차면 정지 */
	IIS3DWB_FIFO_MODE_CONT_TO_FIFO    = 3,  /* Continuous → FIFO (이벤트 후 보존) */
	IIS3DWB_FIFO_MODE_BYPASS_TO_CONT  = 4,
	IIS3DWB_FIFO_MODE_CONTINUOUS      = 6,  /* 링버퍼 — 오래된 것 덮어쓰기 (PHM 권장) */
	IIS3DWB_FIFO_MODE_BYPASS_TO_FIFO  = 7,
}IIS3DWB_FifoMode_t;

/* ───────── Batch Data Rate (FIFO_CTRL3[3:0] BDR_XL) ─────────
 * 자이로가 없으므로 상위 니블(BDR_GY)은 0 고정 */
typedef enum{
	IIS3DWB_BDR_OFF      = 0b0000,
	IIS3DWB_BDR_26_667K  = 0b1010,  /* 유일한 유효 배치 레이트 — ODR 과 동일 */
}IIS3DWB_BDR_t;

/* ───────── FIFO TAG (entry 식별, TAG byte의 bit[7:3]) ───────── */
#define IIS3DWB_TAG_ACCEL_NC                 0x02
#define IIS3DWB_TAG_TEMP                     0x03
#define IIS3DWB_TAG_TIMESTAMP                0x04
#define IIS3DWB_TAG_CFG_CHANGE               0x05

#define IIS3DWB_FIFO_ENTRY_SIZE              7  /* tag 1 byte + data 6 byte */
#define IIS3DWB_FIFO_GET_TAG(x)              (((x) >> 3) & 0x1F)
#define IIS3DWB_FIFO_MAX_ENTRY               512

/* ───────── Accelerometer ODR (CTRL1_XL[7:5]) ─────────
 * 광대역 전용 소자라 레이트 선택지가 없다 — OFF 아니면 26.667 kHz */
typedef enum{
	IIS3DWB_XL_ODR_OFF      = 0b000,
	IIS3DWB_XL_ODR_26_667K  = 0b101,
}IIS3DWB_AccelODR_t;

/* Accelerometer FS (CTRL1_XL[3:2]) — ST 특이 순서 주의 */
typedef enum{
	IIS3DWB_XL_FS_2G   = 0b00,
	IIS3DWB_XL_FS_16G  = 0b01,
	IIS3DWB_XL_FS_4G   = 0b10,
	IIS3DWB_XL_FS_8G   = 0b11,
}IIS3DWB_AccelFS_t;

/* Accelerometer HPF/LPF2 cutoff (CTRL8_XL[7:5] HPCF_XL) — ODR 대비 분주 */
typedef enum{
	IIS3DWB_HPCF_XL_ODR_4    = 0b000,  /* ODR/4   */
	IIS3DWB_HPCF_XL_ODR_10   = 0b001,  /* ODR/10  */
	IIS3DWB_HPCF_XL_ODR_20   = 0b010,  /* ODR/20  */
	IIS3DWB_HPCF_XL_ODR_45   = 0b011,  /* ODR/45  */
	IIS3DWB_HPCF_XL_ODR_100  = 0b100,  /* ODR/100 */
	IIS3DWB_HPCF_XL_ODR_200  = 0b101,  /* ODR/200 */
	IIS3DWB_HPCF_XL_ODR_400  = 0b110,  /* ODR/400 */
	IIS3DWB_HPCF_XL_ODR_800  = 0b111,  /* ODR/800 — 최저 cutoff */
}IIS3DWB_HpcfXL_t;

#pragma pack(1)
typedef struct{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;                                        /* CS 핀 (값 — ISM330DHCX 패턴) */

	struct{
		uint8_t WHO_AM_I;                            /* 0x0F: WHO_AM_I (expected 0x7B) */

		union{
			struct{
				uint8_t XLDA	: 1;                 /* Accel data ready */
				uint8_t			: 1;
				uint8_t TDA		: 1;              /* Temp data ready */
				uint8_t			: 5;
			};
			uint8_t Byte;
		}STATUS;                                     /* 0x1E */

		struct{
			int16_t TEMP;                            /* 0x20-21 */
			int16_t ACCEL_X;                         /* 0x28-29 */
			int16_t ACCEL_Y;                         /* 0x2A-2B */
			int16_t ACCEL_Z;                         /* 0x2C-2D */
		}Data;

		union{
			struct{
				uint8_t					: 1;
				uint8_t LPF2_XL_EN		: 1;       /* bit 1: LPF2 경로 사용 (IIS3DWB 고유) */
				IIS3DWB_AccelFS_t  FS_XL	: 2;     /* [3:2] full-scale */
				uint8_t					: 1;
				IIS3DWB_AccelODR_t ODR_XL	: 3;    /* [7:5] ODR — 3bit 임에 주의 */
			};
			uint8_t Byte;
		}CTRL1_XL;                                   /* 0x10 */

		union{
			struct{
				uint8_t SW_RESET		: 1;
				uint8_t					: 1;
				uint8_t IF_INC			: 1;       /* auto-increment (default 1) */
				uint8_t SIM				: 1;      /* SPI 3-wire (0=4-wire) */
				uint8_t PP_OD			: 1;        /* push-pull / open-drain INT */
				uint8_t H_LACTIVE		: 1;
				uint8_t BDU				: 1;      /* block data update */
				uint8_t BOOT			: 1;
			};
			uint8_t Byte;
		}CTRL3_C;                                    /* 0x12 */

		union{
			struct{
				uint8_t					: 1;
				uint8_t				 	: 1;
				uint8_t I2C_disable		: 1;
				uint8_t DRDY_MASK		: 1;
				uint8_t					: 1;
				uint8_t INT2_on_INT1	: 1;
				uint8_t					: 2;
			};
			uint8_t Byte;
		}CTRL4_C;                                    /* 0x13 */

		union{
			struct{
				uint8_t LOW_PASS_ON_6D		: 1;
				uint8_t						: 1;
				uint8_t HP_SLOPE_XL_EN		: 1;   /* 1 = HPF, 0 = LPF2 */
				uint8_t FASTSETTL_MODE_XL	: 1;
				uint8_t HP_REF_MODE_XL		: 1;   /* 1 = auto reset DC reference */
				uint8_t HPCF_XL				: 3;  /* HPF/LPF2 cutoff: 0~7 (ODR/4 ~ ODR/800) */
			};
			uint8_t Byte;
		}CTRL8_XL;                                   /* 0x17 */

	}Register;

	double Temperature;

	oVector3_t Acceleration;                         /* µG */

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfError;
		uint8_t CountOfSpiError;                     /* HAL_ERROR 누적 → recovery 트리거 */
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}IIS3DWB_t;
#pragma pack()

extern oResult_t IIS3DWB_WhoAmI(IIS3DWB_t *pDev, uint8_t *pId);
extern oResult_t IIS3DWB_GetStatus(IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_GetData(IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_SetConfig(IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_SoftReset(IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_Close(IIS3DWB_t *pDev);
extern oResult_t IIS3DWB_Recovery(IIS3DWB_t *pDev);

/* FIFO API */
extern oResult_t IIS3DWB_FIFO_Setup(IIS3DWB_t *pDev, IIS3DWB_FifoMode_t Mode,
                                    uint16_t Watermark, IIS3DWB_BDR_t BDR_XL);
extern oResult_t IIS3DWB_FIFO_GetUnreadCount(IIS3DWB_t *pDev, uint16_t *pCount);
extern oResult_t IIS3DWB_FIFO_ReadBurst(IIS3DWB_t *pDev, uint8_t *pBuf, uint16_t EntryCount);
extern oResult_t IIS3DWB_FIFO_EnableINT1(IIS3DWB_t *pDev, uint8_t EnableThreshold, uint8_t EnableOverrun);
extern double    IIS3DWB_FIFO_ConvertAccel(IIS3DWB_t *pDev, const uint8_t *pData6, double *pAccelXYZ);

#endif /* INC_MEMS_IIS3DWBTR_H_ */

/* History

2026-09-16 | v0.1
	- baseline (MEMS_IIS3DWBTR.h)

2026-09-16 | v0.2
	- SoftReset 후 CTRL3_C.SW_RESET 셰도우 클리어 (.c) — 셰도우가 1 로 남으면 SetConfig 의
	  CTRL3_C 쓰기가 소프트리셋을 재발동시켜 직후 CTRL4_C·CTRL8_XL 쓰기가 소실된다
	- 주요 구간 oSerial_Log 추가 (.c, 태그 "IIS3DW") — Open·SoftReset·SetConfig·FIFO_Setup·
	  INT1·Close·Recovery. 레지스터 접근 함수와 SoftReset 폴링 루프는 제외 (26.7 kHz 배출
	  경로 범람 + 50 ms 리셋 대기 예산을 시리얼 전송이 잠식)

2026-09-17 | v0.3
	- 데이터시트(DS12569 Rev 8) 대조 완료 — WHO_AM_I·감도·FS 순서·CTRL1_XL 비트 배치가
	  모두 추정대로였다. "미확인" 표기 해제
	- zero-g offset ±60/±180 mg 기재 — 실측 크기 편차 5.1 %(51 mg)의 원인이며,
	  기울기 측정과 가속도 크기 게이팅에 이 소자를 쓸 수 없는 근거다

2026-10-20 | v0.4
	- CS 인자 값 전달 통일 - CS.Port == NULL 이면 CS 없음으로 보고 Open 에서 거부하지 않으며, CS 토글 지점은 Port NULL 이면 건너뜀
*/
