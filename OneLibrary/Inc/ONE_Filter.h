/*
 * ONE_Filter.h
 *
 *  Created on: Apr 14, 2026
 *      Author: JONE
 *
 * 2nd-order Biquad IIR Filters (Audio EQ Cookbook)
 * LPF, HPF, BPF, Low Shelf, High Shelf
 * Moving Average Filter
 */

#ifndef INC_ONE_FILTER_H_
#define INC_ONE_FILTER_H_

#define ONE_FILTER_VERSION         0.2

#include "ONE_Math.h"

/* ============================================================
 * Initializer Macros
 * ============================================================
 * sample_freq : Sampling frequency (Hz)
 * cutoff_freq : Cutoff frequency (Hz), BPF: center frequency
 * q           : Quality factor (0.707 = Butterworth flat response)
 * gain_db     : Boost/Cut gain in dB (Shelf filters only)
 *
 * Usage:
 *   oFilter_t lpf = FILTER_PASS_INITIALIZER(1000.0, 50.0, 0.707);
 *   oFilter_t shelf = FILTER_SHELF_INITIALIZER(1000.0, 200.0, 6.0, 0.707);
 */
#define FILTER_PASS_INITIALIZER(sample_freq, cutoff_freq, q) { \
    .SamplingFreq = (sample_freq), \
    .CutoffFreq = (cutoff_freq), \
    .Q = (q), \
    .Reset = 1, \
}

#define FILTER_SHELF_INITIALIZER(sample_freq, cutoff_freq, gain_db, q) { \
    .SamplingFreq = (sample_freq), \
    .CutoffFreq = (cutoff_freq), \
    .GainDB = (gain_db), \
    .Q = (q), \
    .Reset = 1, \
}

/* ============================================================
 * Biquad Filter Structure
 * ============================================================ */
/* 대상 MCU 의 FPU 는 단정도 전용이라 double 은 소프트웨어 에뮬레이션이 된다.
 * 계수 산출(Reset 시 1회)만 double 중간값을 쓰고 저장·실행은 float. */
typedef struct {
	float SamplingFreq;  /* Fs (Hz) */
	float CutoffFreq;    /* Fc (Hz), BPF: center frequency */
	float Q;             /* Quality factor (0.707 = Butterworth) */
	float GainDB;        /* Gain in dB (Shelf filters only) */

	/* Biquad coefficients (normalized by a0) */
	float b0, b1, b2;
	float a1, a2;

	/* Delay line (Direct Form I) */
	float x1, x2;        /* previous inputs */
	float y1, y2;        /* previous outputs */

	uint8_t Reset;
} oFilter_t;

/* ============================================================
 * Moving Average Filter
 * ============================================================ */
#define MOVEAVERAGE_MAX_DATACOUNT  1000

#define MOVEAVERAGE_INITIALIZER(buffer, sizeofbuffer, datatype) { \
	.pBuffer = (uint8_t *)buffer,\
	.LengthOfBuffer = MATH_MIN(sizeofbuffer / SIZE_OF_DATATYPE(datatype), MOVEAVERAGE_MAX_DATACOUNT),\
	.MaxSumCount = MATH_MIN(sizeofbuffer / SIZE_OF_DATATYPE(datatype), MOVEAVERAGE_MAX_DATACOUNT),\
	.CountOfData = 0,\
	.DataType = datatype,\
	.IndexOfData = 0,\
	.SumOfData.d = 0,\
	.IsFull = 0,\
	.Reset = 1,\
}

typedef struct{
	uint8_t *pBuffer;
	uint32_t IndexOfData;
	uint32_t CountOfData;
	uint32_t MaxSumCount;
	uint32_t LengthOfBuffer;
	oDataType_t DataType;

	union {
	    int64_t   s64;
	    uint64_t  u64;
	    float     f;
	    double    d;
	} SumOfData;

	uint8_t Reset;
	uint8_t IsFull;
}oMoveAverage_t;

/* ============================================================
 * Filter Functions
 * ============================================================ */
extern float oFilter_LPF(oFilter_t *pFilter, float input);
extern float oFilter_HPF(oFilter_t *pFilter, float input);
extern float oFilter_BPF(oFilter_t *pFilter, float input);
extern float oFilter_LSF(oFilter_t *pFilter, float input);
extern float oFilter_HSF(oFilter_t *pFilter, float input);

extern uint8_t oMoveAverage_SetData(oMoveAverage_t *pAverage, void *pData);
extern uint8_t oMoveAverage_GetData(oMoveAverage_t *pAverage, void *pData);
extern oRange_t oMoveAverage_GetRange(oMoveAverage_t *pAvg);

#endif /* INC_ONE_FILTER_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_Filter.h)

2026-09-17 | v0.2
	- oFilter_t 와 5 개 filter 함수를 double -> float (FPU 단정도 전용). 계수 산출식만 double 유지
	- 주의: fc/fs 가 1e-3 수준이면 DF-I 재귀 상태에 float 반올림이 누적될 수 있다.
	  double 판과 출력 대조 필요 — 어긋나면 Transposed DF-II 로 (연산 수 동일)
*/
