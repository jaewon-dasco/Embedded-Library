/*
 * ONE_IMU.h
 *
 *  Created on: 2026-09-17
 *      Author: JONE
 *
 * 6DOF IMU 자세 추정 (가속도 3축 + 자이로 3축) — 상보필터 / Madgwick / Mahony
 *
 * 각도 규약이 알고리즘군에 따라 다르다. 섞어 쓰기 전에 반드시 확인할 것:
 *
 *   상보필터        Roll ±90 도  — 축이 수평면과 이루는 각(2축 경사계). 위아래 구분 불가
 *   Madgwick·Mahony Roll ±180 도 — 쿼터니언에서 뽑은 ZYX Euler. 360 도 전체를 구분
 *
 * 상보필터는 Euler 각을 직접 적분하고 oMath_AccelationToRPY 로 보정하는데, 그 식의
 * 분모가 항상 양수라 ±90 도로 접힌다. 쿼터니언 계열은 Z 부호가 살아 있어 접히지 않는다.
 * Pitch 는 어느 쪽이든 ±90 도다 — 고도각이라 그게 물리적으로 맞다
 */

#ifndef INC_ONE_IMU_H_
#define INC_ONE_IMU_H_

#define ONE_IMU_VERSION     0.7

#include "ONE_Math.h"

/* 6DOF IMU 자세 추정 상태.
 * Pitch·Roll 은 중력 벡터가 절대 기준이라 드리프트가 없고, Yaw 는 중력에 대해
 * 불변이라 가속도로 관측할 수 없다 — 지자기 센서가 없으면 자이로 적분한 상대값뿐이다.
 * 입력 각속도·가속도는 이미 보드 축이어야 한다 — 실장 방향 보정은 드라이버의
 * oAxisMap_t 가 맡는다. 부호가 틀리면 정지 시엔 정상이고 회전 중에만 각도가 튄다.
 *
 * 주의: double 멤버를 가지므로 pack(1) 구조체 안에 두면 안 된다 — 비정렬 오프셋에
 * 놓이고 그 주소를 넘기면 Cortex-M 계열에서 폴트가 난다. */
typedef struct{
	oRPY_t		Angle;           /* deg — Pitch·Roll 절대, Yaw 상대 */
	oVector3_t	GyroBias;        /* mdps — 정지 중 자동 추정, 자이로에서 뺀다 */
	double		Quaternion[4];   /* Madgwick·Mahony 상태 (w, x, y, z) — 상보필터는 쓰지 않는다 */
	double		Beta;            /* Madgwick 경사 이득 — 클수록 가속도를 빨리 따라간다 */
	double		Kp;              /* Mahony 비례 이득 — Madgwick 의 Beta 에 대응 */
	double		Ki;              /* Mahony 적분 이득 — 0 이면 적분항 없음 */
	double		IntegralError[3];/* Mahony 적분 누적 [rad/s] */
	double		Tau;             /* 상보필터 시정수 [s] — 클수록 자이로를 더 신뢰 */
	double		AccelTrust;      /* 0~1 — 직전 크기 게이팅 결과 (읽기 전용) */
	double		StillThreshold;  /* dps — 자이로 크기가 이 이하면 정지로 본다 */
	double		BiasTau;         /* s — 바이어스 추정 시정수 (온도 드리프트 추종 속도) */
	double		StillTime;       /* s — 정지 지속 시간 */
	uint32_t	CountOfBias;       /* 바이어스 갱신 횟수 (초기 수렴 가속용) */
	uint32_t	Timer;             /* 이전 갱신 tick (TICKBASE_SYSTICK) */
	uint8_t		IsInitialized;
}oIMU_6DOF_t;

