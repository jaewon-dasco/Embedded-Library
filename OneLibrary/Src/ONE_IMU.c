/*
 * ONE_IMU.c
 *
 *  Version: 0.7 (2026-09-17)
 */
#include <math.h>
#include <string.h>
#include "ONE_IMU.h"
#include "ONE_Time.h"

/* 상태 초기화 — Tau 가 0 이하면 기본값을 쓴다. 다음 갱신 호출이 가속도로 자세를
 * 다시 잡으므로 자세 기준점을 새로 잡고 싶을 때도 호출한다 */
void oIMU_Reset(oIMU_6DOF_t *pIMU, double Tau)
{
	if(pIMU == NULL){
		return;
	}

	memset(pIMU, 0, sizeof(oIMU_6DOF_t));

	pIMU->Tau = (Tau > 0) ? Tau : IMU_DEFAULT_TAU;
	pIMU->StillThreshold = IMU_STILL_DPS;
	pIMU->BiasTau = IMU_BIAS_TAU;
	pIMU->Beta = IMU_MADGWICK_BETA;
	pIMU->Kp = IMU_MAHONY_KP;
	pIMU->Ki = IMU_MAHONY_KI;
	pIMU->AccelTrust = 1.0;
	pIMU->Quaternion[0] = 1;
}

/* 경과 시간 산출 + 첫 호출 판정. 0 을 돌려주면 호출부는 그대로 빠져나가야 한다 */
static double oIMU_DeltaTime(oIMU_6DOF_t *pIMU)
{
	uint32_t Now = oTMR_GetTick(TICKBASE_SYSTICK);
	double DeltaTime = (double)oTMR_Interval(Now, pIMU->Timer) / 1000.0;

	pIMU->Timer = Now;
	return DeltaTime;
}

/* 정지 판정 + 바이어스 추정 — 자이로는 정지 시 참값이 0 이라 그때 읽히는 값이 곧
 * 바이어스다. 가속도계엔 같은 방법을 못 쓴다: 정지해도 참값이 0 이 아니라 중력이고
 * 방향을 모른다 */
static void oIMU_UpdateBias(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, double DeltaTime)
{
	/* 바이어스를 뺀 값으로 판정한다. raw 로 하면 임계가 개체의 ZRO(최악 ±3 dps/축,
	   3축 합성 5.2 dps)를 넘어야 해서 크게 잡을 수밖에 없고, 그만큼 느린 실제
	   움직임이 정지로 섞인다. 되먹임 우려는 추정 시정수가 30 초라 1 초 창 안에서
	   판정 조건을 따라갈 수 없어 성립하지 않는다.
	   미교정 상태에서는 GyroBias 가 0 이라 raw 와 같은 값이 된다 */
	double dx = GyroRate.X - pIMU->GyroBias.X;
	double dy = GyroRate.Y - pIMU->GyroBias.Y;
	double dz = GyroRate.Z - pIMU->GyroBias.Z;
	double Magnitude = sqrt(dx * dx + dy * dy + dz * dz) / 1000.0;

	if(Magnitude > pIMU->StillThreshold){
		pIMU->StillTime = 0;
		return;
	}

	pIMU->StillTime += DeltaTime;

	/* 잠깐 멈춘 구간이 섞이지 않도록 일정 시간 연속 정지한 뒤에만 갱신한다 */
	if(pIMU->StillTime < IMU_STILL_SETTLE){
		return;
	}

	{
		double BiasAlpha = DeltaTime / (pIMU->BiasTau + DeltaTime);
		double AverageAlpha;

		if(pIMU->CountOfBias < 0xFFFFFFFF){
			pIMU->CountOfBias++;
		}

		/* 초기에는 누적평균(1/n)이 지수평활보다 이득이 커서 빨리 수렴하고,
		   n 이 커지면 자연스럽게 BiasTau 지수평활로 넘어간다 */
		AverageAlpha = 1.0 / (double)pIMU->CountOfBias;
		if(AverageAlpha > BiasAlpha){
			BiasAlpha = AverageAlpha;
		}

		pIMU->GyroBias.X += (GyroRate.X - pIMU->GyroBias.X) * BiasAlpha;
		pIMU->GyroBias.Y += (GyroRate.Y - pIMU->GyroBias.Y) * BiasAlpha;
		pIMU->GyroBias.Z += (GyroRate.Z - pIMU->GyroBias.Z) * BiasAlpha;
	}
}

