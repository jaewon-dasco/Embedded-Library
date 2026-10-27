/*
 * ONE_FFT.c
 *
 *  Version: 0.3 (2026-09-17)
 */

#include <math.h>
#include <string.h>
#include "ONE_FFT.h"

/* ============================================================
 * Twiddle table precompute (cos, sin interleaved, N/2 pair)
 * ============================================================ */
void oFFT_Init(oFFT_t *pFft, float *pTwiddles, float *pScratch, uint32_t N)
{
	uint32_t i;

	pFft->pTwiddles = pTwiddles;
	pFft->pScratch  = pScratch;
	pFft->N         = N;

	for(i = 0; i < N/2; i++){
		float angle = (float)(-2.0 * MATH_PI * (double)i / (double)N);
		pTwiddles[2*i]     = cosf(angle);
		pTwiddles[2*i + 1] = sinf(angle);
	}

	pFft->Ready = 1;
}

/* In-place radix-2 DIT complex FFT — L-point.
 *   pTw     : W_N^i (i = 0..TableN/2-1) 테이블. L < TableN 이면 stride 를 키워 재사용한다
 *             (W_L^k = W_N^{k*TableN/L} 이므로 단계별 stride 가 TableN/size 로 같아진다)
 *   pData   : interleaved complex [Re0,Im0, ...], 길이 2*L
 *   Sign    : +1 정변환, -1 역변환(켤레 twiddle, 스케일 없음) */
static void oFFT_ComplexCore(const float *pTw, uint32_t TableN, float *pData, uint32_t L, float Sign)
{
	uint32_t i, j, k, size, half, i1, i2;
	float wr, wi, xr, xi, tr, ti;

	/* Bit-reversal permutation */
	j = 0;
	for(i = 1; i < L; i++){
		uint32_t bit = L >> 1;
		while(j & bit){ j ^= bit; bit >>= 1; }
		j ^= bit;
		if(i < j){
			tr = pData[2*i];     pData[2*i]     = pData[2*j];     pData[2*j]     = tr;
			ti = pData[2*i + 1]; pData[2*i + 1] = pData[2*j + 1]; pData[2*j + 1] = ti;
		}
	}

	/* Stage 1 (size=2) — twiddle 이 (1, 0) 이라 복소 곱셈이 전부 항등식이다.
	   특수화해서 실수 곱셈 4*(L/2) 회를 통째로 없앤다 (결과는 비트 동일) */
	for(i = 0; i < L; i += 2){
		xr = pData[2*i + 2];
		xi = pData[2*i + 3];
		pData[2*i + 2] = pData[2*i]     - xr;
		pData[2*i + 3] = pData[2*i + 1] - xi;
		pData[2*i]     = pData[2*i]     + xr;
		pData[2*i + 1] = pData[2*i + 1] + xi;
	}

	/* Stage 2 (size=4) — twiddle 참값이 (1,0)·(0,-Sign) 이라 곱셈이 필요 없다.
	   테이블의 cos(-pi/2) 는 -4.4e-08 이라 비트 동일은 아니고, 참값을 쓰므로 더 정확하다 */
	if(L >= 4){
		for(i = 0; i < L; i += 4){
			/* k=0 : wr=1, wi=0 */
			i1 = i;
			i2 = i + 2;
			tr = pData[2*i2];
			ti = pData[2*i2 + 1];
			pData[2*i2]     = pData[2*i1]     - tr;
			pData[2*i2 + 1] = pData[2*i1 + 1] - ti;
			pData[2*i1]     = pData[2*i1]     + tr;
			pData[2*i1 + 1] = pData[2*i1 + 1] + ti;

			/* k=1 : wr=0, wi=-Sign */
			i1 = i + 1;
			i2 = i + 3;
			xr = pData[2*i2];
			xi = pData[2*i2 + 1];
			tr = xi * Sign;
			ti = -xr * Sign;
			pData[2*i2]     = pData[2*i1]     - tr;
			pData[2*i2 + 1] = pData[2*i1 + 1] - ti;
			pData[2*i1]     = pData[2*i1]     + tr;
			pData[2*i1 + 1] = pData[2*i1 + 1] + ti;
		}
	}

	/* Stage 3.. — twiddle 루프(k)를 블록 루프(i) 바깥으로 올린다.
	   원래는 버터플라이마다 k*step 정수 곱셈과 twiddle 로드 2 회가 붙었는데,
	   이 순서면 블록 수(L/size)만큼 같은 wr/wi 를 재사용한다 */
	for(size = 8; size <= L; size <<= 1){
		const float *pw = pTw;
		uint32_t twStride = (TableN / size) * 2;

		half = size >> 1;

		for(k = 0; k < half; k++){
			wr = pw[0];
			wi = pw[1] * Sign;
			pw += twStride;

			for(i = k; i < L; i += size){
				i1 = i;
				i2 = i + half;
				xr = pData[2*i2];
				xi = pData[2*i2 + 1];
				tr = xr*wr - xi*wi;
				ti = xr*wi + xi*wr;
				pData[2*i2]     = pData[2*i1]     - tr;
				pData[2*i2 + 1] = pData[2*i1 + 1] - ti;
				pData[2*i1]     = pData[2*i1]     + tr;
				pData[2*i1 + 1] = pData[2*i1 + 1] + ti;
			}
		}
	}
}

void oFFT_Complex(oFFT_t *pFft, float *pData, int32_t Direction)
{
	oFFT_ComplexCore(pFft->pTwiddles, pFft->N, pData, pFft->N, (Direction < 0) ? -1.0f : 1.0f);
}

