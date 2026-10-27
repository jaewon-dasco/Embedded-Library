/*
 * NAND_MT29F2G01ABAGDWB_IT.c
 *
 *  Version: 0.6 (2026-08-07)
 */

#include "NAND_MT29F2G01ABAGDWB_IT.h"

#include "string.h"
#include "ONE_Math.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"

#ifdef MT29F2G_ENABLED
#define NAND_TIMEOUT_VALUE	100U   // HAL QSPI 단일 트랜잭션(cmd/data/AutoPolling) 블로킹 상한. QUADSPI 페리페럴 완료/상태매치 플래그를 SysTick(ms)로 감시. 정상 ~0.15ms·최악(AutoPolling erase-WIP) ~10ms 대비 10배 여유. 페리페럴 행·칩 busy 고착 시에만 걸림(1000→100ms로 main 루프 블로킹 축소)
#define NAND_ERASE_TIMEOUT	100U  // tERS max ~10ms 대비 여유 (비블로킹 WIP 폴링 상한)
#define NAND_PROGRAM_TIMEOUT	50U   // tPROG max ~2ms 대비 여유
#define NAND_TRD_WAIT_US	(MT29F2G_TRD_MAX_US + 50)   // 0x13 후 무조건 대기 — tRD max(70µs)+여유
#define NAND_READ_TIMEOUT	10U   // tRD max(70µs) 대비 여유 (비블로킹 WIP 폴링 상한). read는 array→cache 전송뿐이라 Erase·Program보다 짧다