/* 크기 게이팅 — 정지 중이면 |a| 가 1 g 다. 벗어난 양이 곧 운동가속도이므로 그만큼
 * 가속도 보정 신뢰도를 낮춘다. 세 알고리즘이 이 값을 각자의 이득에 곱해서 쓴다 */
static double oIMU_AccelTrust(oVector3_t Acceleration)
{
	double Error = sqrt(Acceleration.X * Acceleration.X
	                  + Acceleration.Y * Acceleration.Y
	                  + Acceleration.Z * Acceleration.Z) / 1000000.0;	/* µG -> g */

	Error = fabs(Error - 1.0);

	if(Error <= IMU_ACCEL_DEADZONE){
		return 1.0;
	}

	Error -= IMU_ACCEL_DEADZONE;
	if(Error >= IMU_ACCEL_RAMP){
		return 0.0;
	}

	return 1.0 - Error / IMU_ACCEL_RAMP;
}

/* 바이어스 보정된 각속도 [rad/s] — 축 보정은 드라이버에서 이미 끝났다 */
static void oIMU_CorrectedRate(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, double *pRate)
{
	pRate[0] = DEGREE_TO_RADIAN((GyroRate.X - pIMU->GyroBias.X) / 1000.0);
	pRate[1] = DEGREE_TO_RADIAN((GyroRate.Y - pIMU->GyroBias.Y) / 1000.0);
	pRate[2] = DEGREE_TO_RADIAN((GyroRate.Z - pIMU->GyroBias.Z) / 1000.0);
}

/* ============================================================
 * 상보필터
 *   Angle = a(Angle + w*dt) + (1-a)*AccelAngle,  a = Tau / (Tau + dt)
 *   짧은 시간에는 자이로(잡음 적음·드리프트 있음)를, 긴 시간에는 가속도(드리프트
 *   없음·진동에 취약)를 따르게 해서 양쪽 약점을 서로 덮는다
 * ============================================================ */
oRPY_t oIMU_UpdateComplementary_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration)
{
	oRPY_t Accel = {0, 0, 0};
	double DeltaTime, Alpha, Rate[3];

	if(pIMU == NULL){
		return Accel;
	}

	Accel = oMath_AccelationToRPY(Acceleration);

	/* 첫 호출은 융합할 과거값이 없다 — 가속도 해로 초기화하고 기준 tick 만 잡는다 */
	if(!pIMU->IsInitialized){
		pIMU->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
		pIMU->Angle = Accel;
		pIMU->Angle.Yaw = 0;
		pIMU->IsInitialized = 1;
		return pIMU->Angle;
	}

	DeltaTime = oIMU_DeltaTime(pIMU);

	/* 같은 tick 안에서 다시 불리면 적분할 시간이 없다 */
	if(DeltaTime <= 0){
		return pIMU->Angle;
	}

	oIMU_UpdateBias(pIMU, GyroRate, DeltaTime);
	oIMU_CorrectedRate(pIMU, GyroRate, Rate);

	pIMU->AccelTrust = oIMU_AccelTrust(Acceleration);

	/* Alpha 는 자이로 가중치다. 가속도 몫 (1-Alpha) 에 신뢰도를 곱해 깎고 나머지는
	   자이로로 돌린다 — 신뢰도 0 이면 그 구간은 순수 적분이 된다 */
	Alpha = 1.0 - (1.0 - pIMU->Tau / (pIMU->Tau + DeltaTime)) * pIMU->AccelTrust;

	pIMU->Angle.Roll  = Alpha * (pIMU->Angle.Roll  + RADIAN_TO_DEGREE(Rate[0]) * DeltaTime)
	                  + (1.0 - Alpha) * Accel.Roll;
	pIMU->Angle.Pitch = Alpha * (pIMU->Angle.Pitch + RADIAN_TO_DEGREE(Rate[1]) * DeltaTime)
	                  + (1.0 - Alpha) * Accel.Pitch;

	/* Yaw 는 보정할 절대 기준이 없어 적분만 한다 — 바이어스를 빼도 잔여분은 누적된다 */
	pIMU->Angle.Yaw += RADIAN_TO_DEGREE(Rate[2]) * DeltaTime;

	return pIMU->Angle;
}

