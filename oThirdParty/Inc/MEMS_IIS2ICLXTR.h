/*
 * MEMS_IIS2ICLXTR.h
 *
 *  Created on: 2026-09-03
 *      Author: JONE
 *
 ************************************************************
 * ST IIS2ICLX 2-Axis High-Accuracy MEMS Inclinometer
 *  - X·Y 2축 가속도 + 온도 (Z축 없음 — 3축 부품과 달리 중력벡터 크기로 정규화 불가)
 *  - SPI 4-wire (≤10 MHz) / I2C — 본 드라이버는 SPI 전용
 *  - 16-bit 데이터, LSM6DSO 계열 레지스터 맵
 *
 * 출처: ST IIS2ICLX Datasheet DS13005 Rev 7 (원문 대조 완료 — 2026-09-09)
 *
 * 데이터시트 대조 결과 (DS13005 Rev 7)
 *   WHO_AM_I(0Fh)=6Bh(Table 36) · ODR 코드(Table 38) · FS 코드(Table 39) · 감도(Table 2) ·
 *   SPI mode 3 / 최대 10 MHz(Table 5) · Ton 15 ms(Table 4) 전부 일치 확인.
 *   FS 코드가 값 순서와 어긋나는 점에 주의 — 00=±0.5g, 01=±3g, 10=±1g, 11=±2g (Table 39).
 *   순서대로 매긴 코드를 쓰면 감도가 어긋나 각도가 조용히 틀린다.
 *   구조(트랜잭션·상태머신·Open/Close)는 MEMS_ISM330DHCXTR 검증본을 따랐고,
 *   비트 배치는 LSM6DSO 계열 ST 공식 드라이버(lsm6dso_reg.h)와도 일치한다.
 *
 * ⚠ 예약 주소 쓰기 금지 (DS13005 Rev 7 p.32 원문)
 *   "Reserved registers must not be changed. Writing to those registers may cause
 *    permanent damage to the device."
 *   권장이 아니라 하드웨어 파괴 조건이다. 그래서 이 드라이버의 모든 쓰기는
 *   레지스터 서술자(IIS2ICLX_RegInfo_t)를 통과해야 하고, 서술자가 RW 로 표기하지 않은
 *   주소는 전송 경로에 오를 방법이 없다. 이미지 통짜 쓰기 API 는 의도적으로 없다.
 *
 * ⚠ 하드웨어 요구사항 (DS13005 Rev 7 §9.3 · Table 16) — 보드 설계 시 확인
 *   1) INT1 은 전원 인가 시 '0' 이거나 미연결이어야 한다. 이 핀에 풀업 금지
 *   2) Mode 1(센서허브 미사용)에서 SDx(7번)·SCx(12번)는 GND 또는 VDD_IO 에 연결. 부유 금지
 *   3) SPI 사용 시 CTRL4_C.I2C_disable=1 과 CTRL9_XL.DEVICE_CONF=1 을 모두 설정해야 한다
 *      (SetConfig 가 수행 — DEVICE_CONF 는 소프트 리셋 후에도 다시 세워야 한다)
 *
 * SPI 트랜잭션 (LSM6DSO 계열 공통)
 *   1st byte = [bit7 R/W][bit6:0 레지스터 주소], R/W: 1=Read 0=Write
 *   CTRL3_C.IF_INC(기본 1) 상태에서 다중 바이트 접근 시 주소 자동 증가
 *   CS를 LOW로 내린 뒤 주소 → 데이터, 트랜잭션 종료 시 HIGH 복귀
 *
 * SPI 모드
 *   CPOL=1 / CPHA=1 (mode 3), MSB first, 8-bit. 클럭 상한 10 MHz
 *
 * Accelerometer Sensitivity (FS 별 mg/LSB)
 *   ±0.5 g → 0.015 mg/LSB
 *   ±1 g   → 0.031 mg/LSB
 *   ±2 g   → 0.061 mg/LSB
 *   ±3 g   → 0.122 mg/LSB
 *   경사계 용도에서는 최저 FS(±0.5 g)가 분해능이 가장 좋다 — 설치 기울기가
 *   ±30° 이내(sin30°=0.5 g)면 ±0.5 g로 충분하고 그 이상이면 포화한다
 *
 * Temperature: T(℃) = TEMP_OUT / 256 + 25
 *
 * 2축 경사각 산출 — 부착 방향에 따라 기하가 다르다 (Mount 필드로 선택)
 *   Z축이 없어 oMath_AccelationToRPY(3축 atan2)를 어느 경우에도 쓸 수 없다.
 *
 *   [HORIZONTAL] 보드면 수평 — 중력이 보드면에 수직이라 정지 시 X,Y 모두 0.
 *     Angle.Pitch = asin(X/1g), Angle.Roll = asin(Y/1g) — 두 축 독립 경사.
 *     두 축이 동시에 크게 기울면 근사가 벌어져 소각도 범위를 전제로 한다.
 *
 *   [VERTICAL_X / VERTICAL_Y] 보드면 수직 — 중력이 보드면 안에 있어 지정 축이 ±1 g 를 읽는다.
 *     중력 축을 기준으로 면내 경사를 atan2 로 구한다 (VERTICAL_Y 예):
 *       Angle.Pitch = atan2(X, |Y|)
 *     분모에 절댓값을 쓰는 이유는 +Y 가 위를 향하든 아래를 향하든 같은 결과를 내기 위함이다 —
 *     설치 극성에 무관해지고 부호는 X 가 결정한다 (범위 ±90°, IPI 실사용 ±10° 대비 충분).
 *     asin(X/1g) 대비 이점은 두 축을 함께 써서 감도 오차가 비(ratio)로 상쇄된다는 점이다.
 *     Angle.Roll 은 0 으로 둔다 — 면내 경사 축이 하나뿐이라 Roll 에 담을 값이 없다.
 *
 *     면외(보드면에 수직인 방향) 경사는 OutOfPlaneAngle 에 따로 담는다:
 *       OutOfPlaneAngle = acos(|a| / 1g) — 크기만, 부호 없음 (Z축이 없어 방향을 모른다)
 *
 *   GravityMagnitude — 측정 벡터 크기 |a| [µG]. 1 g 에서 벗어나면 정지 상태가 아니라
 *   동적 가속도가 섞인 것이므로 그 시점 각도는 신뢰할 수 없다. 유효성 판정용.
 *
 * 미구현
 *   FIFO — 정적 경사 측정은 저속 폴링으로 충분해 넣지 않았다. 진동 용도로 쓰려면
 *   FIFO_CTRL1~4(0x07~0x0A)·FIFO_STATUS(0x3A)·FIFO_DATA_OUT_TAG(0x78) 추가 필요
 *************************************************************
 */

