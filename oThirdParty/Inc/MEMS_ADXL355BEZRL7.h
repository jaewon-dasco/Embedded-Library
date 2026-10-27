/*0
 * MEMS_ADXL355BEZRL7.h
 *
 *  Created on: Aug 1, 2025
 *      Author: JONE
 *
 ************************************************************
 * Sensitivity (LSB/g):
 * ±2g mode → 256,000 LSB/g
 * ±4g mode → 128,000 LSB/g
 * ±8g mode → 64,000 LSB/g
 *************************************************************
 */

#ifndef INC_MEMS_ADXL355BEZRL7_H_
#define INC_MEMS_ADXL355BEZRL7_H_

#define MEMS_ADXL355BEZRL7_VERSION      0.4
#include "Mi_Main.h"

#define ADXL355_DATACONVERT_TO_ACC(x)   (int32_t)((((x & 0xFF0000) >> 16 | (x & 0x00FF00) | (x & 0x0000FF) << 16) >> 4) | ((x & 0x000080) ? 0xFFF00000 : 0))
#define ADXL355_ACC_TO_2ug(x)           ((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 3.8147)
#define ADXL355_ACC_TO_4ug(x)           ((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 7.6294)
#define ADXL355_ACC_TO_8ug(x)           ((double)(ADXL355_DATACONVERT_TO_ACC(x)) * 15.2588)

//#define ADXL355_DATACONVERT_TO_TEMP(x)	(int16_t)((((x & 0xFF00) >> 8 | (x & 0x00FF) << 8) >> 4) | ((x & 0x0080) ? 0xF000 : 0))
#define ADXL355_DATACONVERT_TO_TEMP(x)  (int16_t)(((x & 0xFF0) >> 4 | (x & 0x0F) << 8) | ((x & 0x0008) ? 0xF000 : 0))
#define ADXL355_TEMP_TO_Degree(x)       (25.0 + (double)(ADXL355_DATACONVERT_TO_TEMP(x) - 1852) / -9.05)

typedef enum{
	ADXL355_HPF_NOFILTER  = 0,
	ADXL355_HPF_24_7      = 0b001,  //0.00247 * ODR = Hz
	ADXL355_HPF_6_2084    = 0b010,  //0.00062084 * ODR = Hz
	ADXL355_HPF_1_5545    = 0b011,  //0.00015545 * ODR = Hz
	ADXL355_HPF_0_3862    = 0b100,  //0.00003862 * ODR = Hz
	ADXL355_HPF_0_0954    = 0b101,  //0.00000954 * ODR = Hz
	ADXL355_HPF_0_0238    = 0b110   //0.00000238 * ODR = Hz
}ADXL355_HPF;

typedef enum{
	ADXL355_ODR_4000hz_LPF_1000hz  = 0b0000,
	ADXL355_ODR_2000hz_LPF_500hz   = 0b0001,
	ADXL355_ODR_1000hz_LPF_250hz   = 0b0010,
	ADXL355_ODR_500hz_LPF_125hz    = 0b0011,
	ADXL355_ODR_250hz_LPF_62_5hz   = 0b0100,
	ADXL355_ODR_125hz_LPF_31_2hz   = 0b0101,
	ADXL355_ODR_62_5hz_LPF_15_6hz  = 0b0110,
	ADXL355_ODR_31_2hz_LPF_7_8hz   = 0b0111,
	ADXL355_ODR_15_6hz_LPF_3_9hz   = 0b1000,
	ADXL355_ODR_7_8hz_LPF_1_9hz    = 0b1001,
	ADXL355_ODR_3_9_2hz_LPF_0_9hz  = 0b1010,
}ADXL355_ODR_LPF;

typedef enum{
	ADXL355_G_RANGE_2G  = 0b01,
	ADXL355_G_RANGE_4G  = 0b10,
	ADXL355_G_RANGE_8G  = 0b11,
}ADXL355_GRagne_t;