/* ============================================================
 * Madgwick (6DOF, 지자기 없음)
 * ============================================================ */

/* 가속도로 쿼터니언을 시드한다. 지구 중력 (0,0,1) 을 측정 방향으로 옮기는 최소 회전.
 * 항등에서 출발하면 초기 오차만큼 beta 로 기어가야 해서 수십 초가 걸린다 */
static void oIMU_SeedQuaternion(oIMU_6DOF_t *pIMU, oVector3_t Acceleration)
{
	double Norm = sqrt(Acceleration.X * Acceleration.X
	                 + Acceleration.Y * Acceleration.Y
	                 + Acceleration.Z * Acceleration.Z);
	double ax, ay, az, AxisNorm, Angle, Sine;

	pIMU->Quaternion[0] = 1;
	pIMU->Quaternion[1] = 0;
	pIMU->Quaternion[2] = 0;
	pIMU->Quaternion[3] = 0;

	if(Norm <= 0){
		return;
	}

	ax = Acceleration.X / Norm;
	ay = Acceleration.Y / Norm;
	az = Acceleration.Z / Norm;

	/* 회전축 = (0,0,1) x (ax,ay,az) = (-ay, ax, 0) */
	AxisNorm = sqrt(ay * ay + ax * ax);
	if(AxisNorm < 1.0e-12){
		if(az < 0){		/* 정확히 뒤집힌 경우 — 임의의 수직축으로 180 도 */
			pIMU->Quaternion[0] = 0;
			pIMU->Quaternion[1] = 1;
		}
		return;
	}

	if(az > 1.0)  az = 1.0;
	if(az < -1.0) az = -1.0;

	Angle = acos(az);
	Sine = sin(Angle / 2.0);

	/* Madgwick 의 예측 중력식 v = [2(q1q3-q0q2), 2(q0q1+q2q3), 2(0.5-q1^2-q2^2)] 은
	   지구->센서 방향 회전을 전제한다. 축각으로 구한 센서->지구 회전의 켤레를 쓴다 */
	pIMU->Quaternion[0] =  cos(Angle / 2.0);
	pIMU->Quaternion[1] = -(-ay / AxisNorm) * Sine;
	pIMU->Quaternion[2] = -( ax / AxisNorm) * Sine;
	pIMU->Quaternion[3] = 0;
}