MT29F2G_Result_t MT29F2G_GetFeature(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_GET_FEATURES;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = Address & 0xFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if (HAL_QSPI_Receive(pMT29F2G->pHandle, pData, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_RX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_GetStatusRegister(MT29F2G_t *pMT29F2G)
{
	return MT29F2G_GetFeature(pMT29F2G, MT29F2G_FEATURE_STATUS, (uint8_t *)&pMT29F2G->State);
}

int8_t MT29F2G_CheckStatusRegister(MT29F2G_t *pMT29F2G, uint8_t Mask)
{
	if(MT29F2G_GetStatusRegister(pMT29F2G) != MT29F2G_RESULT_OK){
		return -1;
	}

	if(Mask != 0){
		if((pMT29F2G->State & Mask) != 0){
			return 1;
		}
		else{
			return 0;
		}
	}

	return 1;
}

/**
  * @brief  QSPI 주변장치를 강제 복구한다 (Abort 실패 시 RCC 리셋 + 재초기화).
  */
static void MT29F2G_RecoverQSPI(MT29F2G_t *pMT29F2G)
{
	HAL_QSPI_Abort(pMT29F2G->pHandle);
	if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
		__HAL_RCC_QSPI_FORCE_RESET();
		__HAL_RCC_QSPI_RELEASE_RESET();
		pMT29F2G->pHandle->State = HAL_QSPI_STATE_RESET;
		HAL_QSPI_Init(pMT29F2G->pHandle);
	}
}

/**
  * @brief  메모리 준비(Write In Progress 해제)를 확인하기 위해 SR를 자동 폴링한다.
  * @retval MT29F2G_RESULT_OK 또는 RESULT_ERROR
  */
MT29F2G_Result_t MT29F2G_AutoPolling(MT29F2G_t *pMT29F2G, uint8_t Match, uint8_t Mask)
{
	QSPI_CommandTypeDef s_command;
	QSPI_AutoPollingTypeDef s_config;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 메모리 준비 완료 상태를 확인 (WIP 비트가 0인지)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_GET_FEATURES;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = MT29F2G_FEATURE_STATUS;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData			= 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;

	s_config.Match            = Match;
	s_config.Mask             = Mask;
	s_config.MatchMode        = QSPI_MATCH_MODE_AND;
	s_config.StatusBytesSize  = 1;
	s_config.Interval         = 0x10;
	s_config.AutomaticStop    = QSPI_AUTOMATIC_STOP_ENABLE;

	if (HAL_QSPI_AutoPolling(pMT29F2G->pHandle, &s_command, &s_config, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		MT29F2G_RecoverQSPI(pMT29F2G);
		return MT29F2G_RESULT_AUTOPOLLING_ERROR;
	}

	return MT29F2G_RESULT_OK;
}


/**
  * @brief  Write Enable 명령을 전송하고, 쓰기 활성화 상태가 될 때까지 대기한다.
  * @retval MT29F2G_RESULT_OK 또는 RESULT_ERROR
  */
MT29F2G_Result_t MT29F2G_WriteEnable(MT29F2G_t *pMT29F2G)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// Write Enable 명령 (0x06) 전송
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_WRITE_ENABLE;
	s_command.AddressMode     = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	// 쓰기 활성화 상태를 확인하기 위해 자동 폴링 모드 설정
	if(MT29F2G_AutoPolling(pMT29F2G, MT29F2G_SR_WREN, MT29F2G_SR_WREN) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_AUTOPOLLING_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

/**
  * @brief  Write Disable(0x04) — WEL 클리어. 보류된 program/erase 의도를 명시적으로 해제할 때 사용.
  */
MT29F2G_Result_t MT29F2G_WriteDisable(MT29F2G_t *pMT29F2G)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_WRITE_DISABLE;
	s_command.AddressMode     = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

MT29F2G_Result_t MT29F2G_SetFeature(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t Data)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	// SetFeature(0x1F)는 Write Enable 불필요 (Micron 데이터시트)
	// WREN은 Program/Erase 명령에만 필요

	memset(&s_command, 0, sizeof(s_command));

	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_SET_FEATURES;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = Address & 0xFF;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	if (HAL_QSPI_Transmit(pMT29F2G->pHandle, &Data, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

/**
  * @brief  페이지 읽기. tRD(array→cache, ~70µs) 대기를 매 사이클 1회 확인으로 분할한다.
  *         case 0: BUSY 확인 + PAGE READ(0x13) 발행(µs, 블로킹) → RUN
  *         case 1: tRD 경과+OIP 해제까지 재진입 확인 → READ FROM CACHE(0x6B) 수신(블로킹, DMA 미사용) → OK/오류
  *         단일 static 상태(재진입 1건 전용) — 이전 read 미완료 중 다른 (Address,pData,Size)로 재호출되면
  *         이전 read를 폐기하고 새 read를 재시작한다(미완료 0x13은 칩 상태를 남기지 않아 폐기 안전).
  */
MT29F2G_Result_t MT29F2G_ReadMemory(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData, uint32_t Size)
{
	static uint8_t RdStep = 0;
	static uint32_t RdTimer = 0;
	static uint32_t RdWipTimer = 0;
	static uint32_t RdAddress = 0;
	static uint8_t *RdData = NULL;
	static uint32_t RdSize = 0;
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL || pData == NULL || Size == 0){
		RdStep = 0;
		return MT29F2G_RESULT_NULL;
	}

	switch(RdStep)
	{
		case 0:
			// Recover QSPI if stuck from previous failed operation
			if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
				MT29F2G_RecoverQSPI(pMT29F2G);
			}

			if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP|MT29F2G_SR_CRBSY) == 1){
				return MT29F2G_RESULT_BUSY;
			}

			memset(&s_command, 0, sizeof(s_command));

			// PAGE READ 명령 전송 (0x13)
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_PAGE_READ;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
			s_command.Address     		= MT29F2G_ROW_ADDRESS(Address);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_NONE;
			s_command.DummyCycles     = 0;
			s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
			s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
			s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

			if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
			{
				return MT29F2G_RESULT_TX_ERROR;
			}

			RdAddress = Address; RdData = pData; RdSize = Size;
			RdTimer = oTMR_GetTick(TICKBASE_CORECYCLE_US);
			RdWipTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			RdStep = 1;
			return MT29F2G_RESULT_RUN;

		case 1:
			// 재진입 인자 변경 = 이전 read 폐기 후 새 read 재시작 (self-healing).
			// 완료 전 0x13은 칩 상태를 남기지 않으므로(캐시 로드뿐) 폐기가 안전. 상위가 새 대상을
			// 요청하면 그것이 곧 새 read의 시작 — 하드 실패 대신 case 0부터 재수행.
			if(Address != RdAddress || pData != RdData || Size != RdSize){
				RdStep = 0;
				return MT29F2G_ReadMemory(pMT29F2G, Address, pData, Size);   // 1단 재귀(case 0 경로) — 새 0x13 발행 후 RUN
			}

			// PAGE READ(0x13) 후 tRD 동안 array→cache 전송(datasheet tRD max 70µs). 그동안 OIP/WIP(bit0)=1.
			// 첫 상태 샘플이 OIP 상승 전(또는 통신오류 -1)에 "완료"로 falls-through하면 0x6B가 직전
			// 페이지의 stale 캐시를 읽는다(= 쓰기 후 읽기 오판의 근본 원인). 무조건 tRD(여유 120µs)
			// 경과를 보장한 뒤 OIP=0까지 확인. !=0(busy=1·오류=-1 모두)이면 다음 사이클로 대기 유지.
			if(!oTMR_Elapsed(&RdTimer, NAND_TRD_WAIT_US, TICKBASE_CORECYCLE_US)){
				return MT29F2G_RESULT_RUN;
			}
			if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP) != 0){
				// `!=0`은 busy(1)와 통신오류(-1)를 함께 묶으므로 칩 무응답이면 대기가 영구화된다.
				// 블로킹 래퍼(BadBlockCheck·ReadByte의 `while(...==RUN){}`)는 하드 무한루프가 되고
				// IWDG는 상위에서 계속 급여되므로, 상한이 유일한 탈출구다.
				if(oTMR_Elapsed(&RdWipTimer, NAND_READ_TIMEOUT, TICKBASE_SYSTICK)){
					RdStep = 0;
					return MT29F2G_RESULT_TIMEOUT;
				}
				return MT29F2G_RESULT_RUN;
			}

			memset(&s_command, 0, sizeof(s_command));

			// 실제 읽기 명령 전송 (READ FROM CACHE) — DMA 미사용, 이 수신 자체는 블로킹 유지.
			// 컬럼 필드 = [15:13]dummy | [12]plane select | [11:0]column. 2Gb(듀얼 플레인)는 plane select
			// = 블록주소 LSB 필수 (1Gb에서만 dummy — 데이터시트 Fig.13/15 note). 미포함 시 홀수 블록(plane 1)
			// 읽기가 plane 0 캐시(직전 값)를 반환 — 홀수 블록 전체 stale read의 근본 원인 (HW 실증).
			#if MT29F2G_READ_MODE == 1
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_READ_CACHE_X1;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
			s_command.Address         = MT29F2G_CACHE_ADDRESS(RdAddress);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_1_LINE;
			s_command.DummyCycles     = 0;
			s_command.NbData          = RdSize;
			#elif MT29F2G_READ_MODE == 2
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_READ_CACHE_X2;
			s_command.AddressMode     = QSPI_ADDRESS_2_LINES;
			s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
			s_command.Address         = MT29F2G_CACHE_ADDRESS(RdAddress);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_2_LINES;
			s_command.DummyCycles     = 8;
			s_command.NbData          = RdSize;
			#elif MT29F2G_READ_MODE == 4
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_READ_CACHE_X4;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
			s_command.Address         = MT29F2G_CACHE_ADDRESS(RdAddress);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_4_LINES;
			s_command.DummyCycles     = 8;
			s_command.NbData          = RdSize;
			#endif

			RdStep = 0;   // 이하 모든 경로가 terminal(RUN 아님) → 여기서 미리 클리어

			if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
			{
				return MT29F2G_RESULT_TX_ERROR;
			}

			if(HAL_QSPI_Receive(pMT29F2G->pHandle, RdData, NAND_TIMEOUT_VALUE) != HAL_OK)
			{
				return MT29F2G_RESULT_RX_ERROR;
			}

			// ECC 상태(ECCS, bits4-6 = 0x70): 010b(0x20)=8비트 초과 정정불가만 오류.
			// 001/011/101(정정 성공)은 정상 데이터. 기존 (State & 0x20)은 011b(0x30, 4~6비트 정정성공)까지 오판.
			if(MT29F2G_GetStatusRegister(pMT29F2G) != MT29F2G_RESULT_OK){
				return MT29F2G_RESULT_RX_ERROR;
			}
			if((pMT29F2G->State & MT29F2G_SR_ECC) == MT29F2G_ECCS_UNCORRECTABLE){ //ECC uncorrectable
				return MT29F2G_RESULT_ECC_ERROR;
			}

			return MT29F2G_RESULT_OK;
	}

	RdStep = 0;
	return MT29F2G_RESULT_STATE_FAIL;   // unreachable(RdStep 범위 밖) 안전망
}