#ifndef INC_MEMS_IIS2ICLXTR_H_
#define INC_MEMS_IIS2ICLXTR_H_

#define MEMS_IIS2ICLXTR_VERSION                0.2

#include <stddef.h>

#include "ONE_Common.h"
#include "ONE_Signal.h"
#include "ONE_Math.h"
#include "main.h"

#define IIS2ICLX_WHO_AM_I_VALUE                0x6B  /* DS13005 Rev 7 Table 36 대조 완료 */

/* SPI 1st byte: bit7 = R/W (1=Read, 0=Write), bit6:0 = register address */
#define IIS2ICLX_SPI_READ                      0x80
#define IIS2ICLX_SPI_WRITE                     0x00
#define IIS2ICLX_SPI_TIMEOUT                   100   /* HAL 타임아웃 [ms] */

#define IIS2ICLX_SPI_ERROR_RECOVERY_THRESHOLD  5     /* HAL_ERROR 연속 누적 → Recovery */
#define IIS2ICLX_SOFTRESET_SETTLE_MS           10    /* SW_RESET 후 내부 안정화 대기 */

/* Data conversion */
#define IIS2ICLX_DATACONVERT_16BIT(msb, lsb)   ((int16_t)((uint16_t)(msb) << 8 | (uint16_t)(lsb)))

/* Accelerometer raw → µG (micro-g) — mg/LSB × 1000 */
#define IIS2ICLX_ACC_TO_500MG_uG(raw)          ((double)(raw) * 15.0)
#define IIS2ICLX_ACC_TO_1G_uG(raw)             ((double)(raw) * 31.0)
#define IIS2ICLX_ACC_TO_2G_uG(raw)             ((double)(raw) * 61.0)
#define IIS2ICLX_ACC_TO_3G_uG(raw)             ((double)(raw) * 122.0)

/* Temperature raw → ℃ */
#define IIS2ICLX_TEMP_TO_Degree(raw)           ((double)(raw) / 256.0 + 25.0)

/* µG → 경사각[°] (중력 1 g = 1,000,000 µG 기준 asin) */
#define IIS2ICLX_uG_PER_G                      1000000.0
#define IIS2ICLX_uG_TO_DEGREE(ug)              RADIAN_TO_DEGREE(asin((ug) / IIS2ICLX_uG_PER_G))