oRPY_t oIMU_UpdateMadgwick_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration)
{
	oRPY_t Result = {0, 0, 0};
	oVector3_t Gravity;
	double DeltaTime, Rate[3], Norm;
	double q0, q1, q2, q3;
	double qDot0, qDot1, qDot2, qDot3;

	if(pIMU == NULL){
		return Result;
	}

	if(!pIMU->IsInitialized){
		pIMU->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
		oIMU_SeedQuaternion(pIMU, Acceleration);
		pIMU->Angle = oMath_AccelationToRPY(Acceleration);
		pIMU->Angle.Yaw = 0;
		pIMU->IsInitialized = 1;
		return pIMU->Angle;
	}

	DeltaTime = oIMU_DeltaTime(pIMU);

	if(DeltaTime <= 0){
		return pIMU->Angle;
	}

	oIMU_UpdateBias(pIMU, GyroRate, DeltaTime);
	oIMU_CorrectedRate(pIMU, GyroRate, Rate);

	q0 = pIMU->Quaternion[0];
	q1 = pIMU->Quaternion[1];
	q2 = pIMU->Quaternion[2];
	q3 = pIMU->Quaternion[3];

	/* 자이로에 의한 변화율 — qDot = 0.5 * q ⊗ (0, wx, wy, wz) */
	qDot0 = 0.5 * (-q1 * Rate[0] - q2 * Rate[1] - q3 * Rate[2]);
	qDot1 = 0.5 * ( q0 * Rate[0] + q2 * Rate[2] - q3 * Rate[1]);
	qDot2 = 0.5 * ( q0 * Rate[1] - q1 * Rate[2] + q3 * Rate[0]);
	qDot3 = 0.5 * ( q0 * Rate[2] + q1 * Rate[1] - q2 * Rate[0]);

	pIMU->AccelTrust = oIMU_AccelTrust(Acceleration);

	Norm = sqrt(Acceleration.X * Acceleration.X
	          + Acceleration.Y * Acceleration.Y
	          + Acceleration.Z * Acceleration.Z);

	/* 가속도가 0 이거나 크기가 1 g 를 크게 벗어나면 보정할 기준이 없다 —
	   자이로 적분만 한다 (자유낙하·급가속 등) */
	if(Norm > 0 && pIMU->AccelTrust > 0){
		double ax = Acceleration.X / Norm;
		double ay = Acceleration.Y / Norm;
		double az = Acceleration.Z / Norm;
		double s0, s1, s2, s3;
		double _2q0 = 2.0 * q0, _2q1 = 2.0 * q1, _2q2 = 2.0 * q2, _2q3 = 2.0 * q3;
		double _4q0 = 4.0 * q0, _4q1 = 4.0 * q1, _4q2 = 4.0 * q2;
		double _8q1 = 8.0 * q1, _8q2 = 8.0 * q2;
		double q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

		/* 측정 중력과 쿼터니언이 예측한 중력의 차이를 목적함수로 두고 그 경사를 구한다 */
		s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
		s1 = _4q1 * q3q3 - _2q3 * ax + 4.0 * q0q0 * q1 - _2q0 * ay - _4q1
		   + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
		s2 = 4.0 * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2
		   + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
		s3 = 4.0 * q1q1 * q3 - _2q1 * ax + 4.0 * q2q2 * q3 - _2q2 * ay;

		Norm = sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
		if(Norm > 0){
			double Gain = pIMU->Beta * pIMU->AccelTrust;

			qDot0 -= Gain * (s0 / Norm);
			qDot1 -= Gain * (s1 / Norm);
			qDot2 -= Gain * (s2 / Norm);
			qDot3 -= Gain * (s3 / Norm);
		}
	}

	q0 += qDot0 * DeltaTime;
	q1 += qDot1 * DeltaTime;
	q2 += qDot2 * DeltaTime;
	q3 += qDot3 * DeltaTime;

	Norm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	if(Norm <= 0){
		return pIMU->Angle;
	}

	pIMU->Quaternion[0] = q0 / Norm;
	pIMU->Quaternion[1] = q1 / Norm;
	pIMU->Quaternion[2] = q2 / Norm;
	pIMU->Quaternion[3] = q3 / Norm;

	q0 = pIMU->Quaternion[0];
	q1 = pIMU->Quaternion[1];
	q2 = pIMU->Quaternion[2];
	q3 = pIMU->Quaternion[3];

	/* 쿼터니언이 예측한 중력 방향 (센서 축 기준) */
	Gravity.X = 2.0 * (q1 * q3 - q0 * q2);
	Gravity.Y = 2.0 * (q0 * q1 + q2 * q3);
	Gravity.Z = 2.0 * (0.5 - q1 * q1 - q2 * q2);

	/* Roll 은 Z 부호가 들어가는 atan2 라 ±180 도까지 나온다 — oMath_AccelationToRPY
	   를 거치면 분모가 항상 양수라 ±90 도로 접혀 위아래를 구분하지 못한다.
	   Pitch 는 고도각이라 ±90 도가 물리적으로 맞다.
	   부호는 교과서형(asin(2(q0q2-q1q3)))이 아니라 Gravity.X 를 그대로 쓴다 —
	   종전 출력과 부호를 같게 유지해야 실장 방향(MiMeasure_*Axis) 판정이 어긋나지 않는다.
	   주의: Pitch 가 ±90 도에 가까우면 Gravity.Y·Z 가 함께 0 으로 가 Roll 이 불안정해진다.
	   짐벌락이라 Euler 각으로는 피할 수 없다 */
	if(Gravity.X >  1.0){ Gravity.X =  1.0; }
	if(Gravity.X < -1.0){ Gravity.X = -1.0; }	/* asin 이 NaN 을 내면 자세가 영구히 오염된다 */

	pIMU->Angle.Roll  = RADIAN_TO_DEGREE(atan2(Gravity.Y, Gravity.Z));
	pIMU->Angle.Pitch = RADIAN_TO_DEGREE(asin(Gravity.X));

	/* Yaw 는 중력으로 관측 불가 — 쿼터니언에서 직접 뽑는다 (자이로 적분값) */
	pIMU->Angle.Yaw = RADIAN_TO_DEGREE(atan2(2.0 * (q1 * q2 + q0 * q3),
	                                         q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3));

	return pIMU->Angle;
}

