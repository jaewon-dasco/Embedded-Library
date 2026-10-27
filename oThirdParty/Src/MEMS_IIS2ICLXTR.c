/*
 * MEMS_IIS2ICLXTR.c
 *
 *  Version: 0.2 (2026-10-20)
 */

#include "ONE_Common.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"

/* 진단 로그 스위치 — 이 드라이버가 oThirdParty 최초의 로그 사용처다.
   ONE_Serial.h 는 .h 가 아니라 여기(.c)에만 들여온다. 헤더에 넣으면 이 드라이버를
   쓰는 모든 프로젝트가 로깅 의존성을 강제로 떠안기 때문이다.
   oSerial_Log 는 런타임 no-op 이지만 포맷 문자열은 플래시에 남으므로,
   로거가 없는 프로젝트가 걷어낼 수 있도록 컴파일 타임 게이트를 둔다 */
#ifndef IIS2ICLX_LOG_ENABLE
#define IIS2ICLX_LOG_ENABLE   1
#endif

#if IIS2ICLX_LOG_ENABLE
#define IIS2ICLX_LOG(fmt, ...)   oSerial_Log("IIS2IC", fmt, ##__VA_ARGS__)
#else
#define IIS2ICLX_LOG(fmt, ...)   do{}while(0)
#endif
#include "MEMS_IIS2ICLXTR.h"

#include <math.h>

/* ============================================================
 * Recovery — SPI 페리페럴 DeInit/Init 으로 stuck 복구
 * ============================================================ */