/* ───────── Register Addresses ───────── */
#define IIS2ICLX_REG_FUNC_CFG_ACCESS           0x01
#define IIS2ICLX_REG_PIN_CTRL                  0x02
#define IIS2ICLX_REG_FIFO_CTRL1                0x07
#define IIS2ICLX_REG_FIFO_CTRL2                0x08
#define IIS2ICLX_REG_FIFO_CTRL3                0x09
#define IIS2ICLX_REG_FIFO_CTRL4                0x0A
#define IIS2ICLX_REG_COUNTER_BDR1              0x0B
#define IIS2ICLX_REG_COUNTER_BDR2              0x0C
#define IIS2ICLX_REG_INT1_CTRL                 0x0D
#define IIS2ICLX_REG_INT2_CTRL                 0x0E
#define IIS2ICLX_REG_WHO_AM_I                  0x0F
#define IIS2ICLX_REG_CTRL1_XL                  0x10
#define IIS2ICLX_REG_CTRL3_C                   0x12
#define IIS2ICLX_REG_CTRL4_C                   0x13
#define IIS2ICLX_REG_CTRL5_C                   0x14
#define IIS2ICLX_REG_CTRL6_C                   0x15
#define IIS2ICLX_REG_CTRL7_XL                  0x16
#define IIS2ICLX_REG_CTRL8_XL                  0x17
#define IIS2ICLX_REG_CTRL9_XL                  0x18
#define IIS2ICLX_CTRL9_DEVICE_CONF             0x02  /* CTRL9_XL bit1 — 초기화 시 1 필수 (DS13005 Rev 7) */
#define IIS2ICLX_REG_CTRL10_C                  0x19
#define IIS2ICLX_REG_ALL_INT_SRC               0x1A
#define IIS2ICLX_REG_STATUS                    0x1E
#define IIS2ICLX_REG_OUT_TEMP_L                0x20
#define IIS2ICLX_REG_OUTX_L_A                  0x28
#define IIS2ICLX_REG_OUTY_L_A                  0x2A
#define IIS2ICLX_REG_FIFO_STATUS1              0x3A
#define IIS2ICLX_REG_X_OFS_USR                 0x73
#define IIS2ICLX_REG_Y_OFS_USR                 0x74
#define IIS2ICLX_REG_FIFO_DATA_OUT_TAG         0x78

/* ───────── Accelerometer ODR (CTRL1_XL[7:4]) ───────── */
typedef enum{
	IIS2ICLX_XL_ODR_OFF     = 0b0000,
	IIS2ICLX_XL_ODR_12_5HZ  = 0b0001,
	IIS2ICLX_XL_ODR_26HZ    = 0b0010,
	IIS2ICLX_XL_ODR_52HZ    = 0b0011,
	IIS2ICLX_XL_ODR_104HZ   = 0b0100,
	IIS2ICLX_XL_ODR_208HZ   = 0b0101,
	IIS2ICLX_XL_ODR_416HZ   = 0b0110,
	IIS2ICLX_XL_ODR_833HZ   = 0b0111,
}IIS2ICLX_AccelODR_t;

/* Accelerometer FS (CTRL1_XL[3:2]) — ST 특이 순서 (오름차순 아님) 주의 */
typedef enum{
	IIS2ICLX_XL_FS_500MG  = 0b00,
	IIS2ICLX_XL_FS_3G     = 0b01,
	IIS2ICLX_XL_FS_1G     = 0b10,
	IIS2ICLX_XL_FS_2G     = 0b11,
}IIS2ICLX_AccelFS_t;

/* HPF/LPF2 cutoff (CTRL8_XL[7:5] HPCF_XL) — ODR 대비 분주 */
typedef enum{
	IIS2ICLX_HPCF_XL_ODR_4    = 0b000,  /* ODR/4   */
	IIS2ICLX_HPCF_XL_ODR_10   = 0b001,  /* ODR/10  */
	IIS2ICLX_HPCF_XL_ODR_20   = 0b010,  /* ODR/20  */
	IIS2ICLX_HPCF_XL_ODR_45   = 0b011,  /* ODR/45  */
	IIS2ICLX_HPCF_XL_ODR_100  = 0b100,  /* ODR/100 */
	IIS2ICLX_HPCF_XL_ODR_200  = 0b101,  /* ODR/200 */
	IIS2ICLX_HPCF_XL_ODR_400  = 0b110,  /* ODR/400 */
	IIS2ICLX_HPCF_XL_ODR_800  = 0b111,  /* ODR/800 — 최저 cutoff */
}IIS2ICLX_HpcfXL_t;