/**
  * @brief  페이지 프로그램. tPROG(~수백µs~ms) WIP 대기를 매 사이클 1회 폴링으로 분할한다.
  *         case 0: WREN + PROGRAM LOAD/EXECUTE 발행(µs, 블로킹) → RUN
  *         case 1: WIP 해제까지 재진입 폴링 → OK/PROGRAM_FAIL/TIMEOUT
  */
MT29F2G_Result_t MT29F2G_WriteMemory(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData, uint32_t Size)
{
	static uint8_t WrStep = 0;
	static uint32_t WrTimer = 0;
	MT29F2G_Result_t result = MT29F2G_RESULT_RUN;
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL || pData == NULL || Size == 0){
		WrStep = 0;
		return MT29F2G_RESULT_NULL;
	}

	switch(WrStep)
	{
		case 0:
			if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP) == 1){
				return MT29F2G_RESULT_BUSY;
			}

			// 쓰기 활성화 명령 전송
			if((result=MT29F2G_WriteEnable(pMT29F2G)) != MT29F2G_RESULT_OK){
				break;
			}

			memset(&s_command, 0, sizeof(s_command));

			// PROGRAM Load 명령 전송.
			// 컬럼 필드 = [15:13]dummy | [12]plane select | [11:0]column. 2Gb(듀얼 플레인)는 plane select
			// = 블록주소 LSB 필수 (1Gb에서만 dummy). 미포함 시 홀수 블록(plane 1) LOAD가 plane 0 캐시에
			// 적재되고 EXECUTE는 빈 plane 1 캐시를 구움 — 홀수 블록 전체 phantom program의 근본 원인 (HW 실증).
			#if MT29F2G_WRITE_MODE == 1
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_PROGRAM_LOAD_X1;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
			s_command.Address         = MT29F2G_CACHE_ADDRESS(Address);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_1_LINE;
			s_command.DummyCycles     = 0;
			s_command.NbData          = Size;
			#elif MT29F2G_WRITE_MODE == 4
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_PROGRAM_LOAD_X4;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_16_BITS;
			s_command.Address         = MT29F2G_CACHE_ADDRESS(Address); //Plane|Column
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_4_LINES;
			s_command.DummyCycles     = 0;
			s_command.NbData          = Size;
			#endif

			if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
			{
				result = MT29F2G_RESULT_TX_ERROR;
				break;
			}

			if(HAL_QSPI_Transmit(pMT29F2G->pHandle, pData, NAND_TIMEOUT_VALUE) != HAL_OK)
			{
				result = MT29F2G_RESULT_TX_ERROR;
				break;
			}

			memset(&s_command, 0, sizeof(s_command));

			// PROGRAM EXECUTE 명령 전송 (0x10)
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_PROGRAM_EXECUTE;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
			s_command.Address         = MT29F2G_ROW_ADDRESS(Address);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_NONE;
			s_command.DummyCycles     = 0;
			s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
			s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
			s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
			s_command.NbData          = 0;

			if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK){
				result = MT29F2G_RESULT_TX_ERROR;
				break;
			}

			WrTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			WrStep = 1;
			result = MT29F2G_RESULT_RUN;
			break;

		case 1:
		{
			// tPROG WIP 해제를 비블로킹 폴링 (블로킹 AutoPolling 대체)
			int8_t wip = MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP);

			if(wip < 0){
				result = MT29F2G_RESULT_AUTOPOLLING_ERROR;
			}
			else if(wip == 1){
				result = oTMR_Elapsed(&WrTimer, NAND_PROGRAM_TIMEOUT, TICKBASE_SYSTICK) ? MT29F2G_RESULT_TIMEOUT : MT29F2G_RESULT_RUN;
			}
			else if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_PFAIL) == 1){
				result = MT29F2G_RESULT_PROGRAM_FAIL;
			}
			else{
				result = MT29F2G_RESULT_OK;
			}
			break;
		}
	}

	if(result != MT29F2G_RESULT_RUN){
		WrStep = 0;
	}

	return result;
}

