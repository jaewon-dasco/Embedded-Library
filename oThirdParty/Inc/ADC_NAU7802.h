/*
 * ADC_NAU7802.h
 *
 *  Created on: Jan 27, 2025
 *      Author: JONE
 */
#ifndef INC_ADC_NAU7802_H_
#define INC_ADC_NAU7802_H_

#define ADC_NAU7802_VERSION              0.9

#ifdef __HAL_I2C_ENABLE

#include "Mi_Main.h"
#define NAU7802_VREF(REFP,REFN)          ((double)(REFP)-(double)(REFN))
#define NAU7802_INPUT_RANGE(VREF,PGA)    (0.5/((double)(VREF)/(double)(PGA)))
#define NAU7802_EV_GAIN(VMAX,VREF)       ((double)(VREF)/((double)(VMAX)*0.5))  //Expected gain | VMAX(Max input voltage (1V)) | VREF(1V)
#define NAU7802_ADC_BITMASK              0xFFFFFF
#define NAU7802_ADC_MAXVALUE             (16777215)
#define NAU7802_ADC_TO_V(ADC)            ((((double)(NAU7802_VLDO)/(double)NAU7802_ADC_MAXVALUE) * (double)(ADC)) / (MATH_MAX((double)(NAU7802_Gain), 1)))
#define NAU7802_ADDRESS                  (uint16_t)(0x54)                       // slave address

#define NAU7802_REG_PUCTRL               0x00
#define NAU7802_REG_CTRL1                0x01
#define NAU7802_REG_CTRL2                0x02
#define NAU7802_REG_I2C_CTRL             0x11
#define NAU7802_REG_ADC_B2               0x12
#define NAU7802_REG_ADC_B1               0x13
#define NAU7802_REG_ADC_B0               0x14
#define NAU7802_REG_ADC_CTRL             0x15
#define NAU7802_REG_PGA_CTRL             0x1B
#define NAU7802_REG_POWER                0x1C
#define NAU7802_REG_CHIP_ID              0x1F

#define OCAL1_REG_B2                     0x03                                   /* CH1 OFFSE Calibration[23:16] 	0x00 */
#define OCAL1_REG_B1                     0x04                                   /* CH1 OFFSET Calibration[15:8] 	0x00 */
#define OCAL1_REG_B0                     0x05                                   /* CH1 OFFSET Calibration[7:0] 		0x00 */
#define GCAL1_REG_B3                     0x06                                   /* CH1 GAIN Calibration[31:24] 		0x00 */
#define GCAL1_REG_B2                     0x07                                   /* CH1 GAIN Calibration[23:16] 		0x80 */
#define GCAL1_REG_B1                     0x08                                   /* CH1 GAIN Calibration[15:8] 		0x00 */
#define GCAL1_REG_B0                     0x09                                   /* CH1 GAIN Calibration[7:0] 		0x00 */
#define OCAL2_REG_B2                     0x0A                                   /* CH2 OFFSET Calibration[23:16] 	0x00 */
#define OCAL2_REG_B1                     0x0B                                   /* CH2 OFFSET Calibration[15:8] 	0x00 */
#define OCAL2_REG_B0                     0x0C                                   /* CH2 OFFSET Calibration[7:0] 		0x00 */
#define GCAL2_REG_B3                     0x0D                                   /* CH2 GAIN Calibration[31:24] 		0x00 */
#define GCAL2_REG_B2                     0x0E                                   /* CH2 GAIN Calibration[23:16] 		0x80 */
#define GCAL2_REG_B1                     0x0F                                   /* CH2 GAIN Calibration[15:8] 		0x00 */
#define GCAL2_REG_B0                     0x10                                   /* CH2 GAIN Calibration[7:0] 		0x00 */


#define NAU7802_MIN_CONVERSIONS          6

/* 비트 상승 대기 상한 — I2C가 정상인데(ReadBit=RESULT_NULL) 비트가 서지 않는 정지를 끊는다.
 * 오류 카운터는 I2C 실패(RESULT_ERROR)만 세므로 이 경로는 시간으로만 종료 가능.
 * 변환 관련 대기는 설정된 변환율에서 유도한다 — 고정값을 쓰면 320SPS에서 실제 필요치의 수백 배가 되어
 * 고장 검출이 느려지고, 반대로 값을 낮추면 저속 설정에서 정상 변환을 죽인다. */