/* ───────── 부착 방향 (각도 환산 기하 선택) ───────── */
typedef enum{
	IIS2ICLX_MOUNT_HORIZONTAL  = 0,  /* 보드면 수평 — 정지 시 X,Y 모두 약 0 */
	IIS2ICLX_MOUNT_VERTICAL_X  = 1,  /* 보드면 수직 — 정지 시 |X| 가 약 1 g */
	IIS2ICLX_MOUNT_VERTICAL_Y  = 2,  /* 보드면 수직 — 정지 시 |Y| 가 약 1 g */
}IIS2ICLX_Mount_t;

/* ───────── Register Shadow Image ─────────
 * Image[] 가 정본이고, 아래 비트필드 오버레이는 "주소 = 배열 오프셋" 항등을 지킨다.
 * 항등이 깨지면 ReadReg/WriteReg 가 엉뚱한 레지스터를 건드리므로 파일 하단
 * IIS2ICLX_STATIC_ASSERT 가 빌드에서 막는다. 다만 런타임 경로는 이 항등을 가정하지 않고
 * 서술자의 ImageOffset 을 읽는다 — 항등이 성립하지 않는 IC(멀티바이트 레지스터 등)가
 * 같은 계층에 들어와도 인터페이스가 바뀌지 않게 하기 위함이다.
 *
 * 이미지 범위는 0x00~0x2B — 현행 드라이버가 만지는 전 레지스터를 덮는다.
 * TAP_CFG 계열(0x56~0x5F)·X/Y_OFS_USR(0x73·0x74)·FIFO(0x3A~)는 이미지 밖이며
 * 서술자 범주 IIS2ICLX_REGACCESS_OUTSIDE_IMAGE 로 명시된다 — 배열 밖으로 조용히
 * 쓰는 사고를 막기 위해 범주를 없애지 말고 기능 추가 시 이미지를 확장하라 */
#define IIS2ICLX_REG_IMAGE_SIZE                0x2C