MT29F2G_Result_t MT29F2G_GetInfo(MT29F2G_t *pMT29F2G, MT29F2G_Info_t *pInfo)
{
	QSPI_CommandTypeDef s_command;
	uint8_t status;
	uint8_t status_tmp;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return MT29F2G_RESULT_NULL;
	}

	// 상태 레지스터를 두 번 읽어 안정화
	MT29F2G_GetStatusRegister(pMT29F2G);

	memset(&s_command, 0, sizeof(s_command));

	// 정보 읽기를 위한 명령 구성 (0x0F, 주소 0xB0)
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_GET_FEATURES;
	s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
	s_command.AddressSize     = QSPI_ADDRESS_8_BITS;
	s_command.Address         = MT29F2G_FEATURE_CONFIG;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_1_LINE;
	s_command.DummyCycles     = 0;
	s_command.NbData          = 1;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_TX_ERROR;
	}
	if(HAL_QSPI_Receive(pMT29F2G->pHandle, &status, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
		return MT29F2G_RESULT_RX_ERROR;
	}

	status_tmp = status;

	// OTP_EN 활성화를 위해 비트 수정
	CLEAR_BIT(status_tmp, MT29F2G_CFG_BITS);
	SET_BIT(status_tmp, MT29F2G_CFG_ACCESS_PARAMETER);

	// OTP_EN 설정 (WriteEnable 포함)
	if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_CONFIG, status_tmp) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	// 정보 페이지 읽기 (주소 0x1000) 후 pInfo에 저장 (블로킹 래퍼)
	{
		MT29F2G_Result_t rr;
		while((rr = MT29F2G_ReadMemory(pMT29F2G, 0x1000, (uint8_t *)pInfo, sizeof(MT29F2G_Info_t))) == MT29F2G_RESULT_RUN){}
		(void)rr;
	}

	// 원래 상태 복원 (WriteEnable 포함)
	if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_CONFIG, status) != MT29F2G_RESULT_OK){
		return MT29F2G_RESULT_TX_ERROR;
	}

	return MT29F2G_RESULT_OK;
}

