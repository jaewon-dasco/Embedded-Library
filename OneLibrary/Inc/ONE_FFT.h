/*
 * ONE_FFT.h
 *
 *  Created on: Jul 3, 2026
 *      Author: JONE
 *
 * Radix-2 DIT FFT (float, in-place) — CMSIS-DSP 를 쓸 수 없는 타깃용
 * Real FFT wrapper — CMSIS arm_rfft_fast_f32 packed 포맷 호환
 */

#ifndef INC_ONE_FFT_H_
#define INC_ONE_FFT_H_

#define ONE_FFT_VERSION  0.3

#include "ONE_Math.h"

/* ============================================================
 * oFFT_t — N-point 실수 FFT 컨텍스트
 * ============================================================
 * pTwiddles : N개 float 버퍼 (N/2 pair of cos,sin) — 호출자 할당, static 권장
 * pScratch  : v0.2 부터 미사용 (NULL 허용). 실수 FFT 가 출력 버퍼에서 in-place 로
 *             처리하도록 바뀌어 2*N float 작업버퍼가 필요 없어졌다 — 인자는 기존
 *             호출부 호환을 위해 남겨둔다
 * N         : FFT 크기 (2의 거듭제곱)
 *
 * oFFT_RealFast 는 pIn 을 보존하고 pOut 만 쓴다 (CMSIS 는 pIn 을 파괴한다).
 * pIn==pOut 로 호출해도 된다 — 내부에서 자기복사를 건너뛰고 pOut 에서 in-place 로
 * 처리하므로, RAM 이 빠듯하면 버퍼 하나(N float)를 아낄 수 있다.
 *
 * Usage:
 *   static float Twiddles[FFT_SIZE];
 *   static oFFT_t Fft;
 *
 *   oFFT_Init(&Fft, Twiddles, NULL, FFT_SIZE);      // 부팅 1회
 *   oFFT_RealFast(&Fft, Input, Output, 0);          // forward
 *   oFFT_RealFast(&Fft, Output, Input, 1);          // inverse
 */
typedef struct {
	float    *pTwiddles;
	float    *pScratch;
	uint32_t N;
	uint8_t  Ready;
} oFFT_t;

extern void oFFT_Init(oFFT_t *pFft, float *pTwiddles, float *pScratch, uint32_t N);
extern void oFFT_Complex(oFFT_t *pFft, float *pData, int32_t Direction);
extern void oFFT_RealFast(oFFT_t *pFft, float *pIn, float *pOut, int32_t Inverse);

#endif /* INC_ONE_FFT_H_ */

/* History

2026-07-03 | v0.1
	- baseline (ONE_FFT.h) — Mi_Measurement.c 자체 FFT 구현 추출

2026-09-17 | v0.2
	- 실수 FFT 를 N 점 복소 FFT(허수부 0 채움)에서 M=N/2 점 복소 FFT + split 후처리로 교체.
	  split 용 W_N^k 가 기존 twiddle 테이블과 같은 값이라 테이블 추가 없음
	- 내부 루프 개선: twiddle 루프를 블록 루프 바깥으로, 역변환 부호를 최내곽에서 제거,
	  첫 스테이지 특수화
	- pScratch 미사용(NULL 허용) — 호출부가 2*N float 버퍼를 해제할 수 있다
	- 실측 (M33 160 MHz, -O0, 2048-pt 정방향): 13262 -> 5614 us (2.36 배).
	  CMSIS arm_rfft_fast_f32 = 3534 us. 왕복 복원오차 3.28e-07 (CMSIS 2.98e-07)

2026-09-17 | v0.3
	- oFFT_RealFast: in-place 호출(pIn==pOut) 허용 명시 + memcpy 자기복사 가드
	- size=4 스테이지 특수화 — twiddle 참값이 (1,0)·(0,-Sign) 이라 곱셈 불필요.
	  테이블의 cos(-pi/2)=-4.4e-08 을 참값 0 으로 대체하므로 비트 동일은 아니다(더 정확)
*/