#define IMU_DEFAULT_TAU     1.0   /* s — 진동 환경에서 가속도 잡음 억제용 기본값 */
#define IMU_STILL_DPS       2.0   /* dps — 미교정 상태 임계 (GyroBias 0 이라 raw 와 동일) */
#define IMU_STILL_DPS_CAL   0.5   /* dps — 바이어스 확보 후 임계. 보정값 기준이라 조일 수 있다 */
#define IMU_BIAS_TAU        30.0  /* s — 바이어스 지수평활 시정수 (온도 추종 지연 0.0017 dps) */
#define IMU_STILL_SETTLE    1.0   /* s — 이만큼 연속 정지해야 바이어스 갱신 시작 */
#define IMU_MADGWICK_BETA   0.1   /* rad/s — Madgwick 경사 이득 기본값 */
#define IMU_MAHONY_KP       0.5   /* 1/s — Mahony 비례 이득 기본값 */
#define IMU_MAHONY_KI       0.0   /* 1/s^2 — 적분항. 동적 바이어스 교정과 역할이 겹쳐 기본 0 */

/* 크기 게이팅 — 정지 중이면 가속도 벡터 크기가 1 g 다. 벗어난 양이 운동가속도이므로
 * 그만큼 가속도 보정 이득을 줄인다. 계단이 아니라 연속으로 줄이는 이유는 진동 중
 * 크기가 늘 흔들려 임계로 끊으면 보정이 점멸하며 각도가 튀기 때문이다.
 * DEADZONE 은 ISM330 zero-g 오프셋 ±65 mg(max) 를 덮는 폭 — 실기 산포 측정 후 확정.
 * 한계: 지속되는 일정 가속은 크기가 1 g 근처라 통과한다 (6DOF 로는 원리적 구분 불가) */
#define IMU_ACCEL_DEADZONE  0.07  /* g — 이 이내 오차는 신뢰도 1 */
#define IMU_ACCEL_RAMP      0.15  /* g — DEADZONE 부터 이만큼 더 벗어나면 신뢰도 0 */

extern void oIMU_Reset(oIMU_6DOF_t *pIMU, double Tau);

/* 두 알고리즘은 상태 구조체와 자이로 바이어스 교정을 공유하지만 자세 원본이 다르다 —
 * 상보필터는 Angle, Madgwick 은 Quaternion. 한 인스턴스에 둘을 번갈아 호출하지 말 것 */

/* 상보필터 — Euler 각에 각속도를 직접 적분한다. 파라미터가 Tau 하나라 다루기 쉽지만
 * Euler 각속도 변환의 tan(Pitch) 결합항을 생략하므로 Pitch 가 ±90 도에 가까울수록
 * 회전 중 축간 오차가 커진다. 준정적 기울기 감시용 */
extern oRPY_t oIMU_UpdateComplementary_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration);

/* Madgwick — 쿼터니언 경사하강. Roll 은 ±180 도(위 규약 참조). 임의 자세·다축 동시 회전에 정확하고 짐벌락이 없다.
 * Roll·Pitch 는 쿼터니언이 예측한 중력 방향을 oMath_AccelationToRPY 에 넣어 뽑으므로
 * 상보필터와 각도 규약이 같다. Yaw 는 6DOF 라 관측 불가 — 자이로 적분값이다 */
extern oRPY_t oIMU_UpdateMadgwick_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration);

/* Mahony — 쿼터니언 상보필터. Roll 은 ±180 도(위 규약 참조). 측정 중력과 예측 중력의 외적을 오차로 쓰고 Kp 를 곱해
 * 각속도에 더한다. 오차에 비례하므로 정지 시 보정량이 0 으로 수렴한다 — Madgwick 은
 * 경사를 정규화해 늘 일정량을 밀어 정지 중에도 미세 진동이 남는다.
 * Kp 를 런타임에 흔들 수 있어 크기 게이팅과 결합하기 쉽다 */
extern oRPY_t oIMU_UpdateMahony_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration);

/* dt 를 호출부가 주는 형태. FIFO 로 여러 샘플을 한 번에 처리할 때 필요하다 —
 * 위 함수는 dt 를 tick(ms 정수) 차이로 재므로, 같은 ms 안에 두 번 부르면 dt 가 0 이 되어
 * 두 번째 샘플이 조용히 버려진다. 배치 처리에서는 반드시 이 함수를 쓴다.
 * DeltaTime 이 0 이하이면 자세를 그대로 돌려준다 (첫 호출은 예외 — 가속도로 시드) */