#pragma pack(1)
typedef union{
	uint8_t Image[IIS2ICLX_REG_IMAGE_SIZE];         /* 정본 */

	struct{
		uint8_t Reserved00[0x0D];                   /* 0x00~0x0C */

		/* 0x0D — INT1 핀에 실을 신호 선택. 비트 배치는 ST lsm6dso_reg.h 와 대조 완료.
		   자이로가 없는 부품이라 INT1_DRDY_G(bit1)는 의미 없다 */
		union{
			struct{
				uint8_t INT1_DRDY_XL   : 1;         /* 1 = 가속도 data-ready 를 INT1 로 */
				uint8_t                : 1;
				uint8_t INT1_BOOT      : 1;
				uint8_t INT1_FIFO_TH   : 1;
				uint8_t INT1_FIFO_OVR  : 1;
				uint8_t INT1_FIFO_FULL : 1;
				uint8_t INT1_CNT_BDR   : 1;
				uint8_t DEN_DRDY_FLAG  : 1;
			};
			uint8_t Byte;
		}INT1_CTRL;                                 /* 0x0D */

		uint8_t INT2_CTRL;                          /* 0x0E — 미사용 (INT1 만 라우팅) */
		uint8_t WHO_AM_I;                           /* 0x0F (expected IIS2ICLX_WHO_AM_I_VALUE) */

		union{
			struct{
				uint8_t                       : 1;  /* 예약 (0 고정) */
				uint8_t LPF2_XL_EN            : 1;  /* bit1: LPF2 경로 사용 */
				IIS2ICLX_AccelFS_t  FS_XL     : 2;  /* [3:2] full-scale */
				IIS2ICLX_AccelODR_t ODR_XL    : 4;  /* [7:4] ODR */
			};
			uint8_t Byte;
		}CTRL1_XL;                                  /* 0x10 */

		uint8_t Reserved11;                         /* 0x11 — 예약 주소 */

		union{
			struct{
				uint8_t SW_RESET    : 1;            /* 자기클리어 — Sync diff 제외 */
				uint8_t             : 1;            /* 예약 (0 고정) */
				uint8_t IF_INC      : 1;            /* 주소 자동증가 (POR 기본 1) */
				uint8_t SIM         : 1;            /* SPI 3-wire (0=4-wire) */
				uint8_t PP_OD       : 1;            /* INT 핀 push-pull / open-drain */
				uint8_t H_LACTIVE   : 1;            /* INT active level */
				uint8_t BDU         : 1;            /* block data update */
				uint8_t BOOT        : 1;            /* 자기클리어 — Sync diff 제외 */
			};
			uint8_t Byte;
		}CTRL3_C;                                   /* 0x12 */

		union{
			struct{
				uint8_t                : 2;
				uint8_t I2C_disable    : 1;         /* 1 = I2C 비활성 (SPI 전용) */
				uint8_t DRDY_MASK      : 1;
				uint8_t                : 1;
				uint8_t INT2_on_INT1   : 1;
				uint8_t                : 2;
			};
			uint8_t Byte;
		}CTRL4_C;                                   /* 0x13 */

		uint8_t CTRL5_C;                            /* 0x14 — self-test, 미사용 */
		uint8_t CTRL6_C;                            /* 0x15 — DEN trigger, 미사용 */
		uint8_t CTRL7_XL;                           /* 0x16 — user offset, 미사용 */

		/* 0x17 — bit1:0 은 DS13005 Rev 7 Table 52 상 "must be set to 0" 예약이다.
		   아래 두 이름은 LSM6DSO 계열 잔재로 본 부품엔 그 기능이 없다.
		   1 을 쓰면 미정의 동작이므로 서술자 WriteMask(0xFC)가 전송에서 지운다 */
		union{
			struct{
				uint8_t LOW_PASS_ON_6D     : 1;     /* 예약 (0 고정) — 본 부품 미지원 */
				uint8_t XL_FS_MODE         : 1;     /* 예약 (0 고정) — 본 부품 미지원 */
				uint8_t HP_SLOPE_XL_EN     : 1;     /* 1 = HPF, 0 = LPF2 */
				uint8_t FASTSETTL_MODE_XL  : 1;
				uint8_t HP_REF_MODE_XL     : 1;
				uint8_t HPCF_XL            : 3;     /* cutoff: ODR/4 ~ ODR/800 */
			};
			uint8_t Byte;
		}CTRL8_XL;                                  /* 0x17 */

		/* 0x18 — DEN 설정과 "1 고정" 예약비트가 섞여 있어 통째로 쓰면 안 된다.
		   드라이버가 구동하는 비트는 DEVICE_CONF 뿐이고 WriteMask(0x02)가 그것을 보장한다 —
		   나머지 비트는 마지막 실측값(SyncedImage)에서 보존된다 */
		union{
			struct{
				uint8_t                : 1;         /* 예약 (0 고정) */
				uint8_t DEVICE_CONF    : 1;         /* 초기화·소프트리셋 후 1 필수 */
				uint8_t DEN_LH         : 1;
				uint8_t DEN_EN         : 1;
				uint8_t                : 2;         /* 예약 (DEN 사용 시 1 고정) */
				uint8_t DEN_Y          : 1;
				uint8_t DEN_X          : 1;
			};
			uint8_t Byte;
		}CTRL9_XL;                                  /* 0x18 */

		uint8_t CTRL10_C;                           /* 0x19 — timestamp, 미사용 */
		uint8_t Reserved1A[4];                      /* 0x1A~0x1D — *_SRC (RO) */

		union{
			struct{
				uint8_t XLDA        : 1;            /* Accel data ready */
				uint8_t             : 1;
				uint8_t TDA         : 1;            /* Temp data ready */
				uint8_t             : 5;
			};
			uint8_t Byte;
		}STATUS;                                    /* 0x1E */

		uint8_t Reserved1F;                         /* 0x1F — 예약 주소 */

		/* 0x20~0x2B — IF_INC 자동증가로 한 번에 읽는 연속 블록.
		   0x22~0x27은 자이로 자리(본 부품 미존재)라 Reserved로 건너뛴다.
		   이 블록은 읽기 전용 경로다 — 예약 구간을 관통하므로 쓰기에 재사용 금지 */
		struct{
			int16_t TEMP;                           /* 0x20-21 */
			int16_t Reserved[3];                    /* 0x22-27 — 자이로 자리 (미사용) */
			int16_t ACCEL_X;                        /* 0x28-29 */
			int16_t ACCEL_Y;                        /* 0x2A-2B */
		}Data;
	};
}IIS2ICLX_Register_t;
#pragma pack()