#pragma pack(1)
typedef struct{
	I2C_HandleTypeDef *pI2C;

	struct{
		uint8_t  				: 8;                        //DEVID_AD
		uint8_t  				: 8;                        //DEVID_MST
		uint8_t  				: 8;                        //PARTID
		uint8_t  				: 8;                        //REVID

		union{
			struct{
				uint8_t DATA_RDY 	: 1;
				uint8_t FIFO_FULL 	: 1;
				uint8_t FIFO_OVR 	: 1;
				uint8_t Activity 	: 1;
				uint8_t NVM_BUSY 	: 1;
				uint8_t				: 3;
			};

			uint8_t Byte;
		}Status;

		uint8_t  FIFO_ENTRIES	: 8;

		struct{
			uint16_t TEMP			: 16;
			uint32_t XDATA			: 24;
			uint32_t YDATA			: 24;
			uint32_t ZDATA			: 24;
		}Data;

		uint8_t  FIFO_DATA		: 8;

		struct{
			uint16_t XOFFSET		: 16;
			uint16_t YOFFSET		: 16;
			uint16_t ZOFFSET		: 16;
			uint8_t  ACT_EN			: 8;
			uint16_t ACT_THRESH		: 16;
			uint8_t  ACT_COUNT		: 8;

			union{
				struct{
					uint8_t ODR_LPF		: 4;
					uint8_t HPF_CORNER	: 3;
					uint8_t 			: 1;
				};

				uint8_t Byte;
			}Filter;

			uint8_t  FIFO_SAMPLES	: 8;

			union{
				struct{
					uint8_t RDY_EN1		: 1;
					uint8_t FULL_EN1	: 1;
					uint8_t OVR_EN1		: 1;
					uint8_t ACT_EN1		: 1;
					uint8_t RDY_EN2		: 1;
					uint8_t FULL_EN2	: 1;
					uint8_t OVR_EN2		: 1;
					uint8_t ACT_EN2		: 1;
				};

				uint8_t Byte;
			}INT_MAP;

			uint8_t 				: 8;                     //Sync

			union{
				struct{
					ADXL355_GRagne_t GravityRange	: 2;    //01=±2g | 10=±4g | 11=±8g
					uint8_t							: 4;  //Reserved
					uint8_t			 INT_POL		: 1;
					uint8_t			 I2C_HS			: 1;
				};

				uint8_t Byte;
			}Range;

			union{
				struct{
					uint8_t Standby		: 1;              //1=Standby 0=Operating
					uint8_t TEMP_OFF	: 1;
					uint8_t DRDY_OFF	: 1;
					uint8_t				: 5;
				};

				uint8_t Byte;
			}POWER_CTL;

			uint8_t 				: 8;                     //SELF_TEST
		}Parameter;

		uint8_t  Reset			: 8;                       //write code 0x52 to reset the device.
	}Register;

	double Temperature;
	double DataRate;
	double LowPassFreqeuncy;
	double HighPassFreqeuncy;

	/* State 는 Open/Close 에서 memset 되므로 세션을 넘겨 살아야 하는 진단값은 여기 둔다 */
	uint32_t	MaxI2CElapsedUs;                             /* I2C 전송 최대 소요시간 (us) — 타임아웃 값의 근거 */
	uint8_t		CountOfI2CTimeout;                        /* HAL_TIMEOUT 누적 — 버스 wedge 지표 (NACK 와 구분) */

	oVector3_t 	Acceleration;//uG
	oRPY_t 		Angle;
	oRPY_t 		OverSampling;

	struct{
		uint8_t SequenceStep;
		uint8_t CountOfOverSampling;
		uint8_t CountOfError;
		uint8_t CountOfI2CError;                             /* HAL 실패 연속 누적 — recovery 트리거 */
		uint8_t IsRun;
		uint8_t IsBusy;
		uint8_t IsOpen;
	}State;

}ADXL355_t;
#pragma pack()