oResult_t IIS2ICLX_Recovery(IIS2ICLX_t *pDev)
{
	if(pDev == NULL || pDev->pSPI == NULL){
		return RESULT_NULL;
	}

	HAL_SPI_DeInit(pDev->pSPI);
	if(HAL_SPI_Init(pDev->pSPI) != HAL_OK){
		return RESULT_ERROR;
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

static void IIS2ICLX_OnHalError(IIS2ICLX_t *pDev)
{
	if(pDev->State.CountOfSpiError < 0xFF){
		pDev->State.CountOfSpiError++;
	}

	/* 트랜잭션 단위(ReadReg/WriteReg)가 아니라 여기서만 남긴다 — 폴백 경로가 5ms 주기라
	   배선 결함 시 초당 200줄이 9600bps 링버퍼로 쏟아진다. 여기는 에러가 실제로 누적될
	   때만 불리고 복구 임계로 자연히 제한된다 */
	IIS2ICLX_LOG("HalError count=%u HalErr=0x%08lX",
	             (unsigned int)pDev->State.CountOfSpiError,
	             (unsigned long)(pDev->pSPI ? HAL_SPI_GetError(pDev->pSPI) : 0));

	if(pDev->State.CountOfSpiError >= IIS2ICLX_SPI_ERROR_RECOVERY_THRESHOLD){
		IIS2ICLX_LOG("Recovery (threshold %u)", (unsigned int)IIS2ICLX_SPI_ERROR_RECOVERY_THRESHOLD);
		IIS2ICLX_Recovery(pDev);
	}
}

/* ============================================================
 * SPI Low-Level Read/Write — 1st byte = [R/W][주소], CS는 트랜잭션 단위로 토글
 * ============================================================ */
static oResult_t IIS2ICLX_SpiWrite(IIS2ICLX_t *pDev, uint8_t RegAddress, const uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pDev->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIS2ICLX_SPI_WRITE;

	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, pDev->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pDev->pSPI, &addr, 1, IIS2ICLX_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS2ICLX_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(HAL_SPI_Transmit(pDev->pSPI, (uint8_t *)pData, SizeOfData, IIS2ICLX_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS2ICLX_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

static oResult_t IIS2ICLX_SpiRead(IIS2ICLX_t *pDev, uint8_t RegAddress, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t addr;

	if(!pDev->State.IsOpen){
		return RESULT_ERROR;
	}

	addr = RegAddress | IIS2ICLX_SPI_READ;

	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, pDev->CS.ActiveLevel);
	}
	if(HAL_SPI_Transmit(pDev->pSPI, &addr, 1, IIS2ICLX_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS2ICLX_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(HAL_SPI_Receive(pDev->pSPI, pData, SizeOfData, IIS2ICLX_SPI_TIMEOUT) != HAL_OK){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		IIS2ICLX_OnHalError(pDev);
		return RESULT_ERROR;
	}
	if(pDev->CS.Port != NULL){
		HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
	}

	pDev->State.CountOfSpiError = 0;
	return RESULT_OK;
}

/* ============================================================
 * Register Descriptor Table — 런타임 접근의 SSOT
 *
 * 주소로 색인하되, 오프셋·크기는 산술로 구하지 않고 항목에서 읽는다 —
 * 주소=오프셋 항등이 성립하지 않는 IC 가 같은 계층에 들어와도 이 코드 모양이 유지된다.
 * 목록에 없는 주소는 Access=RESERVED(0) — 쓰기 경로에 오를 수 없다.
 * 출처: DS13005 Rev 7 Table 17(주소·타입·리셋값) + 각 레지스터 설명표(고정 비트 각주)
 * ============================================================ */
#define REG_RW(adr, por, res, wr, sc)   [adr] = { .Address = (adr), .ImageOffset = (adr), .SizeOfRegister = 1,                                                   .Access = IIS2ICLX_REGACCESS_RW, .PorValue = (por),                                                   .ReservedMask = (res), .WriteMask = (wr), .SelfClearMask = (sc) }
#define REG_RO(adr, por)                [adr] = { .Address = (adr), .ImageOffset = (adr), .SizeOfRegister = 1,                                                   .Access = IIS2ICLX_REGACCESS_RO, .PorValue = (por), .IsVolatile = 1 }

static const IIS2ICLX_RegInfo_t IIS2ICLX_RegMap[IIS2ICLX_REG_IMAGE_SIZE] = {
	REG_RW(IIS2ICLX_REG_FUNC_CFG_ACCESS, 0x00, 0x3F, 0xC0, 0x00),
	REG_RW(IIS2ICLX_REG_PIN_CTRL,        0x3F, 0xBF, 0x40, 0x00),
	/* FIFO·BDR 계열은 미사용 — WriteMask 0 으로 두어 Sync 가 건드리지 않게 한다.
	   쓰기 기능을 추가할 때 각 레지스터의 고정 비트를 데이터시트에서 확인해 채울 것 */
	REG_RW(IIS2ICLX_REG_FIFO_CTRL1,      0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_FIFO_CTRL2,      0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_FIFO_CTRL3,      0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_FIFO_CTRL4,      0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_COUNTER_BDR1,    0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_COUNTER_BDR2,    0x00, 0x00, 0x00, 0x00),
	REG_RW(IIS2ICLX_REG_INT1_CTRL,       0x00, 0x02, 0xFD, 0x00),
	REG_RW(IIS2ICLX_REG_INT2_CTRL,       0x00, 0x82, 0x7D, 0x00),
	REG_RO(IIS2ICLX_REG_WHO_AM_I,        IIS2ICLX_WHO_AM_I_VALUE),
	REG_RW(IIS2ICLX_REG_CTRL1_XL,        0x00, 0x01, 0xFE, 0x00),
	/* CTRL3_C: BOOT(bit7)·SW_RESET(bit0) 는 자기클리어 — diff·전송값 양쪽에서 제거된다 */
	REG_RW(IIS2ICLX_REG_CTRL3_C,         0x04, 0x02, 0xFD, 0x81),
	REG_RW(IIS2ICLX_REG_CTRL4_C,         0x00, 0xD3, 0x2C, 0x00),
	REG_RW(IIS2ICLX_REG_CTRL5_C,         0x00, 0xFC, 0x03, 0x00),
	REG_RW(IIS2ICLX_REG_CTRL6_C,         0x00, 0x17, 0xE8, 0x00),
	REG_RW(IIS2ICLX_REG_CTRL7_XL,        0x00, 0xFD, 0x02, 0x00),
	/* CTRL8_XL: bit1:0 은 Rev 7 Table 52 상 "must be set to 0" — WriteMask 밖으로 묶는다 */
	REG_RW(IIS2ICLX_REG_CTRL8_XL,        0x00, 0x03, 0xFC, 0x00),
	/* CTRL9_XL: 드라이버가 구동하는 비트는 DEVICE_CONF 뿐이다. DEN 비트와 "1 고정"
	   예약비트는 WriteMask 밖 — 마지막 실측값에서 보존된다 (POR 0xE0) */
	REG_RW(IIS2ICLX_REG_CTRL9_XL,        0xE0, 0xFD, 0x02, 0x00),
	REG_RW(IIS2ICLX_REG_CTRL10_C,        0x00, 0xDF, 0x20, 0x00),
	REG_RO(0x1A, 0x00),                              /* ALL_INT_SRC */
	REG_RO(0x1B, 0x00),                              /* WAKE_UP_SRC */
	REG_RO(0x1C, 0x00),                              /* TAP_SRC     */
	REG_RO(0x1D, 0x00),                              /* DEN_SRC     */
	REG_RO(IIS2ICLX_REG_STATUS,          0x00),
	REG_RO(0x20, 0x00),                              /* OUT_TEMP_L  */
	REG_RO(0x21, 0x00),                              /* OUT_TEMP_H  */
	REG_RO(0x28, 0x00),                              /* OUTX_L_A    */
	REG_RO(0x29, 0x00),                              /* OUTX_H_A    */
	REG_RO(0x2A, 0x00),                              /* OUTY_L_A    */
	REG_RO(0x2B, 0x00),                              /* OUTY_H_A    */
};

#undef REG_RW
#undef REG_RO

const IIS2ICLX_RegInfo_t *IIS2ICLX_GetRegInfo(uint8_t RegAddress)
{
	if(RegAddress >= IIS2ICLX_REG_IMAGE_SIZE){
		return NULL;
	}
	return &IIS2ICLX_RegMap[RegAddress];
}

/* 전송값 합성 — 드라이버가 구동하는 비트만 이미지에서 가져오고, 나머지는
   마지막 실측값에서 보존한다. 데이터시트가 "must be set to 0/1" 로 고정한 비트에
   통짜 바이트 쓰기로 0 을 밀어넣지 않기 위함이다 */
static uint8_t IIS2ICLX_ComposeWriteValue(const IIS2ICLX_t *pDev, const IIS2ICLX_RegInfo_t *pInfo, uint8_t Address)
{
	uint8_t value;

	value  = (uint8_t)(pDev->Register.Image[Address] & pInfo->WriteMask);
	value |= (uint8_t)(pDev->SyncedImage[Address] & (uint8_t)~pInfo->WriteMask);

	return (uint8_t)(value & (uint8_t)~pInfo->SelfClearMask);
}

/* 부품 → 이미지. 읽기는 예약 주소를 관통해도 무해하다(GetData 가 0x22~0x27 을 관통).
   읽은 구간은 SyncedImage 에도 반영한다 — 그것이 부품의 현재 진실이다 */
oResult_t IIS2ICLX_ReadReg(IIS2ICLX_t *pDev, uint8_t RegAddress, uint32_t SizeOfData)
{
	const IIS2ICLX_RegInfo_t *pInfo;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	pInfo = IIS2ICLX_GetRegInfo(RegAddress);
	if(pInfo == NULL || SizeOfData == 0 || (RegAddress + SizeOfData) > IIS2ICLX_REG_IMAGE_SIZE){
		return RESULT_ERROR;
	}

	if(IIS2ICLX_SpiRead(pDev, RegAddress, &pDev->Register.Image[pInfo->ImageOffset], SizeOfData) != RESULT_OK){
		return RESULT_ERROR;
	}

	memcpy(&pDev->SyncedImage[RegAddress], &pDev->Register.Image[pInfo->ImageOffset], SizeOfData);
	pDev->IsSeeded = 1;

	return RESULT_OK;
}

/* 이미지 → 부품. 예약·RO 주소가 범위에 끼면 조용히 넘기지 않고 오류로 끝난다 —
   예약 주소 쓰기는 DS13005 Rev 7 p.32 상 부품 영구 손상 조건이다.
   레지스터마다 마스크가 달라 버스트로 묶지 않고 1바이트씩 전송한다 */
oResult_t IIS2ICLX_WriteReg(IIS2ICLX_t *pDev, uint8_t RegAddress, uint32_t SizeOfData)
{
	const IIS2ICLX_RegInfo_t *pInfo;
	uint32_t i;
	uint8_t  address;
	uint8_t  value;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(SizeOfData == 0 || (RegAddress + SizeOfData) > IIS2ICLX_REG_IMAGE_SIZE){
		return RESULT_ERROR;
	}

	for(i = 0; i < SizeOfData; i++){
		address = (uint8_t)(RegAddress + i);
		pInfo   = IIS2ICLX_GetRegInfo(address);

		if(pInfo == NULL || pInfo->Access != IIS2ICLX_REGACCESS_RW){
			IIS2ICLX_LOG("WriteReg REFUSED addr=0x%02X (not writable)", (unsigned int)address);
			return RESULT_ERROR;
		}

		value = IIS2ICLX_ComposeWriteValue(pDev, pInfo, address);
		if(IIS2ICLX_SpiWrite(pDev, address, &value, 1) != RESULT_OK){
			return RESULT_ERROR;
		}

		pDev->Register.Image[pInfo->ImageOffset] = value;
		pDev->SyncedImage[address] = value;
	}

	return RESULT_OK;
}

/* 이미지를 부품에 반영한다. AutoSync=1 이면 마지막 동기화 섬도와 diff 해
   바뀐 RW 바이트만 보낸다. C 에는 프로퍼티 세터가 없으므로 "대입 순간 반영"이
   아니라 이 지연 diff 가 기능의 실질이다. 읽기로 seed 되기 전에는 거부한다 —
   부품을 본 적 없는 섬도는 목표값이 0 이 아닌 모든 RW 바이트에 쓰기를 발행한다 */
oResult_t IIS2ICLX_Sync(IIS2ICLX_t *pDev)
{
	const IIS2ICLX_RegInfo_t *pInfo;
	uint32_t address;
	uint8_t  value;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(!pDev->IsSeeded){
		IIS2ICLX_LOG("Sync REFUSED - shadow not seeded");
		return RESULT_ERROR;
	}

	for(address = 0; address < IIS2ICLX_REG_IMAGE_SIZE; address++){
		pInfo = &IIS2ICLX_RegMap[address];

		if(pInfo->Access != IIS2ICLX_REGACCESS_RW || pInfo->IsVolatile || pInfo->WriteMask == 0){
			continue;
		}

		value = IIS2ICLX_ComposeWriteValue(pDev, pInfo, (uint8_t)address);
		if(pDev->AutoSync && value == pDev->SyncedImage[address]){
			continue;
		}

		if(IIS2ICLX_SpiWrite(pDev, (uint8_t)address, &value, 1) != RESULT_OK){
			return RESULT_ERROR;
		}

		pDev->Register.Image[pInfo->ImageOffset] = value;
		pDev->SyncedImage[address] = value;
	}

	return RESULT_OK;
}

/* ============================================================
 * Public API
 * ============================================================ */
oResult_t IIS2ICLX_WhoAmI(IIS2ICLX_t *pDev, uint8_t *pId)
{
	if(pDev == NULL || pId == NULL){
		return RESULT_NULL;
	}

	if(IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_WHO_AM_I, 1) != RESULT_OK){
		return RESULT_ERROR;
	}

	*pId = pDev->Register.WHO_AM_I;
	return RESULT_OK;
}

oResult_t IIS2ICLX_GetStatus(IIS2ICLX_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}
	return IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_STATUS, 1);
}

/* SW_RESET 자동 클리어를 폴링해 리셋 완료를 판정한다 — 고정 지연만으로는
   느린 부팅에서 뒤따르는 설정 쓰기가 리셋에 덮어써진다 */
oResult_t IIS2ICLX_SoftReset(IIS2ICLX_t *pDev)
{
	uint32_t timer;
	uint8_t  reg = 0;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	pDev->Register.CTRL3_C.Byte = 0;
	pDev->Register.CTRL3_C.SW_RESET = 1;
	pDev->Register.CTRL3_C.IF_INC   = 1;   /* 주소 자동증가 유지 — 블록 읽기 전제 */

	/* 이것만은 합성 경로(WriteReg)를 쓰지 않는다 — 그쪽은 SelfClearMask 로 SW_RESET 을
	   지우므로 리셋이 영원히 발행되지 않는다. 리셋은 명시적 행위라 이 함수가 책임진다 */
	if(IIS2ICLX_SpiWrite(pDev, IIS2ICLX_REG_CTRL3_C, &pDev->Register.CTRL3_C.Byte, 1) != RESULT_OK){
		return RESULT_ERROR;
	}

	timer = oTMR_GetTick(TICKBASE_SYSTICK);
	do{
		if(IIS2ICLX_SpiRead(pDev, IIS2ICLX_REG_CTRL3_C, &reg, 1) != RESULT_OK){
			return RESULT_ERROR;
		}
		if((reg & 0x01) == 0){
			/* 부품은 방금 전 레지스터가 POR 값으로 돌아갔다. diff 기준인 SyncedImage 만
			   재시드한다 — Register(목표 상태)까지 되돌리면 호출부가 Open 전에 심은 프리셋과
			   DEVICE_CONF=1 의도가 지워져, 재시드가 막으려던 "diff 가 차이를 못 보는" 증상을
			   오히려 만든다. 읽기로 확인된 상태이므로 IsSeeded 도 여기서 선다 */
			uint32_t i;

			for(i = 0; i < IIS2ICLX_REG_IMAGE_SIZE; i++){
				pDev->SyncedImage[i] = IIS2ICLX_RegMap[i].PorValue;
			}
			pDev->SyncedImage[IIS2ICLX_REG_CTRL3_C] = reg;
			pDev->IsSeeded = 1;

			pDev->Register.CTRL3_C.Byte = reg;
			pDev->State.CountOfSpiError = 0;
			return RESULT_OK;
		}
	}while(!oTMR_Elapsed(&timer, IIS2ICLX_SOFTRESET_SETTLE_MS, TICKBASE_SYSTICK));

	/* SW_RESET 비트가 끝내 안 내려갔다 — 마지막으로 읽힌 CTRL3_C 를 남긴다.
	   0x00/0xFF 면 통신 자체가 안 된 것이고, 그 외 값이면 부품이 리셋에서 안 빠져나온 것 */
	IIS2ICLX_LOG("SoftReset TIMEOUT (%ums) CTRL3_C=0x%02X",
	             (unsigned int)IIS2ICLX_SOFTRESET_SETTLE_MS, (unsigned int)reg);

	return RESULT_ERROR;
}

oResult_t IIS2ICLX_GetConfig(IIS2ICLX_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	/* 0x10~0x18 은 예약 주소 0x11 을 포함하므로 버스트로 묶지 않고 나누어 읽는다 —
	   읽기 자체는 무해하나 이미지에 쓰레기를 싫어 넣지 않는다 */
	if(IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_INT1_CTRL, 1) != RESULT_OK
	   || IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL1_XL, 1) != RESULT_OK
	   || IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL3_C,  1) != RESULT_OK
	   || IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL4_C,  1) != RESULT_OK
	   || IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL8_XL, 1) != RESULT_OK
	   || IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL9_XL, 1) != RESULT_OK){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

/* CTRL3_C를 먼저 쓴다 — IF_INC·BDU가 확정돼야 뒤이은 데이터 블록 읽기가 성립한다.
   CTRL1_XL(ODR)은 마지막이다 — 인터럽트 라우팅이 확정된 뒤에 변환이 시작돼야
   첫 샘플의 data-ready 를 놓치지 않는다 */
oResult_t IIS2ICLX_SetConfig(IIS2ICLX_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	/* DEVICE_CONF(CTRL9_XL bit1) — DS13005 Rev 6 "9.3 초기화 절차" 가 SPI·I2C 양쪽 모두에
	   요구하는 비트다. "초기화 시점(및 소프트 리셋 후)에 1 로 설정하고 이후 계속 1 로 유지할 것"
	   이라고 명시돼 있다. 나머지 비트는 DEN 설정과 "반드시 0/1 이어야 하는" 예약 비트라
	   통째로 쓰면 안 되고, 읽어서 해당 비트만 세워 되쓴다 */
	if(IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_CTRL9_XL, 1) != RESULT_OK){
		return RESULT_ERROR;
	}
	if(pDev->Register.CTRL9_XL.DEVICE_CONF == 0){
		pDev->Register.CTRL9_XL.DEVICE_CONF = 1;
		/* WriteMask=0x02 가 DEVICE_CONF 외의 비트를 방금 읽은 값으로 보존한다 */
		if(IIS2ICLX_WriteReg(pDev, IIS2ICLX_REG_CTRL9_XL, 1) != RESULT_OK){
			return RESULT_ERROR;
		}
	}

	/* 실패한 레지스터를 지목한다 — 한 덩어리 OR 로 묶여 있으면 어디서 끊겼는지 알 수 없다.
	   ODR(CTRL1_XL)을 마지막에 쓰는 순서는 유지한다 */
	{
		static const uint8_t regs[] = {
			IIS2ICLX_REG_CTRL3_C, IIS2ICLX_REG_CTRL4_C, IIS2ICLX_REG_CTRL8_XL,
			IIS2ICLX_REG_INT1_CTRL, IIS2ICLX_REG_CTRL1_XL
		};
		uint8_t i;

		for(i = 0; i < sizeof(regs); i++){
			if(IIS2ICLX_WriteReg(pDev, regs[i], 1) != RESULT_OK){
				IIS2ICLX_LOG("SetConfig FAIL at reg 0x%02X (image 0x%02X)",
				             (unsigned int)regs[i], (unsigned int)pDev->Register.Image[regs[i]]);
				return RESULT_ERROR;
			}
		}
	}

	return RESULT_OK;
}