oResult_t MT29F2G_Init(MT29F2G_t *pMT29F2G, QSPI_HandleTypeDef *pHandle)
{
	static uint8_t MT29F2G_InitStep = 0;
	static uint32_t MT29F2G_InitTimer = 0;
	static uint8_t MT29F2G_VerifyRetry = 0;

	if(pMT29F2G == NULL || pHandle == NULL){
		MT29F2G_InitStep = 0;
		return RESULT_ERROR;
	}

	oResult_t result = RESULT_RUN;

	switch(MT29F2G_InitStep)
	{
		case 0:
			pMT29F2G->pHandle = pHandle;
			pMT29F2G->IsOpen = 0;
			MT29F2G_VerifyRetry = 0;

			HAL_QSPI_DeInit(pMT29F2G->pHandle);

			if(pMT29F2G->pHandle->State == HAL_QSPI_STATE_READY){
				// QSPI 이미 초기화됨 (DeInit에서 QSPI 유지) → Reset 단계로 직행
				oSerial_Log("MT29F2G", "Init QSPI already READY");
				MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MT29F2G_InitStep = 2;
				break;
			}
			else{
				MT29F2G_InitStep++;
			}
			break;
		case 1:
			if(HAL_QSPI_Init(pMT29F2G->pHandle) != HAL_OK){
				oSerial_Log("MT29F2G", "Init QSPIInit FAIL");
				result = RESULT_FAULT;
				break;
			}
			MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			MT29F2G_InitStep = 2;
			break;
		case 2:
			// 50ms 대기 → Reset 전송
			if(oTMR_Elapsed(&MT29F2G_InitTimer, 50, TICKBASE_SYSTICK)){
				QSPI_CommandTypeDef rst_cmd = {0};
				rst_cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
				rst_cmd.Instruction     = MT29F2G_CMD_RESET;
				rst_cmd.AddressMode     = QSPI_ADDRESS_NONE;
				rst_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
				rst_cmd.DataMode        = QSPI_DATA_NONE;
				rst_cmd.DummyCycles     = 0;
				rst_cmd.DdrMode         = QSPI_DDR_MODE_DISABLE;
				rst_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
				rst_cmd.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;
				HAL_QSPI_Command(pMT29F2G->pHandle, &rst_cmd, 1000);
				MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MT29F2G_InitStep = 3;
			}
			break;
		case 3:
			if(oTMR_Elapsed(&MT29F2G_InitTimer, 5, TICKBASE_SYSTICK)){
				MT29F2G_InitStep++;
			}
			break;
		case 4:
			// Protection register (0xA0) - unlock blocks + WP-E enable
			if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_BLOCKLOCK, MT29F2G_LOCK_UNLOCK_ALL) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "Init SetProt FAIL");
				result = RESULT_FAULT;
				break;
			}

			MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			MT29F2G_InitStep++;
			break;
		case 5:
			// Configuration register (0xB0) - ECC_EN(bit4)만. quad는 별도 QE 비트 없음(기본). bit0은 reserved → 0.
			if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_CONFIG, MT29F2G_CFG_NORMAL) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "Init SetCfg FAIL");
				result = RESULT_FAULT;
				break;
			}
			MT29F2G_InitStep++;
			break;
		case 6:
		{
			// feature read-back 검증: 재-전원 직후 칩이 설정을 실제 반영했는지 확인.
			// 미반영(칩 미준비)이면 재-reset부터 재시도 → 준비된 뒤에만 Open. (재-전원 후 read 불능 대응)
			uint8_t cfg = 0;
			uint8_t prot = 0;
			MT29F2G_GetFeature(pMT29F2G, MT29F2G_FEATURE_BLOCKLOCK, &prot);
			MT29F2G_GetFeature(pMT29F2G, MT29F2G_FEATURE_CONFIG, &cfg);

			// ECC_EN(0x10) 반영 + 블록 언락(0xA0 BP[3:0]·TB = bits 0x7C 모두 0) 동시 확인.
			// 언락 미검증 시 무음 락 → program/erase가 P_Fail로 조용히 실패.
			if(((cfg & MT29F2G_CFG_ECC_EN) == MT29F2G_CFG_ECC_EN) && ((prot & MT29F2G_LOCK_BP_MASK) == 0)){
				MT29F2G_InitStep++;
			}
			else if(++MT29F2G_VerifyRetry <= 10){
				oSerial_Log("MT29F2G", "Init verify retry %d (A0=0x%02X B0=0x%02X)", MT29F2G_VerifyRetry, prot, cfg);
				MT29F2G_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MT29F2G_InitStep = 2;   // 재-reset(50ms 대기 후 0xFF)부터 다시
			}
			else{
				oSerial_Log("MT29F2G", "Init verify FAIL A0=0x%02X B0=0x%02X", prot, cfg);
				result = RESULT_FAULT;
			}
			break;
		}
		case 7:
			MT29F2G_GetStatusRegister(pMT29F2G);
			pMT29F2G->IsOpen = 1;
			oSerial_Log("MT29F2G", "Init OK SR=0x%02X", pMT29F2G->State);
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		MT29F2G_InitStep = 0;
	}

	return result;
}