/* 오프셋 항등 단언용 래퍼. STM32CubeIDE 의 CDT 인덱서는 자체 파서라 C11 _Static_assert 를
   파싱하지 못하고 에디터에 가짜 오류를 낸다. __CDT_PARSER__ 는 그 인덱서만 정의하므로
   실제 빌드(GCC)에서는 단언이 그대로 살아 있다 — 검증을 끄는 것이 아니라 표시만 숨긴다 */
#if defined(__CDT_PARSER__)
#define IIS2ICLX_STATIC_ASSERT(cond, msg)
#else
#define IIS2ICLX_STATIC_ASSERT(cond, msg)      _Static_assert(cond, msg)
#endif

/* 주소↔오프셋 항등 — 오버레이가 어긋나면 여기서 빌드가 멈춘다 */
IIS2ICLX_STATIC_ASSERT(sizeof(IIS2ICLX_Register_t)                 == IIS2ICLX_REG_IMAGE_SIZE, "IIS2ICLX image size");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, INT1_CTRL)    == IIS2ICLX_REG_INT1_CTRL,  "INT1_CTRL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, INT2_CTRL)    == IIS2ICLX_REG_INT2_CTRL,  "INT2_CTRL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, WHO_AM_I)     == IIS2ICLX_REG_WHO_AM_I,   "WHO_AM_I offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL1_XL)     == IIS2ICLX_REG_CTRL1_XL,   "CTRL1_XL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL3_C)      == IIS2ICLX_REG_CTRL3_C,    "CTRL3_C offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL4_C)      == IIS2ICLX_REG_CTRL4_C,    "CTRL4_C offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL5_C)      == IIS2ICLX_REG_CTRL5_C,    "CTRL5_C offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL6_C)      == IIS2ICLX_REG_CTRL6_C,    "CTRL6_C offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL7_XL)     == IIS2ICLX_REG_CTRL7_XL,   "CTRL7_XL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL8_XL)     == IIS2ICLX_REG_CTRL8_XL,   "CTRL8_XL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL9_XL)     == IIS2ICLX_REG_CTRL9_XL,   "CTRL9_XL offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, CTRL10_C)     == IIS2ICLX_REG_CTRL10_C,   "CTRL10_C offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, Reserved1A)   == IIS2ICLX_REG_ALL_INT_SRC,"SRC block offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, STATUS)       == IIS2ICLX_REG_STATUS,     "STATUS offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, Data)         == IIS2ICLX_REG_OUT_TEMP_L, "Data block offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, Data.ACCEL_X) == IIS2ICLX_REG_OUTX_L_A,   "ACCEL_X offset");
IIS2ICLX_STATIC_ASSERT(offsetof(IIS2ICLX_Register_t, Data.ACCEL_Y) == IIS2ICLX_REG_OUTY_L_A,   "ACCEL_Y offset");

/* 쓰기 가능 주소 집합 ∩ 예약 주소 집합 = 공집합 (0x00~0x2B 를 비트로 표현).
   예약 주소에 쓰면 부품이 영구 손상되므로 이 불변식은 빌드에서 강제한다 */
#define IIS2ICLX_ADDRSET_RESERVED              0x000000FC80020079ULL
#define IIS2ICLX_ADDRSET_WRITABLE              0x0000000003FD7F86ULL
IIS2ICLX_STATIC_ASSERT((IIS2ICLX_ADDRSET_RESERVED & IIS2ICLX_ADDRSET_WRITABLE) == 0,
                       "IIS2ICLX writable set overlaps reserved set");

/* ───────── 레지스터 서술자 ─────────
 * 런타임 접근 경로의 SSOT. 주소로부터 오프셋을 계산하지 않고 이 표를 읽는다.
 *
 * Access        : RESERVED(0) 가 기본값이다 — 표에 없는 주소는 쓰기 경로에 오르지 못한다
 * ReservedMask  : 데이터시트가 값을 고정한 비트("must be set to 0/1")
 * WriteMask     : 드라이버가 구동하는 비트. ~WriteMask 는 마지막 실측값에서 보존한다(RMW).
 *                 ReservedMask 와는 교집합이 없고, DEN 처럼 "기능은 있으나 이 드라이버가
 *                 건드리지 않는" 비트도 WriteMask 밖에 둬 보존 대상이 된다
 * SelfClearMask : 부품이 스스로 0 으로 내리는 비트. diff 와 전송값 양쪽에서 지운다 —
 *                 섀도가 1 을 기억하면 SW_RESET 을 매 Sync 마다 재발행한다
 * IsVolatile    : 부품이 임의로 바꾸는 출력. diff 대상에서 제외 (OUT_*·STATUS·*_SRC)
 * PorValue      : Table 17 리셋값. SoftReset 성공 직후 SyncedImage 재시드에 쓴다 */