/* 온도(0x20)부터 가속도(0x2B)까지 IF_INC 자동증가로 한 번에 읽는다.
   자이로 자리(0x22~0x27)는 본 부품에 없어 Reserved로 버려진다 */
oResult_t IIS2ICLX_GetData(IIS2ICLX_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(!pDev->State.IsRun){
		return RESULT_ERROR;
	}

	if(IIS2ICLX_GetStatus(pDev) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(!pDev->Register.STATUS.XLDA){
		return RESULT_RUN;
	}

	if(IIS2ICLX_ReadReg(pDev, IIS2ICLX_REG_OUT_TEMP_L, sizeof(pDev->Register.Data)) != RESULT_OK){
		return RESULT_ERROR;
	}

	pDev->Temperature = IIS2ICLX_TEMP_TO_Degree(pDev->Register.Data.TEMP);

	switch(pDev->Register.CTRL1_XL.FS_XL)
	{
		default:
		case IIS2ICLX_XL_FS_500MG:
			pDev->Acceleration.X = IIS2ICLX_ACC_TO_500MG_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS2ICLX_ACC_TO_500MG_uG(pDev->Register.Data.ACCEL_Y);
			break;
		case IIS2ICLX_XL_FS_1G:
			pDev->Acceleration.X = IIS2ICLX_ACC_TO_1G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS2ICLX_ACC_TO_1G_uG(pDev->Register.Data.ACCEL_Y);
			break;
		case IIS2ICLX_XL_FS_2G:
			pDev->Acceleration.X = IIS2ICLX_ACC_TO_2G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS2ICLX_ACC_TO_2G_uG(pDev->Register.Data.ACCEL_Y);
			break;
		case IIS2ICLX_XL_FS_3G:
			pDev->Acceleration.X = IIS2ICLX_ACC_TO_3G_uG(pDev->Register.Data.ACCEL_X);
			pDev->Acceleration.Y = IIS2ICLX_ACC_TO_3G_uG(pDev->Register.Data.ACCEL_Y);
			break;
	}

	pDev->GravityMagnitude = sqrt(pDev->Acceleration.X * pDev->Acceleration.X
	                              + pDev->Acceleration.Y * pDev->Acceleration.Y);

	return RESULT_OK;
}

/* 부착 방향별 순간 경사각 산출 — GetAngle 의 누산 대상.
 *   HORIZONTAL : 중력이 보드면에 수직 → 축별 asin 독립 환산
 *   VERTICAL_* : 중력이 보드면 안 → atan2(교차축, |중력축|).
 *                분모 절댓값이 설치 극성(중력축이 어느 쪽을 향하든)을 흡수한다.
 * 면외 경사는 |a| 결손으로만 알 수 있어 크기뿐이고 부호가 없다 (Z축 부재).
 */
static void IIS2ICLX_ComputeTilt(IIS2ICLX_t *pDev, double *pPitch, double *pRoll)
{
	double ax = pDev->Acceleration.X;
	double ay = pDev->Acceleration.Y;
	double ratio;

	switch(pDev->Mount)
	{
		default:
		case IIS2ICLX_MOUNT_HORIZONTAL:
			/* |a| > 1 g 는 asin 정의역을 벗어나므로 클램프해 NaN 전파를 막는다 */
			*pPitch = IIS2ICLX_uG_TO_DEGREE((ax >  IIS2ICLX_uG_PER_G) ?  IIS2ICLX_uG_PER_G :
			                                (ax < -IIS2ICLX_uG_PER_G) ? -IIS2ICLX_uG_PER_G : ax);
			*pRoll  = IIS2ICLX_uG_TO_DEGREE((ay >  IIS2ICLX_uG_PER_G) ?  IIS2ICLX_uG_PER_G :
			                                (ay < -IIS2ICLX_uG_PER_G) ? -IIS2ICLX_uG_PER_G : ay);
			pDev->OutOfPlaneAngle = 0;
			return;

		case IIS2ICLX_MOUNT_VERTICAL_X:
			*pPitch = RADIAN_TO_DEGREE(atan2(ay, fabs(ax)));
			break;

		case IIS2ICLX_MOUNT_VERTICAL_Y:
			*pPitch = RADIAN_TO_DEGREE(atan2(ax, fabs(ay)));
			break;
	}

	*pRoll = 0;   /* 면내 경사 축이 하나뿐 — Roll 에 담을 값이 없다 */

	/* 정지 상태면 |a| = 1 g. 보드면이 벽에서 기울어진 만큼 면내 성분이 줄어든다 */
	ratio = pDev->GravityMagnitude / IIS2ICLX_uG_PER_G;
	if(ratio > 1.0){
		ratio = 1.0;
	}
	pDev->OutOfPlaneAngle = RADIAN_TO_DEGREE(acos(ratio));
}

/* IIS2ICLX_GetAngle
 *
 * Input
 * 	pDev	| Pointer of IIS2ICLX structure
 * 	Count	| Count of sampling (oversampling 횟수)
 *
 * Output
 * 	pAngle	| Mount 별 경사각 [°] — 산출식은 헤더 "2축 경사각 산출" 참조
 * 	oResult
 */
oResult_t IIS2ICLX_GetAngle(IIS2ICLX_t *pDev, uint8_t Count, oRPY_t *pAngle)
{
	oResult_t result = RESULT_RUN;
	double pitch = 0, roll = 0;

	if(pDev == NULL || pAngle == NULL || Count == 0){
		return RESULT_NULL;
	}

	switch(IIS2ICLX_GetData(pDev))
	{
		default:
			break;
		case RESULT_ERROR:
			pDev->State.CountOfError++;
			break;
		case RESULT_OK:
			if(pDev->State.CountOfOverSampling == 0){
				memset(&pDev->OverSampling, 0, sizeof(pDev->OverSampling));
			}

			IIS2ICLX_ComputeTilt(pDev, &pitch, &roll);
			pDev->OverSampling.Pitch += pitch;
			pDev->OverSampling.Roll  += roll;
			pDev->State.CountOfOverSampling++;
			break;
	}

	if(pDev->State.CountOfError >= Count){
		result = RESULT_ERROR;
	}
	else if(pDev->State.CountOfOverSampling >= Count){
		pDev->Angle.Pitch = pDev->OverSampling.Pitch / (double)pDev->State.CountOfOverSampling;
		pDev->Angle.Roll  = pDev->OverSampling.Roll  / (double)pDev->State.CountOfOverSampling;

		*pAngle = pDev->Angle;
		result = RESULT_OK;
	}

	if(result != RESULT_RUN){
		pDev->State.CountOfOverSampling = 0;
		pDev->State.CountOfError = 0;
	}

	return result;
}

/* 요청 ODR 이상을 만족하는 가장 낮은 단계를 고른다 — 레지스터에만 반영하며
   실제 적용은 IIS2ICLX_SetConfig 호출 시점이다 */
void IIS2ICLX_SetFilter(IIS2ICLX_t *pDev, double ODR)
{
	if(pDev == NULL){
		return;
	}

	if(ODR > 416){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_833HZ;
		pDev->DataRate = 833;
	}
	else if(ODR > 208){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_416HZ;
		pDev->DataRate = 416;
	}
	else if(ODR > 104){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_208HZ;
		pDev->DataRate = 208;
	}
	else if(ODR > 52){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_104HZ;
		pDev->DataRate = 104;
	}
	else if(ODR > 26){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_52HZ;
		pDev->DataRate = 52;
	}
	else if(ODR > 12.5){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_26HZ;
		pDev->DataRate = 26;
	}
	else if(ODR > 0){
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_12_5HZ;
		pDev->DataRate = 12.5;
	}
	else{
		pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_OFF;
		pDev->DataRate = 0;
	}
}

/* ============================================================
 * Open — non-blocking state machine
 *   0: SPI Init 확인 → CS 핀 idle
 *   1: WHO_AM_I 검증
 *   2: Soft Reset
 *   3: 기본 설정 적용 (CTRL3_C·CTRL4_C·CTRL8_XL·CTRL1_XL)
 *   4: IsRun=1 로 전환
 * ============================================================ */
oResult_t IIS2ICLX_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, IIS2ICLX_t *pDev)
{
	oResult_t result = RESULT_RUN;
	uint8_t id;

	if(pSPI == NULL || pDev == NULL){
		return RESULT_ERROR;
	}

	if(pDev->State.IsOpen && !pDev->State.IsBusy){
		return RESULT_OK;
	}
	else if(!pDev->State.IsBusy && pDev->State.SequenceStep != 0){
		memset(&pDev->State, 0, sizeof(pDev->State));
	}

	pDev->State.IsBusy = 1;

	switch(pDev->State.SequenceStep)
	{
		case 0:
			pDev->pSPI = pSPI;
			pDev->CS = CS;

			/* SPI 페리페럴 재초기화 — 전원 토글 후 stale state 제거 */
			HAL_SPI_DeInit(pDev->pSPI);
			if(HAL_SPI_Init(pDev->pSPI) != HAL_OK){
				IIS2ICLX_LOG("Open step0 FAIL - HAL_SPI_Init");
				result = RESULT_ERROR;
				break;
			}

			if(pDev->CS.Port != NULL){
				HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
			}

			pDev->State.IsOpen = 1;
			pDev->State.SequenceStep++;
			break;

		case 1:
			/* 읽어온 ID 는 성패와 무관하게 항상 남긴다. 값을 버리면 "상수가 틀렸다" 와
			   "응답이 없다" 가 구분되지 않아 원인 규명이 배선까지 못 내려간다 */
			if(IIS2ICLX_WhoAmI(pDev, &id) != RESULT_OK){
				IIS2ICLX_LOG("Open step1 FAIL - WHO_AM_I read error (SpiErr=%u)",
				             (unsigned int)pDev->State.CountOfSpiError);
				result = RESULT_ERROR;
				break;
			}
			pDev->Register.WHO_AM_I = id;
			if(id != IIS2ICLX_WHO_AM_I_VALUE){
				IIS2ICLX_LOG("Open step1 FAIL - ID=0x%02X (expected 0x%02X)",
				             (unsigned int)id, (unsigned int)IIS2ICLX_WHO_AM_I_VALUE);
				result = RESULT_ERROR;   /* 칩 식별 실패 */
				break;
			}
			IIS2ICLX_LOG("Open step1 OK - ID=0x%02X", (unsigned int)id);
			pDev->State.SequenceStep++;
			break;

		case 2:
			if(IIS2ICLX_SoftReset(pDev) != RESULT_OK){
				IIS2ICLX_LOG("Open step2 FAIL - SoftReset");
				result = RESULT_ERROR;
				break;
			}
			IIS2ICLX_LOG("Open step2 OK - SoftReset");
			pDev->State.SequenceStep++;
			break;

		case 3:
			/* 기본 설정 — 호출 측이 별도 지정 안 하면 다음 값 사용.
			   경사계 기본은 최저 FS(±0.5 g)·저속 ODR — 분해능 우선 */
			if(pDev->Register.CTRL1_XL.Byte == 0){
				pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_52HZ;
				pDev->Register.CTRL1_XL.FS_XL  = IIS2ICLX_XL_FS_500MG;
				pDev->DataRate = 52;
			}

			pDev->Register.CTRL3_C.IF_INC = 1;    /* 블록 읽기 전제 */
			pDev->Register.CTRL3_C.BDU    = 1;    /* 상·하위 바이트 tearing 방지 */
			pDev->Register.CTRL4_C.I2C_disable = 1;   /* SPI 전용 — I2C 인터페이스 차단 */

			if(IIS2ICLX_SetConfig(pDev) != RESULT_OK){
				IIS2ICLX_LOG("Open step3 FAIL - SetConfig");
				result = RESULT_ERROR;
				break;
			}
			IIS2ICLX_LOG("Open step3 OK - CTRL1_XL=0x%02X CTRL3_C=0x%02X CTRL4_C=0x%02X INT1=0x%02X",
			             (unsigned int)pDev->Register.CTRL1_XL.Byte,
			             (unsigned int)pDev->Register.CTRL3_C.Byte,
			             (unsigned int)pDev->Register.CTRL4_C.Byte,
			             (unsigned int)pDev->Register.INT1_CTRL.Byte);
			pDev->State.SequenceStep++;
			break;

		case 4:
			pDev->State.IsRun = 1;
			IIS2ICLX_LOG("Open DONE - ODR=%dHz FS=%u Mount=%d",
			             (int)pDev->DataRate,
			             (unsigned int)pDev->Register.CTRL1_XL.FS_XL,
			             (int)pDev->Mount);
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		pDev->State.SequenceStep = 0;
		pDev->State.IsBusy = 0;

		if(result == RESULT_ERROR){
			pDev->State.IsOpen = 0;
			pDev->State.IsRun = 0;
		}
	}

	return result;
}

oResult_t IIS2ICLX_Close(IIS2ICLX_t *pDev)
{
	oResult_t result = RESULT_RUN;

	if(pDev == NULL){
		return RESULT_NULL;
	}

	if(!pDev->State.IsOpen){
		return RESULT_OK;
	}

	switch(pDev->State.SequenceStep)
	{
		case 0:
			pDev->State.IsBusy = 1;

			pDev->Register.CTRL1_XL.ODR_XL = IIS2ICLX_XL_ODR_OFF;   /* power-down */
			pDev->DataRate = 0;
			pDev->State.SequenceStep++;
		case 1:
			if(IIS2ICLX_WriteReg(pDev, IIS2ICLX_REG_CTRL1_XL, 1) == RESULT_OK){
				pDev->State.SequenceStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 2:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		if(pDev->CS.Port != NULL){
			HAL_GPIO_WritePin(pDev->CS.Port, pDev->CS.Pin, !pDev->CS.ActiveLevel);
		}
		HAL_SPI_DeInit(pDev->pSPI);
		memset(&pDev->State, 0, sizeof(pDev->State));
	}

	return result;
}