oResult_t MT29F2G_DeInit(MT29F2G_t *pMT29F2G)
{
	static uint8_t DeInitStep = 0;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		DeInitStep = 0;
		return RESULT_OK;
	}

	oResult_t result = RESULT_RUN;

	switch(DeInitStep)
	{
		case 0:
			// 1. Protection register (0xA0) POR 디폴트 원복
			if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_BLOCKLOCK, MT29F2G_LOCK_POR_DEFAULT) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit SetProt FAIL->skip");
			}
			DeInitStep++;
			break;
		case 1:
			// 2. Configuration register (0xB0) POR 디폴트 원복
			if(MT29F2G_SetFeature(pMT29F2G, MT29F2G_FEATURE_CONFIG, MT29F2G_CFG_NORMAL) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit SetCfg FAIL->skip");
			}
			DeInitStep++;
			break;
		case 2:
			// 3. OIP Ready 상태까지 Polling 대기
			if(MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP) != MT29F2G_RESULT_OK){
				oSerial_Log("MT29F2G", "DeInit Poll FAIL->skip");
			}
			DeInitStep++;
			break;
		case 3:
			// 4. QSPI DeInit
			HAL_QSPI_DeInit(pMT29F2G->pHandle);
			pMT29F2G->IsOpen = 0;
			oSerial_Log("MT29F2G", "DeInit OK");
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		DeInitStep = 0;
	}

	return result;
}