extern oRPY_t oIMU_UpdateMahony_6DOF_Interval(oIMU_6DOF_t *pIMU, oVector3_t GyroRate,
                                              oVector3_t Acceleration, double DeltaTime);

#endif /* INC_ONE_IMU_H_ */

/* History

2026-09-17 | v0.1
	- ONE_Math v0.3 에서 분리. oMath_AccelationToRPY 는 순수 수학이라 ONE_Math 에 남는다

2026-09-17 | v0.2
	- oIMU_Update6DOF -> oIMU_UpdateComplementary_6DOF (알고리즘을 이름에 드러냄)
	- oIMU_UpdateMadgwick_6DOF 추가. 첫 호출에서 가속도로 쿼터니언을 시드한다 —
	  항등에서 출발하면 초기 오차가 커 beta 수렴에 수십 초가 걸린다
	- 자이로 바이어스 교정은 두 알고리즘이 공유한다

2026-09-17 | v0.3
	- oIMU_UpdateMahony_6DOF 추가. 세 알고리즘이 바이어스 교정·크기 게이팅을 공유한다
	- 크기 게이팅 도입 — |a| 가 1 g 에서 벗어난 만큼 가속도 보정 이득을 낮춘다.
	  상보필터는 (1-Alpha), Madgwick 은 Beta, Mahony 는 Kp 에 곱한다
	- DEADZONE·RAMP 는 데이터시트 오프셋 기준 잠정값 — 실기 산포 측정 후 확정 필요
	- IMU_BIAS_TAU 60 -> 30 s. ZRO 온도계수 0.005 dps/°C 기준 추종 지연이 절반이 되고,
	  늘어나는 잡음은 √2 배지만 원래 잡음 바닥(0.02 dps)보다 여전히 훨씬 작다
2026-09-21 | v0.4
	- GyroSign 제거 — 실장 방향 보정을 드라이버 oAxisMap_t 로 일원화했다.
	  같은 일을 하는 기제가 둘이면 서로 어긋났을 때 어느 쪽이 맞는지 알 수 없다

2026-09-22 | v0.5
	- oIMU_UpdateMahony_6DOF_Interval 추가 — dt 를 인자로 받는다.
	  기존 함수는 벽시계 dt 를 구해 이 함수를 부르는 래퍼가 되어 동작이 같다
	- 배치 처리에서 StillTime 누적과 BiasTau 평활도 실제 샘플 간격으로 정확해진다

2026-09-22 | v0.6
	- Madgwick·Mahony 의 Roll·Pitch 를 쿼터니언에서 직접 뽑는다.
	  종전에는 예측 중력을 oMath_AccelationToRPY 에 넣어 상보필터와 규약을 맞췄는데,
	  그 식이 ±90 도로 접혀 360 도 회전에서 위아래를 구분하지 못했다.
	  규약을 맞춘 이유(3종 알고리즘 동시 비교)는 Mahony 채택으로 사라졌다
	- Pitch 부호는 교과서형(asin(2(q0q2-q1q3)))이 아니라 종전 출력과 같게 유지했다 —
	  뒤집으면 실장 방향 판정이 어긋난다
	- asin 입력 클램프 추가. NaN 이 한 번 들어가면 자세가 영구히 오염된다

2026-09-22 | v0.7
	- 정지 판정을 raw 가 아닌 바이어스 보정값 기준으로 바꿨다. raw 는 임계가 개체
	  ZRO(최악 3축 합성 5.2 dps)를 넘어야 해 크게 잡을 수밖에 없었다
	- IMU_STILL_DPS_CAL 추가. 바이어스를 확보한 뒤 StillThreshold 를 이 값으로
	  낮추면 느린 실제 움직임이 정지로 섞이는 것을 4 배 더 잘 막는다
*/