#define NAU7802_CONVERSION_MS(crs)       ((crs) == NAU7802_CTRL2_CRS_320SPS ? 4 : \
                                          (crs) == NAU7802_CTRL2_CRS_80SPS  ? 13 : \
                                          (crs) == NAU7802_CTRL2_CRS_40SPS  ? 25 : \
                                          (crs) == NAU7802_CTRL2_CRS_20SPS  ? 50 : 100)   /* 1변환 소요 | Unit=1ms */

#define NAU7802_TIMEOUT_MARGIN           2                                      /* 변환 소요 대비 안전 배수 */
#define NAU7802_CALS_CONVERSIONS         4                                      /* 내부 오프셋 교정 소요 (변환 수) */
#define NAU7802_TIMEOUT_MIN_MS           50                                     /* 하한 — 메인 루프 1주기가 길어져도 정상 변환을 죽이지 않도록 */

#define NAU7802_CR_TIMEOUT(crs)          MATH_MAX(NAU7802_CONVERSION_MS(crs) * NAU7802_TIMEOUT_MARGIN, NAU7802_TIMEOUT_MIN_MS)
#define NAU7802_CALS_TIMEOUT(crs)        MATH_MAX(NAU7802_CONVERSION_MS(crs) * NAU7802_CALS_CONVERSIONS * NAU7802_TIMEOUT_MARGIN, NAU7802_TIMEOUT_MIN_MS)
#define NAU7802_PUR_TIMEOUT              300                                    /* 파워업(PUD→PUR) 대기 — 변환율 무관 | Unit=1ms */

#define NAU7802_CONVERSION_ERROR_MAX     5                                      /* ADCRead 1세션이 견디는 누적 오류 — 초과 시 세션 ERROR */
#define NAU7802_FLUSH_COUNT              5                                      /* 채널·설정 변경 후 버리는 변환 수 (정착 전 샘플) */

//PUCTRL
#define NAU7802_PUCTRL_RR                0b00000001
#define NAU7802_PUCTRL_PUD               0b00000010
#define NAU7802_PUCTRL_PUA               0b00000100
#define NAU7802_PUCTRL_PUR               0b00001000
#define NAU7802_PUCTRL_CS                0b00010000
#define NAU7802_PUCTRL_CR                0b00100000
#define NAU7802_PUCTRL_OSCS              0b01000000
#define NAU7802_PUCTRL_AVDDS             0b10000000

//CTRL1
#define NAU7802_CTRL1_GAIN_X1            0b00000000
#define NAU7802_CTRL1_GAIN_X2            0b00000001
#define NAU7802_CTRL1_GAIN_X4            0b00000010
#define NAU7802_CTRL1_GAIN_X8            0b00000011
#define NAU7802_CTRL1_GAIN_X16           0b00000100
#define NAU7802_CTRL1_GAIN_X32           0b00000101
#define NAU7802_CTRL1_GAIN_X64           0b00000110
#define NAU7802_CTRL1_GAIN_X128          0b00000111
#define NAU7802_CTRL1_GAIN_MASK          0b00000111

#define NAU7802_CTRL1_VLDO_4_5V          0b00000000
#define NAU7802_CTRL1_VLDO_4_2V          0b00001000
#define NAU7802_CTRL1_VLDO_3_9V          0b00010000
#define NAU7802_CTRL1_VLDO_3_6V          0b00011000
#define NAU7802_CTRL1_VLDO_3_3V          0b00100000
#define NAU7802_CTRL1_VLDO_3_0V          0b00101000
#define NAU7802_CTRL1_VLDO_2_7V          0b00110000
#define NAU7802_CTRL1_VLDO_2_4V          0b00111000
#define NAU7802_CTRL1_VLDO_MASK          0b00111000

#define NAU7802_CTRL1_DRDY_SEL           0b01000000
#define NAU7802_CTRL1_CRP                0b10000000

//CTRL2
#define NAU7802_CTRL2_CHS_CH1            0b00000000
#define NAU7802_CTRL2_CHS_CH2            0b10000000
#define NAU7802_CTRL2_CHS_MASK           0b10000000