/**
  * @brief  블록 소거. tERS(~2~10ms) WIP 대기를 매 사이클 1회 폴링으로 분할한다.
  *         case 0: WREN + BLOCK ERASE(0xD8) 발행(µs, 블로킹) → RUN
  *         case 1: WIP 해제까지 재진입 폴링 → OK/FAULT(erase fail·timeout)
  */
oResult_t MT29F2G_Erase(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock)
{
	static uint8_t EraseStep = 0;
	static uint32_t EraseTimer = 0;
	uint32_t Address = MT29F2G_ADDRESS(NumberOfBlock, 0, 0);
	oResult_t result = RESULT_RUN;
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || !pMT29F2G->IsOpen){
		EraseStep = 0;
		return RESULT_NULL;
	}

	switch(EraseStep)
	{
		case 0:
			// 쓰기 활성화
			if(MT29F2G_WriteEnable(pMT29F2G) != MT29F2G_RESULT_OK){
				result = RESULT_FAULT;
				break;
			}

			memset(&s_command, 0, sizeof(s_command));

			// BLOCK ERASE 명령 (0xd8) 전송
			s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
			s_command.Instruction     = MT29F2G_CMD_BLOCK_ERASE;
			s_command.AddressMode     = QSPI_ADDRESS_1_LINE;
			s_command.AddressSize     = QSPI_ADDRESS_24_BITS;
			s_command.Address         = MT29F2G_ROW_ADDRESS(Address);
			s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
			s_command.DataMode        = QSPI_DATA_NONE;
			s_command.DummyCycles     = 0;
			s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
			s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
			s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

			if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK){
				result = RESULT_FAULT;
				break;
			}

			EraseTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			EraseStep = 1;
			break;

		case 1:
		{
			// tERS WIP 해제를 비블로킹 폴링 (블로킹 AutoPolling 대체)
			int8_t wip = MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_WIP);

			if(wip < 0){
				result = RESULT_FAULT;
			}
			else if(wip == 1){
				result = oTMR_Elapsed(&EraseTimer, NAND_ERASE_TIMEOUT, TICKBASE_SYSTICK) ? RESULT_FAULT : RESULT_RUN;
			}
			else if(MT29F2G_CheckStatusRegister(pMT29F2G, MT29F2G_SR_EFAIL) == 1){ //erase failed
				result = RESULT_FAULT;
			}
			else{
				result = RESULT_OK;
			}
			break;
		}
	}

	if(result != RESULT_RUN){
		EraseStep = 0;
	}

	return result;
}


oResult_t MT29F2G_BadBlockCheck(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock)
{
	uint8_t spare[2];
	// 배드블록 마킹은 블록의 첫 페이지·마지막 페이지 spare에 위치 (Micron 규약) → 두 페이지만 확인 (기존 6→2)
	const uint8_t checkPages[2] = { 0, MT29F2G_PAGE_LENGTH - 1 };
	uint8_t i;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return RESULT_NULL;
	}

	for(i = 0; i < 2; i++)
	{
		uint32_t Address = MT29F2G_ADDRESS(NumberOfBlock, checkPages[i], MT29F2G_SPARE_COLUMN);

		MT29F2G_Result_t rr;
		while((rr = MT29F2G_ReadMemory(pMT29F2G, Address, (uint8_t *)&spare, sizeof(spare))) == MT29F2G_RESULT_RUN){}   // 블로킹 래퍼(비-인덱서 경로)
	    if(rr != MT29F2G_RESULT_OK){
	    	oSerial_Log("MT29F2G", "BBC B=%u P=%u err(%d)", NumberOfBlock, checkPages[i], rr);
	    	return RESULT_FAULT;
	    }

		if(spare[0] != 0xFF || spare[1] != 0xFF) //배드블럭
		{
			return RESULT_ERROR;
		}
	}

	return RESULT_OK;
}