/* ============================================================
 * Mahony (6DOF, 지자기 없음)
 *   e = a_meas x a_pred 를 오차로 두고 Kp 를 곱해 각속도에 더한다.
 *   보정량이 오차에 비례하므로 정지하면 0 으로 수렴한다 — 정규화된 경사를
 *   늘 일정량 미는 Madgwick 과 갈리는 지점이다
 * ============================================================ */
/* 벽시계 dt 판 — 호출 간격이 곧 적분 구간일 때 쓴다 */
oRPY_t oIMU_UpdateMahony_6DOF(oIMU_6DOF_t *pIMU, oVector3_t GyroRate, oVector3_t Acceleration)
{
	double DeltaTime = 0;

	/* 미초기화 상태에서는 tick 기준점이 없다 — 0 을 넘겨 시드만 하게 한다 */
	if(pIMU != NULL && pIMU->IsInitialized){
		DeltaTime = oIMU_DeltaTime(pIMU);
	}

	return oIMU_UpdateMahony_6DOF_Interval(pIMU, GyroRate, Acceleration, DeltaTime);
}

oRPY_t oIMU_UpdateMahony_6DOF_Interval(oIMU_6DOF_t *pIMU, oVector3_t GyroRate,
                                       oVector3_t Acceleration, double DeltaTime)
{
	oRPY_t Result = {0, 0, 0};
	oVector3_t Gravity;
	double Rate[3], Norm;
	double q0, q1, q2, q3;
	double qDot0, qDot1, qDot2, qDot3;

	if(pIMU == NULL){
		return Result;
	}

	if(!pIMU->IsInitialized){
		pIMU->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
		oIMU_SeedQuaternion(pIMU, Acceleration);
		pIMU->Angle = oMath_AccelationToRPY(Acceleration);
		pIMU->Angle.Yaw = 0;
		pIMU->IsInitialized = 1;
		return pIMU->Angle;
	}

	if(DeltaTime <= 0){
		return pIMU->Angle;
	}

	oIMU_UpdateBias(pIMU, GyroRate, DeltaTime);
	oIMU_CorrectedRate(pIMU, GyroRate, Rate);
	pIMU->AccelTrust = oIMU_AccelTrust(Acceleration);

	q0 = pIMU->Quaternion[0];
	q1 = pIMU->Quaternion[1];
	q2 = pIMU->Quaternion[2];
	q3 = pIMU->Quaternion[3];

	Norm = sqrt(Acceleration.X * Acceleration.X
	          + Acceleration.Y * Acceleration.Y
	          + Acceleration.Z * Acceleration.Z);

	if(Norm > 0 && pIMU->AccelTrust > 0){
		double ax = Acceleration.X / Norm;
		double ay = Acceleration.Y / Norm;
		double az = Acceleration.Z / Norm;
		double vx = 2.0 * (q1 * q3 - q0 * q2);
		double vy = 2.0 * (q0 * q1 + q2 * q3);
		double vz = 2.0 * (0.5 - q1 * q1 - q2 * q2);
		double ex = ay * vz - az * vy;
		double ey = az * vx - ax * vz;
		double ez = ax * vy - ay * vx;
		double Gain = pIMU->Kp * pIMU->AccelTrust;

		/* Ki 는 기본 0 이다 — 자이로 바이어스 흡수는 oIMU_UpdateBias 가 이미 맡고
		   있어 둘을 동시에 켜면 같은 오차를 두 경로가 나눠 먹으며 서로 밀어낸다 */
		if(pIMU->Ki > 0){
			pIMU->IntegralError[0] += ex * DeltaTime;
			pIMU->IntegralError[1] += ey * DeltaTime;
			pIMU->IntegralError[2] += ez * DeltaTime;

			Rate[0] += pIMU->Ki * pIMU->IntegralError[0];
			Rate[1] += pIMU->Ki * pIMU->IntegralError[1];
			Rate[2] += pIMU->Ki * pIMU->IntegralError[2];
		}

		Rate[0] += Gain * ex;
		Rate[1] += Gain * ey;
		Rate[2] += Gain * ez;
	}

	qDot0 = 0.5 * (-q1 * Rate[0] - q2 * Rate[1] - q3 * Rate[2]);
	qDot1 = 0.5 * ( q0 * Rate[0] + q2 * Rate[2] - q3 * Rate[1]);
	qDot2 = 0.5 * ( q0 * Rate[1] - q1 * Rate[2] + q3 * Rate[0]);
	qDot3 = 0.5 * ( q0 * Rate[2] + q1 * Rate[1] - q2 * Rate[0]);

	q0 += qDot0 * DeltaTime;
	q1 += qDot1 * DeltaTime;
	q2 += qDot2 * DeltaTime;
	q3 += qDot3 * DeltaTime;

	Norm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	if(Norm <= 0){
		return pIMU->Angle;
	}

	pIMU->Quaternion[0] = q0 / Norm;
	pIMU->Quaternion[1] = q1 / Norm;
	pIMU->Quaternion[2] = q2 / Norm;
	pIMU->Quaternion[3] = q3 / Norm;

	q0 = pIMU->Quaternion[0];
	q1 = pIMU->Quaternion[1];
	q2 = pIMU->Quaternion[2];
	q3 = pIMU->Quaternion[3];

	Gravity.X = 2.0 * (q1 * q3 - q0 * q2);
	Gravity.Y = 2.0 * (q0 * q1 + q2 * q3);
	Gravity.Z = 2.0 * (0.5 - q1 * q1 - q2 * q2);

	/* 규약은 Madgwick 과 동일 — 근거는 그쪽 주석 참조 */
	if(Gravity.X >  1.0){ Gravity.X =  1.0; }
	if(Gravity.X < -1.0){ Gravity.X = -1.0; }

	pIMU->Angle.Roll  = RADIAN_TO_DEGREE(atan2(Gravity.Y, Gravity.Z));
	pIMU->Angle.Pitch = RADIAN_TO_DEGREE(asin(Gravity.X));

	pIMU->Angle.Yaw = RADIAN_TO_DEGREE(atan2(2.0 * (q1 * q2 + q0 * q3),
	                                         q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3));

	return pIMU->Angle;
}