#define NAU7802_CTRL2_CRS_10SPS          0b00000000
#define NAU7802_CTRL2_CRS_20SPS          0b00010000
#define NAU7802_CTRL2_CRS_40SPS          0b00100000
#define NAU7802_CTRL2_CRS_80SPS          0b00110000
#define NAU7802_CTRL2_CRS_320SPS         0b01110000
#define NAU7802_CTRL2_CRS_MASK           0b01110000

#define NAU7802_CTRL2_CAL_ERR            0b00001000
#define NAU7802_CTRL2_CALS               0b00000100

#define NAU7802_CTRL2_CALMOD_DEFAULT     0b00000000
#define NAU7802_CTRL2_CALMOD_SYS_OFFSET  0b00000010
#define NAU7802_CTRL2_CALMOD_SYS_GAIN    0b00000011
#define NAU7802_CTRL2_CALMOD_MASK        0b00000011

//PGA
#define NAU7802_PGA_RD_OTP_SEL           0b10000000
#define NAU7802_PGA_LDOMODE              0b01000000
#define NAU7802_PGA_BUFFER_OUTPUT        0b00100000
#define NAU7802_PGA_BYPASS_ENABLE        0b00010000
#define NAU7802_PGA_PGAINV               0b00001000
#define NAU7802_PGA_PGACHPDIS_DISABLE    0b00000001

//POWER
#define NAU7802_PWR_CAP_EN               0b10000000

#define NAU7802_PWR_MASTER_BIAS_100P     0b00000000
#define NAU7802_PWR_MASTER_BIAS_90P      0b00010000
#define NAU7802_PWR_MASTER_BIAS_80P      0b00100000
#define NAU7802_PWR_MASTER_BIAS_73P      0b00110000
#define NAU7802_PWR_MASTER_BIAS_67P      0b01000000
#define NAU7802_PWR_MASTER_BIAS_62P      0b01010000
#define NAU7802_PWR_MASTER_BIAS_58P      0b01100000
#define NAU7802_PWR_MASTER_BIAS_54P      0b01110000

#define NAU7802_PWR_ADC_CURR_100P        0b00000000
#define NAU7802_PWR_ADC_CURR_95P         0b00000100
#define NAU7802_PWR_ADC_CURR_86P         0b00001000
#define NAU7802_PWR_ADC_CURR_70P         0b00001100

#define NAU7802_PWR_PGA_CURR_100P        0b00000000
#define NAU7802_PWR_PGA_CURR_95P         0b00000001
#define NAU7802_PWR_PGA_CURR_86P         0b00000010
#define NAU7802_PWR_PGA_CURR_70P         0b00000011

typedef enum{
	NAU7802_SAMPLING_10SPS   = 0b00000000,
	NAU7802_SAMPLING_20SPS   = 0b00010000,
	NAU7802_SAMPLING_40SPS   = 0b00100000,
	NAU7802_SAMPLING_80SPS   = 0b00110000,
	NAU7802_SAMPLING_320SPS  = 0b01110000
}NAU7802_SamplingRate_t;

typedef enum{
	NAU7802_VLDO_4_5V  = 0b00000000,
	NAU7802_VLDO_4_2V  = 0b00001000,
	NAU7802_VLDO_3_9V  = 0b00010000,
	NAU7802_VLDO_3_6V  = 0b00011000,
	NAU7802_VLDO_3_3V  = 0b00100000,
	NAU7802_VLDO_3_0V  = 0b00101000,
	NAU7802_VLDO_2_7V  = 0b00110000,
	NAU7802_VLDO_2_4V  = 0b00111000,
}NAU7802_VLDO_t;

typedef enum{
	NAU7802_GAIN_X1      = 0b00000000,
	NAU7802_GAIN_X2      = 0b00000001,
	NAU7802_GAIN_X4      = 0b00000010,
	NAU7802_GAIN_X8      = 0b00000011,
	NAU7802_GAIN_X16     = 0b00000100,
	NAU7802_GAIN_X32     = 0b00000101,
	NAU7802_GAIN_X64     = 0b00000110,
	NAU7802_GAIN_X128    = 0b00000111,
	NAU7802_GAIN_BYPASS  = 0b11111111,
}NAU7802_Gain_t;