typedef enum{
	IIS2ICLX_REGACCESS_RESERVED       = 0,  /* 쓰면 영구 손상 — 기본값 */
	IIS2ICLX_REGACCESS_RO             = 1,
	IIS2ICLX_REGACCESS_RW             = 2,
	IIS2ICLX_REGACCESS_OUTSIDE_IMAGE  = 3,  /* 실재하나 이미지 밖 — 직접 API 로만 접근 */
}IIS2ICLX_RegAccess_t;

typedef struct{
	uint8_t Address;
	uint8_t ImageOffset;
	uint8_t SizeOfRegister;
	uint8_t Access;
	uint8_t ReservedMask;
	uint8_t WriteMask;
	uint8_t SelfClearMask;
	uint8_t IsVolatile;
	uint8_t PorValue;
}IIS2ICLX_RegInfo_t;

#pragma pack(1)
typedef struct{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;                                      /* CS 핀 (값 — ISM330DHCX 패턴) */

	IIS2ICLX_Register_t Register;                  /* 섀도 이미지 — 목표 상태 */
	uint8_t SyncedImage[IIS2ICLX_REG_IMAGE_SIZE];  /* 마지막으로 확인된 부품 진실 — diff 기준 */
	uint8_t AutoSync;                              /* 1 = Sync 가 diff 전송, 0 = RW 전량 전송 */
	uint8_t IsSeeded;                              /* 1 = 섀도가 부품에서 유래 — Sync 전제조건 */

	double Temperature;                            /* ℃ */
	double DataRate;                               /* Hz — 현재 ODR 실수값 */

	IIS2ICLX_Mount_t Mount;                        /* 부착 방향 — 0(HORIZONTAL) 기본 */

	oVector2_t Acceleration;                       /* µG — 2축 부품이라 Z 없음 */
	double     GravityMagnitude;                   /* µG — |a|. 1 g 이탈 = 동적 가속 혼입 */
	double     OutOfPlaneAngle;                    /* ° — 면외 경사 크기 (VERTICAL 전용, 부호 없음) */
	oRPY_t     Angle;                              /* ° — Pitch=asin(X), Roll=asin(Y), Yaw 미사용 */
	oRPY_t     OverSampling;                       /* GetAngle 누산기 */

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfOverSampling;
		uint8_t CountOfError;
		uint8_t CountOfSpiError;                   /* HAL_ERROR 연속 누적 — recovery 트리거 */
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}IIS2ICLX_t;
#pragma pack()

extern const IIS2ICLX_RegInfo_t *IIS2ICLX_GetRegInfo(uint8_t RegAddress);
extern oResult_t IIS2ICLX_ReadReg(IIS2ICLX_t *pDev, uint8_t RegAddress, uint32_t SizeOfData);
extern oResult_t IIS2ICLX_WriteReg(IIS2ICLX_t *pDev, uint8_t RegAddress, uint32_t SizeOfData);
extern oResult_t IIS2ICLX_Sync(IIS2ICLX_t *pDev);

