/*
 * EEPROM_24LC16B.c
 *
 *  Version: 0.4 (2026-09-03)
 */

#include "EEPROM_24LC16B.h"
#include "ONE_Serial.h"

/* 진단 로그 — ONE_Serial.h 는 .h 가 아니라 여기(.c)에만 들여온다. 헤더에 넣으면 이
   드라이버를 쓰는 모든 프로젝트가 로깅 의존성을 떠안는다.
   oSerial_Log 는 런타임 no-op 이지만 포맷 문자열은 플래시에 남으므로 컴파일 타임 게이트를 둔다 */
#ifndef EEPROM_24LC16B_LOG_ENABLE
#define EEPROM_24LC16B_LOG_ENABLE   1
#endif

#if EEPROM_24LC16B_LOG_ENABLE
#define EEPROM_LOG(fmt, ...)   oSerial_Log("EEP", fmt, ##__VA_ARGS__)
#else
#define EEPROM_LOG(fmt, ...)   do{}while(0)
#endif

#define EEPROM_24LC16B_TIMEOUT				1000	/* ms — bounded timeout */
#define EEPROM_24LC16B_WRITE_MS			6		/* 페이지 쓰기 후 내부 write cycle 대기 (>5ms) */
#define EEPROM_24LC16B_RECOVERY_THRESHOLD	5		/* HAL_ERROR 연속 누적 임계 */

oResult_t EEPROM_24LC16B_Recovery(EEPROM_24LC16B_t *pDev)
{
	if(pDev == NULL || pDev->pI2C == NULL){
		return RESULT_NULL;
	}

	EEPROM_LOG("Recovery - I2C re-init (errors=%u)", (unsigned int)pDev->CountOfI2CError);

	HAL_I2C_DeInit(pDev->pI2C);
	if(HAL_I2C_Init(pDev->pI2C) != HAL_OK){
		return RESULT_ERROR;
	}

	pDev->CountOfI2CError = 0;
	return RESULT_OK;
}

static void EEPROM_24LC16B_OnHalError(EEPROM_24LC16B_t *pDev)
{
	if(pDev->CountOfI2CError < 0xFF){
		pDev->CountOfI2CError++;
	}

	/* Read/Write 각 호출이 아니라 여기서만 남긴다 — MiStorage 가 주기 폴링이라
	   트랜잭션 단위 로그는 버스 결함 시 링버퍼를 덮는다 */
	EEPROM_LOG("HalError count=%u HalErr=0x%08lX",
	           (unsigned int)pDev->CountOfI2CError,
	           (unsigned long)(pDev->pI2C ? HAL_I2C_GetError(pDev->pI2C) : 0));

	if(pDev->CountOfI2CError >= EEPROM_24LC16B_RECOVERY_THRESHOLD){
		EEPROM_24LC16B_Recovery(pDev);
	}
}

/* WP 제어 — 핀 미배선 보드(WP.Port==NULL)에서는 아무것도 하지 않는다 */
#define EEPROM_24LC16B_WP(pDev, state)   do{ if((pDev)->WP.Port != NULL){ IO_WRITE((pDev)->WP, (state)); } }while(0)

oResult_t EEPROM_24LC16B_Init(EEPROM_24LC16B_t *pDev, I2C_HandleTypeDef *pI2C, oIO_t WP)
{
	if(pDev == NULL || pI2C == NULL){
		return RESULT_NULL;
	}

	pDev->pI2C = pI2C;
	pDev->WP   = WP;
	EEPROM_24LC16B_WP(pDev, IO_HIGH);   /* 기본은 보호 — Write 구간에서만 연다 */

	/* I2C 미초기화(RESET) 상태면 Init — 이미 init된 공유 버스(CubeMX/TMP1075)면 그대로 사용 */
	if(pI2C->State == HAL_I2C_STATE_RESET){
		if(HAL_I2C_Init(pI2C) != HAL_OK){
			EEPROM_LOG("Init FAIL - HAL_I2C_Init");
			return RESULT_ERROR;
		}
	}

	pDev->CountOfI2CError = 0;
	pDev->IsOpen          = 1;

	EEPROM_LOG("Init OK - WP %s", (pDev->WP.Port != NULL) ? "owned" : "not wired");
	return RESULT_OK;
}

oResult_t EEPROM_24LC16B_DeInit(EEPROM_24LC16B_t *pDev)
{
	if(pDev == NULL){
		return RESULT_NULL;
	}

	EEPROM_24LC16B_WP(pDev, IO_HIGH);   /* 닫을 때도 보호 상태로 남긴다 */

	/* ⚠ TMP1075 가 같은 I2C1 을 쓰므로 이 DeInit 은 온도 센서도 함께 내린다 */
	EEPROM_LOG("DeInit - HAL_I2C_DeInit on SHARED bus (TMP1075 also affected)");

	if(pDev->pI2C != NULL){
		HAL_I2C_DeInit(pDev->pI2C);		/* 무조건 I2C DeInit */
	}

	pDev->IsOpen = 0;
	return RESULT_OK;
}