typedef struct{
	void *pI2C;                    //I2C_HandleTypeDef
	double VREF;
	double AmpGain;
	uint8_t CurrentChannel;

	struct{
		uint8_t Channel;
		NAU7802_Gain_t Gain;
		NAU7802_VLDO_t VLDO;
		NAU7802_SamplingRate_t SamplingRate;
	}Registers;

	struct{
		double SumOfAnalog;
		uint8_t ReadCount;
	}OverSampling;

	uint8_t ErrorCount;
	uint8_t ConversionErrorCount;  /* ADCRead·Calibration 전용 — ErrorCount는 AnalogOverSampling이 세션 내내 점유 */
	uint8_t FlushRemain;           /* 남은 정착 버림 변환 수 — 0이 아니면 AnalogRead가 본 read 대신 버림 세션 수행 */
	uint8_t IsTimeoutFail;         /* 실패 사유 구분 — 1=비트 대기 타임아웃, 0=I2C 오류. ADCRead 호출 단위로 초기화 (최종 로그 전용) */
	uint8_t ProcessStep;
	uint8_t CalStep;
	int32_t ReadData;
	uint32_t Timer;
	uint32_t OpenTimestamp;

	uint8_t IsOpen;
	uint8_t IsOpening;
	uint8_t IsADConversion;
	uint8_t IsNeedCalibration;     /* gain·VLDO·채널·rate 변경 시 세트 → 다음 변환 전 내부 오프셋 재교정 */
}NAU7802_t;

extern oResult_t NAU7802_Init(NAU7802_t *pDev, I2C_HandleTypeDef *pI2C, NAU7802_VLDO_t VLDO);
extern oResult_t NAU7802_DeInit(NAU7802_t *pDev);
extern oResult_t NAU7802_IsDataReady(NAU7802_t *pDev);
extern oResult_t NAU7802_SetGain(NAU7802_t *pDev, NAU7802_Gain_t Gain);
extern oResult_t NAU7802_Calibration(NAU7802_t *pDev);
extern oResult_t NAU7802_ADCRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, int32_t *pADC);
extern oResult_t NAU7802_AnalogRead(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, double *pAnalog);
extern oResult_t NAU7802_AnalogOverSampling(NAU7802_t *pDev, uint8_t Channel, NAU7802_SamplingRate_t SamplingRate, uint8_t SamplingCount, double *pAnalog);
#endif
#endif /* INC_ADC_NAU7802_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ADC_NAU7802.h)

2026-07-16 | v0.2
	- 내부 오프셋 교정 추가: NAU7802_Calibration() + IsNeedCalibration 플래그
	- gain·VLDO·채널·rate 변경 시 다음 변환 전 자동 재교정 (채널별 OCAL)
2026-07-27 | v0.4
	- NAU7802_AnalogRead 플러시 트리거 조건 보강: `CurrentChannel != Channel` 단독 →
	  `|| pDev->IsNeedCalibration` 추가
	  · 기존엔 채널 번호 불일치로만 판단해 OCAL(오프셋 재교정) 직후에도 채널이 안 바뀐 경우엔
	    플러시가 생략됨 — gain 변경만으로 OCAL이 걸린 세션에서 정착 전 값이 채택될 여지가 있었음
	    (SIC100 Mi_Measurement v1.3→v1.5 회귀 조사 중 발견, 상세는 SIC100 Mi_Measurement.h 참조)
	  · IsNeedCalibration은 SetGain·SetSampling 모두에서 설정되는 공통 신호라 "OCAL이 막 걸렸다"는
	    쪽이 채널 번호 비교보다 정확한 트리거
	  · 플러시 버림 횟수 3→5회로 상향(SIC100 실측 기준 여유분 확보)
	  · 6개 디바이스 공용 코드 — 이 변경으로 늘어나는 변환 횟수는 SIC100에서만 실측 검증됨,
	    타 디바이스 100_Library 동기화 시 참고