extern oResult_t ADXL355_Reset(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetStatus(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetParameter(ADXL355_t *pADXL355);
extern oResult_t ADXL355_SetParameter(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetData(ADXL355_t *pADXL355);
extern oResult_t ADXL355_GetAngle(ADXL355_t *pADXL355, uint8_t Count, oRPY_t *pAngle);
extern oResult_t ADXL355_GetCountOfFIFOEntries(ADXL355_t *pADXL355, uint8_t *pEntries);
extern oResult_t ADXL355_ReadFIFO(ADXL355_t *pADXL355, uint8_t SetCount, uint8_t *pBuf);
extern void      ADXL355_ParseFIFOSet(ADXL355_t *pADXL355, const uint8_t *p9, oVector3_t *pOut);
extern void ADXL355_SetFilter(ADXL355_t *pADXL355, double HPF, double LPF);
extern oResult_t ADXL355_Open(I2C_HandleTypeDef *pI2C, ADXL355_t *pADXL355);
extern oResult_t ADXL355_Close(ADXL355_t *pADXL355);
extern oResult_t ADXL355_Recovery(ADXL355_t *pADXL355);
#endif /* INC_MEMS_ADXL355BEZRL7_H_ */

/* History

2026-06-26 | v0.1
	- baseline (MEMS_ADXL355BEZRL7.h)
2026-08-26 | v0.3
	- I2C 타임아웃 도입: ADXL355_Read/Write 의 HAL_MAX_DELAY -> ADXL355_I2C_TIMEOUT(100ms),
	  ADXL355_Reset 의 하드코딩 100 도 같은 상수로 통일.
	  HAL_MAX_DELAY 는 버스 wedge(슬레이브가 SDA 를 잡음) 시 HAL 이 영원히 반환하지 않아 메인 루프가
	  정지했고, 그 결과 이 드라이버가 이미 갖고 있던 회복 장치(OnHalError -> CountOfI2CError >=
	  RECOVERY_THRESHOLD -> Recovery 의 I2C DeInit/Init)에 도달조차 못 했다. 상위의 측정 데드라인·
	  루프 가드도 모두 루프가 도는 것을 전제하므로 무력화됐고 IWDG 리셋이 유일한 회복이었다
	- MaxI2CElapsedUs / CountOfI2CTimeout 추가:
	  · 100ms 근거: ADXL355_Reset 이 같은 버스에 쓰던 값이라 드라이버 내 선례가 있고, 정상 전송
	    (수백 us 예상)의 수백 배 여유라 지터를 오류로 오인하지 않는다. 값을 줄여도 회복 지연은
	    RECOVERY_THRESHOLD(5회 누적)가 지배하므로 시스템 거동이 달라지지 않는다 —
	    타임아웃은 짧은 쪽이 위험하고 긴 쪽은 무해한 비대칭이다
	  · MaxI2CElapsedUs 는 TICKBASE_CORECYCLE_US(DWT) 기반 전송 소요시간 최대값 — 상시 유지.
	    필드에서 버스 열화(MaxElapsedUs 상승·TimeoutCount 누적)를 보는 진단값 (Get/SensorI2CState)
	  · OnHalError 가 HAL_StatusTypeDef 를 받아 HAL_TIMEOUT(버스 물림, Recovery 가 유효)과
	    HAL_ERROR(NACK, 슬레이브 부재라 DeInit/Init 무효)를 구분해 기록. 임계 동작은 아직 동일
	  · MaxI2CElapsedUs/CountOfI2CTimeout 은 State 가 아니라 그 밖(구조체 최상위)에 둔다 —
	    State 는 ADXL355_Open/Close 에서 memset 되므로 세션마다 지워져 진단값이 항상 0으로 읽혔다
	    (실기 확인: 측정 후 Get/SensorI2CState 가 MaxElapsedUs=0). CountOfI2CError 는 세션 단위
	    회복 임계 카운터이므로 State 에 그대로 둔다
	  · 실측(SIA100 보드, I2C2 Timing=0x00503D58, 상온): MaxElapsedUs=2106 (2.1ms).
	    타임아웃 100ms 는 47배 여유 — 지터를 오류로 오인할 여지가 없다
2026-07-10 | v0.2
	- ADXL355_Reset(ADXL355_t*) 추가 — RESET reg(0x2F)에 0x52 기록, 칩 소프트리셋.
	  MCU 리셋으로는 센서가 리셋되지 않아(전원 유지) wedged 상태(전축 -FS rail) 지속 —
	  Open 시퀀스(pI2C 바인딩 직후)에 통합, 매 Open마다 known state 보장
2026-09-01 | v0.4
	- 실패 경로 로그 추가 (ONE_Serial 의존 추가, NAND 드라이버와 동일 관용구):
	  · OnHalError 가 RegAddress 를 받아 "I2C fail reg=0x?? hal=? i2cerr=?" 출력 —
	    hal 1=ERROR(NACK) / 2=BUSY / 3=TIMEOUT. 상위 Measurement 계층에서는 State 가 Close 의
	    memset 으로 지워진 뒤라 어느 레지스터에서 실패했는지 복원할 수 없다
	  · Open 실패 시 "Open fail ostep=?" — SequenceStep 이 tail 에서 0 으로 되돌아가기 직전에 남긴다
	  · Recovery 진입·실패 로그
	- 근거: 읽기는 정상인데 특정 레지스터 쓰기만 NACK 되는 개체를 만났고(SIA100_IN 2026-08-31),
	  이 구분을 얻으려고 계측 펌웨어를 3회 만들어야 했다. 로그가 있으면 재발 1회로 판별된다
*/