oResult_t MT29F2G_Reset(MT29F2G_t *pMT29F2G)
{
	QSPI_CommandTypeDef s_command;

	if(pMT29F2G == NULL || pMT29F2G->pHandle == NULL){
		return RESULT_NULL;
	}

	memset(&s_command, 0, sizeof(s_command));

	// 리셋 명령 (0xFF) 전송
	s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction     = MT29F2G_CMD_RESET;
	s_command.AddressMode     = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode        = QSPI_DATA_NONE;
	s_command.DummyCycles     = 0;
	s_command.DdrMode         = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode        = QSPI_SIOO_INST_EVERY_CMD;

	if(HAL_QSPI_Command(pMT29F2G->pHandle, &s_command, NAND_TIMEOUT_VALUE) != HAL_OK)
	{
	  return RESULT_FAULT;
	}

	// 리셋 후 메모리 준비 완료까지 자동 폴링
	if(MT29F2G_AutoPolling(pMT29F2G, 0, MT29F2G_SR_WIP) != MT29F2G_RESULT_OK){
		return RESULT_FAULT;
	}

	MT29F2G_GetStatusRegister(pMT29F2G);

	return RESULT_OK;
}

oResult_t MT29F2G_ReadByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size)
{
    if(pMT29F2G == NULL || pData == NULL || Block >= MT29F2G_BLOCK_LENGTH || Page >= MT29F2G_PAGE_LENGTH || ByteIndex >= MT29F2G_MAINDATA_LENGTH){
    	return RESULT_FAULT;
    }

    if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
    	return RESULT_FAULT;
    }

	Size = MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH - ByteIndex);

    // 블로킹 래퍼: ReadMemory는 비블로킹(RUN) 상태머신 — 완료까지 스핀해 기존 호출자 동작을 그대로 유지.
    // 비블로킹 전환은 인덱서 전용 경로(MiStorage_ReadRecordValid_NB)만 적용 (advisor 권고: 단계적 축소).
    {
    	MT29F2G_Result_t r;
    	while((r = MT29F2G_ReadMemory(pMT29F2G, MT29F2G_ADDRESS(Block, Page, ByteIndex), pData, Size)) == MT29F2G_RESULT_RUN){}
    	if(r != MT29F2G_RESULT_OK){
    		return RESULT_FAULT;
    	}
    }

    return RESULT_OK;
}

oResult_t MT29F2G_ReadPage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size)
{
    return MT29F2G_ReadByte(pMT29F2G, Block, Page, 0, pData, Size);
}

oResult_t MT29F2G_WriteByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size)
{
	if(pMT29F2G == NULL || pData == NULL || Block >= MT29F2G_BLOCK_LENGTH || Page >= MT29F2G_PAGE_LENGTH || ByteIndex >= MT29F2G_MAINDATA_LENGTH){
    	return RESULT_FAULT;
    }

    if(pMT29F2G->pHandle->State != HAL_QSPI_STATE_READY){
    	return RESULT_FAULT;
    }

	Size = MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH - ByteIndex);

	switch(MT29F2G_WriteMemory(pMT29F2G, MT29F2G_ADDRESS(Block, Page, ByteIndex), pData, Size))
	{
		case MT29F2G_RESULT_RUN:	return RESULT_RUN;	// 프로그램 진행 중 (비블로킹)
		case MT29F2G_RESULT_OK:		return RESULT_OK;
		default:					return RESULT_FAULT;
	}
}

oResult_t MT29F2G_WritePage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size)
{
	return MT29F2G_WriteByte(pMT29F2G, Block, Page, 0, pData, Size);
}
#endif