2026-08-07 | v0.6
	- 비트 상승 대기 3곳(Init case7 PUR·Calibration case1 CALS·ADCRead case3 CR)에 종료 조건 추가
	  · RESULT_ERROR(I2C 실패): ErrorCount 3회 관용 후 abort — Init case3~11과 동일 규약
	  · RESULT_NULL(비트 미상승): NAU7802_PUR/CALS/CR_TIMEOUT(각 1000ms) 경과 시 abort
	  · Init case7은 기존 `default: break`가 RESULT_ERROR까지 흡수해 무한 RESULT_RUN이었음
	- ConversionErrorCount 필드 추가 — ADCRead·Calibration 전용 오류 카운터
	  · ErrorCount는 AnalogOverSampling이 읽기 세션 내내 점유(호출 중첩)하므로 공유 불가
	- ADCRead 세션 오류 관용을 종료 블록에서 일괄 집계 — NAU7802_CONVERSION_ERROR_MAX(5) 미만이면
	  같은 step 재시도, 도달 시에만 최종 RESULT_ERROR + 로그 1건
	  · 단발 I2C 실패로 세션이 죽으면 상위가 매번 풀 재Init에 들어가 복구가 느려짐
	  · Calibration 자체 관용(3회)은 제거 — ConversionErrorCount를 매 라운드 0으로 리셋해
	    ADCRead의 누적 집계가 상한에 영영 도달하지 못하는 무한 재시도를 만들었음
	  · 실패마다 찍던 중간 로그 제거, 최종 실패 1건(ADCRead FAIL at step%d)만 유지
	- AnalogRead 정착 버림(flush)을 협조형으로 전환 — FlushRemain 래치, 호출당 1세션 후 RESULT_RUN
	  · 기존 blocking while(5회)은 세션 오류 상한·비트 대기 상한이 곱해져 메인 루프가 워치독 주기를 넘김
	  · NAU7802_FLUSH_COUNT(5)로 버림 횟수 유지 — 트리거 조건이 사라져도 래치가 남아 횟수를 채움

2026-08-07 | v0.7
	- IsTimeoutFail 필드 추가 — ADCRead 최종 실패 로그에 사유(TIMEOUT / I2C) 표기
	  · 중간 로그를 최종 1건으로 줄이면서 비트 대기 타임아웃과 I2C 오류가 구분되지 않던 문제 해소

2026-08-07 | v0.8
	- 변환 대기 상한을 설정된 변환율에서 유도 — NAU7802_CONVERSION_MS(crs) × 배수, 단 TIMEOUT_MIN_MS(50) 하한
	  · CR = 1변환 × 2, CALS = 1변환 × 4 × 2 → 320SPS면 둘 다 하한 50ms, 10SPS면 200ms·800ms
	  · 고정 1000ms는 320SPS 실제 필요치의 수백 배라 고장 검출이 느렸고, 그렇다고 낮추면
	    저속 설정에서 정상 변환을 죽였다 — 레이트 유도로 두 요구를 동시에 만족
	  · 실측(320SPS, SIM100 벤치 2026-08-07): CALS 11ms / CR 5~6ms → 하한 50ms 대비 4.5×·8× 여유.
	    유도식만으로는 320SPS에서 8ms라 실측 5~6ms와 여유가 없다 — 하한 50ms가 실질 상한이므로 낮추지 말 것
	- PUR 대기는 변환율 무관이므로 고정 유지하되 1000 → 300ms (파워업 정착 실측 여유 반영)

2026-08-07 | v0.9
	- SetSampling 조기 반환이 CurrentChannel을 갱신하도록 수정 — 레지스터 그림자(Channel·SamplingRate)가
	  요청값과 같으면 조기 반환하는데, Channel=1(레지스터 0) + 10SPS(레지스터 0)는 DeInit 직후 초기값과 일치해
	  첫 호출부터 조기 반환한다. 그러면 CurrentChannel이 0에 고정돼 AnalogRead가 매 호출을 채널 전환으로 판정,
	  flush를 무한 재장전하며 세션이 영구 RESULT_RUN(워치독 미검출)에 빠진다. 10SPS 강제 주입으로 실측 재현
	- IsTimeoutFail을 ADCRead 호출 단위로 초기화 — 세션 단위 유지 시 흡수된 타임아웃(ConversionErrorCount 미달)
	  이후의 순수 I2C 오류까지 최종 로그가 TIMEOUT으로 오표기했다
*/