extern oResult_t IIS2ICLX_WhoAmI(IIS2ICLX_t *pDev, uint8_t *pId);
extern oResult_t IIS2ICLX_SoftReset(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_GetStatus(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_GetConfig(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_SetConfig(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_GetData(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_GetAngle(IIS2ICLX_t *pDev, uint8_t Count, oRPY_t *pAngle);
extern void      IIS2ICLX_SetFilter(IIS2ICLX_t *pDev, double ODR);
extern oResult_t IIS2ICLX_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_Close(IIS2ICLX_t *pDev);
extern oResult_t IIS2ICLX_Recovery(IIS2ICLX_t *pDev);

#endif /* INC_MEMS_IIS2ICLXTR_H_ */

/* History

2026-09-03 | v0.1
	- baseline (MEMS_IIS2ICLXTR.h) — 구조는 MEMS_ISM330DHCXTR(SPI·oIO_t CS·시퀀스 상태머신) 이식,
	  2축 부품에 맞춰 Z축·자이로 제거 + asin 기반 경사각 산출로 대체.
	  레지스터 값·감도 상수는 데이터시트 대조 전 (상단 "미검증 상수" 참조)
	- INT1_CTRL(0x0D) 지원 — INT1_DRDY_XL 로 data-ready 를 INT1 핀에 실어, 호출측이 STATUS 를
	  SPI 로 폴링하지 않고 GPIO 레벨만 보고 새 샘플 시점을 알 수 있다. 비트 배치는 lsm6dso_reg.h 대조 완료
	- 부착 방향 Mount 지원 (HORIZONTAL / VERTICAL_X / VERTICAL_Y). 벽면 부착은 중력이
	  보드면 안에 놓여 asin 단축 환산이 성립하지 않으므로 atan2(교차축, |중력축|) 기하로 분기.
	  면외 경사(OutOfPlaneAngle)·중력 크기(GravityMagnitude)는 별도 필드로 노출 — Angle.Roll 에
	  섞으면 호출부가 Mount 를 보지 않고는 부호 있는 축인지 크기인지 구분할 수 없다

2026-10-20 | v0.2
	- Register 를 uint8_t Image[0x2C] 정본 + 비트필드 오버레이 union 으로 재구성. 기존 구조체는
	  선언 순서가 주소 순서와 무관해 이미지 기반 접근이 불가능했다. 주소↔오프셋 항등은
	  offsetof 정적 단언으로 빌드에서 강제한다 (호출부 필드 경로는 전부 그대로)
	- 레지스터 서술자 테이블(IIS2ICLX_RegInfo_t) 도입 — 런타임 접근이 주소 산술이 아니라 이 표를
	  경유한다. 항등이 깨지는 IC(멀티바이트 레지스터·뱅크 전환)가 같은 계층에 합류해도
	  인터페이스가 바뀌지 않게 하기 위함이다. 이미지 밖 레지스터는 OUTSIDE_IMAGE 로 명시
	- 예약 주소 쓰기 차단 — DS13005 Rev 7 p.32 는 예약 레지스터 쓰기를 "permanent damage" 로
	  규정한다. 서술자 기본값이 RESERVED 라 표에 없는 주소는 전송 경로에 오를 수 없고,
	  WriteReg 범위가 예약 주소를 관통하면 조용히 넘기지 않고 오류를 반환한다.
	  "쓰기 가능 ∩ 예약 = 공집합"은 64비트 주소집합 마스크로 정적 단언
	- 이미지 기반 IIS2ICLX_ReadReg/WriteReg(pDev, 주소, 길이) 공개. 버퍼를 받던 기존 내부 함수는
	  static SPI 원시 계층(SpiRead/SpiWrite)으로 남아 두 API 가 한 경로를 쓴다
	- AutoSync + IsSeeded + IIS2ICLX_Sync 신설 — SyncedImage 와 diff 해 바뀐 RW 바이트만 전송한다.
	  C 에는 세터가 없으므로 "대입 즉시 반영"이 아니라 지연 diff 가 이 기능의 의미다.
	  부품에서 read 로 seed 되기 전에는 Sync 를 거부한다 — 부품을 본 적 없는 섀도에서 나온
	  트래픽은 목표값이 0 이 아닌 모든 RW 바이트에 쓰기를 발행한다
	- 쓰기값 합성 규칙: (Image & WriteMask) | (SyncedImage & ~WriteMask), 이후 SelfClearMask 제거.
	  예약비트는 데이터시트 각주("must be set to 0/1")를 근거로 되쓰지 않고 실측값에서 보존한다.
	  CTRL9_XL 의 DEVICE_CONF RMW 가 이 규칙의 일반화다
	- SoftReset 성공 직후 SyncedImage 만 POR 값으로 재시드한다. Image(목표 상태)는 보존 —
	  둘 다 되돌리면 DEVICE_CONF=1 의도와 호출부의 FS_XL 프리셋이 지워져, 재시드가 막으려던
	  "diff 가 차이를 못 보는" 증상을 오히려 만든다
	- 데이터시트 재대조: 원문은 DS13005 Rev 7. WHO_AM_I=6Bh·ODR/FS 코드·CTRL 비트배치 일치.
	  CTRL8_XL bit1:0 은 Rev 7 Table 52 상 "must be set to 0" 예약이라 LOW_PASS_ON_6D·XL_FS_MODE
	  이름은 LSM6DSO 계열 잔재다 — WriteMask 0xFC 로 차단하고 주석으로 명시
	- CTRL9_XL 을 구조체에 편입 (기존에는 SetConfig 의 지역변수로만 존재)
	- CS 인자 값 전달 통일 규약 적용 — Open 에서 CS.Port == NULL 을 거부하지 않고,
	  CS 토글 지점마다 Port NULL 이면 건너뛴다 (CS 없는 보드 = oIO_t {0})
*/