/* Real FFT — CMSIS arm_rfft_fast_f32 packed 포맷 호환
 *   forward: pIn=real[N]     -> pOut=packed[N]
 *   inverse: pIn=packed[N]   -> pOut=real[N]
 *   packed: pOut[0]=Re(X[0]), pOut[1]=Re(X[N/2]), pOut[2k]/pOut[2k+1]=Re/Im(X[k]) (k=1..N/2-1)
 *
 * 실수 N 점을 허수부 0 으로 채워 N 점 복소 FFT 를 돌리던 것을, 실수 열을 그대로
 * M=N/2 개의 복소수(짝수 인덱스=Re, 홀수=Im)로 읽어 M 점 복소 FFT + split 후처리로 바꿨다.
 * 버터플라이가 (N/2)log2(N) 에서 (M/2)log2(M) 로 줄고, 스크래치 버퍼(2N float)와
 * 채우기·추출 루프도 사라진다. split 에 쓰는 W_N^k 는 기존 twiddle 테이블 그대로다.
 *
 * G[k] = FFT_M(z)[k],  Even = (G[k]+conj(G[M-k]))/2,  Odd = (G[k]-conj(G[M-k]))/2
 *   X[k]   = Even - j*W_N^k*Odd
 *   X[M-k] = conj(Even) - j*conj(W_N^k*Odd)
 * k=0 이 X[0] 과 X[M](둘 다 실수)을 함께 준다. */
void oFFT_RealFast(oFFT_t *pFft, float *pIn, float *pOut, int32_t Inverse)
{
	const float *pTw = pFft->pTwiddles;
	uint32_t N = pFft->N;
	uint32_t M = N >> 1;
	uint32_t k, mk;
	float gkr, gki, gmr, gmi, er, ei, orr, oi, cr, ci, tr, ti;

	if(!Inverse){
		/* pIn==pOut 이면 자기복사라 건너뛴다 — in-place 호출을 허용한다 */
		if(pOut != pIn){
			memcpy(pOut, pIn, N * sizeof(float));	/* pIn 보존 — 복소 FFT 는 in-place 다 */
		}
		oFFT_ComplexCore(pTw, N, pOut, M, 1.0f);

		/* split — (k, M-k) 쌍을 함께 처리해 in-place 로 끝낸다 */
		for(k = 0; k <= (M >> 1); k++){
			mk = (k == 0) ? 0 : (M - k);

			gkr = pOut[2*k];      gki = pOut[2*k + 1];
			gmr = pOut[2*mk];     gmi = pOut[2*mk + 1];

			er  = 0.5f * (gkr + gmr);
			ei  = 0.5f * (gki - gmi);
			orr = 0.5f * (gkr - gmr);
			oi  = 0.5f * (gki + gmi);

			cr = pTw[2*k];  ci = pTw[2*k + 1];
			tr = cr*orr - ci*oi;
			ti = cr*oi  + ci*orr;

			if(k == 0){
				/* X[0]·X[M] 은 둘 다 실수라 packed 첫 두 칸에 나란히 들어간다 */
				pOut[0] = er + ti;
				pOut[1] = er - ti;
			}
			else{
				float xkr  = er + ti,  xki  =  ei - tr;
				float xmkr = er - ti,  xmki = -ei - tr;

				pOut[2*k]  = xkr;   pOut[2*k + 1]  = xki;
				if(mk != k){
					pOut[2*mk] = xmkr;  pOut[2*mk + 1] = xmki;
				}
			}
		}
	}
	else{
		float invM = 1.0f / (float)M;	/* 역 M 점 FFT 의 스케일 — 실수 IDFT 의 1/N 과 동치다 */
		uint32_t i;

		/* pIn==pOut 이면 자기복사라 건너뛴다 — in-place 호출을 허용한다 (위 forward 분기 참조) */
		if(pOut != pIn){
			memcpy(pOut, pIn, N * sizeof(float));
		}

		/* merge — split 의 역. X[k]·X[M-k] 에서 Even·Odd 를 복원하고 G 를 되돌린다 */
		for(k = 0; k <= (M >> 1); k++){
			float xkr, xki, xmkr, xmki, odr, odi;

			mk = (k == 0) ? 0 : (M - k);

			if(k == 0){
				xkr = pOut[0];  xki = 0;
				xmkr = pOut[1]; xmki = 0;
			}
			else{
				xkr  = pOut[2*k];   xki  = pOut[2*k + 1];
				xmkr = pOut[2*mk];  xmki = pOut[2*mk + 1];
			}

			er =  0.5f * (xkr + xmkr);
			ti =  0.5f * (xkr - xmkr);
			ei =  0.5f * (xki - xmki);
			tr = -0.5f * (xki + xmki);

			/* Odd = conj(W_N^k) * T */
			cr = pTw[2*k];  ci = pTw[2*k + 1];
			odr = cr*tr + ci*ti;
			odi = cr*ti - ci*tr;

			gkr = er + odr;   gki = ei + odi;
			gmr = er - odr;   gmi = -(ei - odi);

			pOut[2*k]  = gkr;   pOut[2*k + 1]  = gki;
			if(mk != k){
				pOut[2*mk] = gmr;   pOut[2*mk + 1] = gmi;
			}
		}

		oFFT_ComplexCore(pTw, N, pOut, M, -1.0f);

		for(i = 0; i < N; i++){
			pOut[i] *= invM;
		}
	}
}