/* 24LC16B: 블록(주소 상위 3비트) → 디바이스 주소, 오프셋(하위 8비트) → 워드 주소.
 * 전송은 HAL_I2C_Mem(8bit) — 블록/페이지 경계에서 분할. 블로킹.
 *   DevAddress = TX_BASE + (block<<1) (R/W 비트는 HAL이 방향에 맞게 처리). */

oResult_t EEPROM_24LC16B_Read(EEPROM_24LC16B_t *pDev, uint16_t Addr, uint8_t *pData, uint16_t Size)
{
	if(pDev == NULL || pDev->pI2C == NULL || !pDev->IsOpen){
		return RESULT_ERROR;
	}
	if(pData == NULL || Size == 0 || (uint32_t)Addr + Size > EEPROM_24LC16B_CAPACITY){
		return RESULT_ERROR;
	}
	/* 공유 버스(TMP1075 등)가 Close로 DeInit(RESET)해 남긴 경우 복구 — 미초기화 버스는 전송 즉시 실패 */
	if(pDev->pI2C->State == HAL_I2C_STATE_RESET && EEPROM_24LC16B_Recovery(pDev) != RESULT_OK){
		return RESULT_ERROR;
	}

	while(Size > 0){
		uint8_t  block  = (uint8_t)(Addr / EEPROM_24LC16B_BLOCK_SIZE);
		uint8_t  offset = (uint8_t)(Addr % EEPROM_24LC16B_BLOCK_SIZE);
		uint16_t devTx  = (uint16_t)(EEPROM_24LC16B_TX_BASE + (block << 1));   /* 블록선택 = 디바이스 주소 */
		uint16_t chunk  = (uint16_t)(EEPROM_24LC16B_BLOCK_SIZE - offset);      /* 블록 경계까지 (블록 넘어 read 시 wrap 방지) */
		if(chunk > Size) chunk = Size;

		if(HAL_I2C_Mem_Read(pDev->pI2C, devTx, offset, I2C_MEMADD_SIZE_8BIT, pData, chunk, EEPROM_24LC16B_TIMEOUT) != HAL_OK){
			EEPROM_24LC16B_OnHalError(pDev);
			return RESULT_ERROR;
		}
		pDev->CountOfI2CError = 0;

		Addr  = (uint16_t)(Addr + chunk);
		pData += chunk;
		Size  = (uint16_t)(Size - chunk);
	}
	return RESULT_OK;
}

oResult_t EEPROM_24LC16B_Write(EEPROM_24LC16B_t *pDev, uint16_t Addr, const uint8_t *pData, uint16_t Size)
{
	oResult_t result = RESULT_OK;

	if(pDev == NULL || pDev->pI2C == NULL || !pDev->IsOpen){
		return RESULT_ERROR;
	}
	if(pData == NULL || Size == 0 || (uint32_t)Addr + Size > EEPROM_24LC16B_CAPACITY){
		return RESULT_ERROR;
	}
	/* 공유 버스(TMP1075 등)가 Close로 DeInit(RESET)해 남긴 경우 복구 — 미초기화 버스는 전송 즉시 실패 */
	if(pDev->pI2C->State == HAL_I2C_STATE_RESET && EEPROM_24LC16B_Recovery(pDev) != RESULT_OK){
		return RESULT_ERROR;
	}

	/* 여기서 WP 를 열고 EXIT 에서 반드시 닫는다 — 중간 이탈이 있어도 보호가 풀린 채 남지 않는다 */
	EEPROM_24LC16B_WP(pDev, IO_LOW);

	while(Size > 0){
		uint8_t  block   = (uint8_t)(Addr / EEPROM_24LC16B_BLOCK_SIZE);
		uint8_t  offset  = (uint8_t)(Addr % EEPROM_24LC16B_BLOCK_SIZE);
		uint16_t devTx   = (uint16_t)(EEPROM_24LC16B_TX_BASE + (block << 1));
		uint16_t pageRem = (uint16_t)(EEPROM_24LC16B_PAGE_SIZE - (offset % EEPROM_24LC16B_PAGE_SIZE));   /* 페이지 경계까지 */
		uint16_t chunk   = (pageRem < Size) ? pageRem : Size;

		if(HAL_I2C_Mem_Write(pDev->pI2C, devTx, offset, I2C_MEMADD_SIZE_8BIT, (uint8_t *)pData, chunk, EEPROM_24LC16B_TIMEOUT) != HAL_OK){
			EEPROM_24LC16B_OnHalError(pDev);
			result = RESULT_ERROR;
			goto EXIT;
		}
		pDev->CountOfI2CError = 0;

		HAL_Delay(EEPROM_24LC16B_WRITE_MS);   /* 내부 write cycle 대기 */

		Addr  = (uint16_t)(Addr + chunk);
		pData += chunk;
		Size  = (uint16_t)(Size - chunk);
	}

EXIT:
	EEPROM_24LC16B_WP(pDev, IO_HIGH);
	return result;
}
