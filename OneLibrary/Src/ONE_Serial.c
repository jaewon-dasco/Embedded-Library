/*
 * ONE_Serial.c
 *
 *  Version: 0.5 (2026-10-06)
 */

#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "ONE_Math.h"
#include "ONE_Time.h"
#include "ONE_Common.h"
#include "ONE_Serial.h"

#if defined(__HAL_UART_ENABLE)
#define SERIAL_REGISTRY_LOGINDEX 	0
#define SERIAL_REGISTRY_MAXCOUNT 	5

oSerialHandler_t *SerialRegister[SERIAL_REGISTRY_MAXCOUNT];
oSerialHandler_t *LogSerial;

static oResult_t oSerial_TransmitBuffer(oSerialHandler_t *pSerial);

void oSerial_RxCpltCallback(struct __UART_HandleTypeDef *huart)
{
	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] && SerialRegister[i]->pUART == huart && !huart->hdmarx){
			HAL_UART_Receive_IT(huart, (uint8_t *)SerialRegister[i]->pRxBuffer, SerialRegister[i]->SizeOfRxBuffer);
		}
	}
}

void oSerial_TxCpltCallback(struct __UART_HandleTypeDef *huart)
{
	oSerialHandler_t *pHandle;

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		pHandle = SerialRegister[i];

		if(!pHandle || pHandle->pUART != huart){
			continue;
		}

		/* 완료된 chunk 만큼 consumer 포인터 전진 + 카운트 감소 */
		pHandle->IndexOfTxFirst = (pHandle->IndexOfTxFirst + pHandle->TxChunkSize) % pHandle->SizeOfTxBuffer;
		pHandle->CountOfTxBuffer -= pHandle->TxChunkSize;
		pHandle->TxChunkSize = 0;
		pHandle->TxCount++;

		/* 잔여 데이터 있으면 다음 chunk 자동 시작, 없으면 IsTxBusy 클리어 */
		if(pHandle->CountOfTxBuffer > 0){
			oSerial_TransmitBuffer(pHandle);
		}
		else{
			pHandle->IsTxBusy = 0;
		}
		break;
	}
}

/* 링버퍼에서 wrap 안 넘는 연속 구간만 한 번에 송신.
 *   - First + Count <= Size : Count 바이트 한 번에 (연속)
 *   - First + Count >  Size : Size - First 바이트만 (wrap — 다음 콜백에서 나머지 송신)
 * 호출 전제: IsTxBusy=1 이 외부에서 이미 설정됨. 실패 시 함수가 IsTxBusy=0 클리어. */
static oResult_t oSerial_TransmitBuffer(oSerialHandler_t *pSerial)
{
	uint32_t size;
	HAL_StatusTypeDef status;

	if(pSerial->CountOfTxBuffer == 0){
		pSerial->IsTxBusy = 0;
		pSerial->TxChunkSize = 0;
		return RESULT_DONE;
	}

	if(pSerial->IndexOfTxFirst + pSerial->CountOfTxBuffer > pSerial->SizeOfTxBuffer){
		size = pSerial->SizeOfTxBuffer - pSerial->IndexOfTxFirst;   /* wrap: First~end 만 */
	}
	else{
		size = pSerial->CountOfTxBuffer;                            /* 연속 */
	}

	pSerial->TxChunkSize = size;

	if(pSerial->pUART->hdmatx){
		status = HAL_UART_Transmit_DMA(pSerial->pUART, &pSerial->pTxBuffer[pSerial->IndexOfTxFirst], size);
	}
	else{
		status = HAL_UART_Transmit_IT(pSerial->pUART, &pSerial->pTxBuffer[pSerial->IndexOfTxFirst], size);
	}

	if(status != HAL_OK){
		pSerial->IsTxBusy = 0;
		pSerial->TxChunkSize = 0;
		pSerial->TxErrorCount++;
		return RESULT_ERROR;
	}
	else if(pSerial->pUART->TxCpltCallback != oSerial_TxCpltCallback){
		pSerial->pUART->TxCpltCallback = oSerial_TxCpltCallback;
	}

	return RESULT_OK;
}

void oSerial_ErrorCallback(struct __UART_HandleTypeDef *huart)
{
	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] && SerialRegister[i]->pUART == huart){
			if(huart->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE)){
				SerialRegister[i]->RxErrorCount++;
			}

			/* 수신 오류도 이 콜백으로 온다 — 송신이 실제로 멈춘 때만 해제해야 oSerial_Write 가
			   막고 있는 "DMA 종료~콜백 사이 idle 오판" 경합이 다시 열리지 않는다 */
			if(huart->gState == HAL_UART_STATE_READY){
				if(SerialRegister[i]->IsTxBusy){
					SerialRegister[i]->TxErrorCount++;
				}
				SerialRegister[i]->IsTxBusy = 0;
			}
			break;
		}
	}
}

void oSerial_ResetRegister(oSerialHandler_t *pSerial)
{
	if(!pSerial){
		return;
	}

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(SerialRegister[i] == pSerial){
			SerialRegister[i]->IsRegistered = 0;
			SerialRegister[i]->IsTxBusy = 0;
			SerialRegister[i]->IndexOfTxFirst = 0;
			SerialRegister[i]->IndexOfTxLast = 0;
			SerialRegister[i]->CountOfTxBuffer = 0;
			SerialRegister[i]->TxChunkSize = 0;
			SerialRegister[i]->pUART->RxCpltCallback = 0;
			SerialRegister[i]->pUART->TxCpltCallback = 0;
			SerialRegister[i]->pUART->ErrorCallback = 0;
			SerialRegister[i] = 0;
			break;
		}
	}
}

void oSerial_SetRegister(oSerialHandler_t *pSerial)
{
	if(!pSerial || !pSerial->pUART || !pSerial->pRxBuffer){
		return;
	}

	for(int i=0; i<SERIAL_REGISTRY_MAXCOUNT; i++){
		if(!SerialRegister[i] || SerialRegister[i] == pSerial){
			SerialRegister[i] = pSerial;
			SerialRegister[i]->IsRegistered = 1;
			SerialRegister[i]->IsTxBusy = 0;
			SerialRegister[i]->IndexOfTxFirst = 0;
			SerialRegister[i]->IndexOfTxLast = 0;
			SerialRegister[i]->CountOfTxBuffer = 0;
			SerialRegister[i]->TxChunkSize = 0;
			SerialRegister[i]->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			SerialRegister[i]->pUART->TxCpltCallback = oSerial_TxCpltCallback;
			SerialRegister[i]->pUART->ErrorCallback  = oSerial_ErrorCallback;
			break;
		}
	}
}

oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData)
{
	uint32_t freeSpace;
	uint8_t needStart;
	HAL_StatusTypeDef status;

	if(pSerial == NULL || pSerial->pUART == NULL || pData == NULL || SizeOfData == 0){
		return RESULT_ERROR;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	/* TxBuffer 미설정 시 블로킹 송신 fallback */
	if(pSerial->pTxBuffer == NULL || pSerial->SizeOfTxBuffer < 50){
		status = HAL_UART_Transmit(pSerial->pUART, (uint8_t *)pData, SizeOfData, 1000);
		if(status == HAL_OK){
			pSerial->TxCount++;
			return RESULT_OK;
		}
		pSerial->TxErrorCount++;
		return RESULT_ERROR;
	}

	/* === Critical: 링버퍼 상태 변경 (ISR과 공유) === */
	__disable_irq();

	/* TX 진행 여부 판단:
	   - IsTxBusy(SW): TxCpltCallback이 bookkeeping 완료 후에만 0으로 클리어
	   - HW state: DMA/UART 컨트롤러의 실제 진행 상태
	   둘 중 하나라도 busy면 진행 중으로 간주.
	   HW만 보면 DMA 종료~콜백 실행 사이의 race window에서 idle로 오판하여
	   미처리 청크가 재전송되는 버그 발생함. */
	uint8_t hwBusy = 0;
	if(pSerial->pUART->hdmatx != NULL){
		hwBusy = (pSerial->pUART->hdmatx->State == HAL_DMA_STATE_BUSY);
	}
	else{
		HAL_UART_StateTypeDef st = pSerial->pUART->gState;
		hwBusy = (st == HAL_UART_STATE_BUSY_TX) || (st == HAL_UART_STATE_BUSY_TX_RX);
	}
	uint8_t txInProgress = pSerial->IsTxBusy || hwBusy;

	freeSpace = pSerial->SizeOfTxBuffer - pSerial->CountOfTxBuffer;
	if(SizeOfData > freeSpace){
		/* 공간 부족 — HW idle 이면서 버퍼에 데이터 남아 있으면 stale → DMA 재시작 */
		uint8_t needRecovery = (!txInProgress && pSerial->CountOfTxBuffer > 0);
		if(needRecovery){
			pSerial->IsTxBusy = 1;
		}
		__enable_irq();

		if(needRecovery){
			oSerial_TransmitBuffer(pSerial);
		}
		return RESULT_BUSY;   /* 데이터 일부 적재 금지 (all-or-nothing) */
	}

	/* 링버퍼 적재 — byte 단위 wrap 처리 */
	for(uint32_t i = 0; i < SizeOfData; i++){
		pSerial->pTxBuffer[pSerial->IndexOfTxLast] = (uint8_t)pData[i];
		pSerial->IndexOfTxLast = (pSerial->IndexOfTxLast + 1) % pSerial->SizeOfTxBuffer;
	}
	pSerial->CountOfTxBuffer += SizeOfData;

	/* HW idle 이면 본 호출이 시작 책임. 진행 중이면 콜백이 자동으로 이어 송신. */
	needStart = !txInProgress;
	if(needStart){
		pSerial->IsTxBusy = 1;
	}

	__enable_irq();
	/* === End critical === */

	if(needStart){
		return oSerial_TransmitBuffer(pSerial);
	}

	return RESULT_OK;
}

oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, uint32_t WaitDelay)
{
	int cnt = 0;

	if(pSerial == NULL || pData == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
		return RESULT_NULL;
	}
	else if(pSerial->pUART->gState == HAL_UART_STATE_RESET){
		return RESULT_NULL;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	if(pSerial->pUART->hdmarx){
		/* DMA 모드 */
		if(pSerial->pUART->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE|HAL_UART_ERROR_DMA)){
			HAL_UART_AbortReceive(pSerial->pUART);
		}

		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(!ONE_DMA_IS_CIRCULAR(pSerial->pUART->hdmarx)){
				ONE_DMA_SET_CIRCULAR(pSerial->pUART->hdmarx);
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}

			/* DMA 재시작으로 NDTR이 돌아가 IndexOfRxLast가 0으로 복귀한다 — 소비 포인터를
			   함께 되돌리지 않으면 First가 Last를 앞지르도록 남아 버퍼 한 바퀴 분량의
			   잔재를 유효 데이터로 내보낸다 (IT idle 재시작 경로와 동일 규약) */
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;

			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			pSerial->pUART->ErrorCallback = oSerial_ErrorCallback;	/* HAL_UART_Init 이 기본값으로 되돌린다 */
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - ONE_DMA_GET_COUNTER(pSerial->pUART->hdmarx);
	}
	else{
		/* 인터럽트 모드 */
		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			/* IT 정상 순환은 RxCpltCallback 안에서 재시작되므로 여기 READY 는 첫 시작·ORE 중단·재Init 뒤다.
			   남은 ORE/RDR 을 지우지 않으면 재시작 즉시 다시 중단되고, 인덱스를 되돌리지 않으면 지난 바퀴를 읽는다 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;
			pSerial->IdleTimer = 0;

			if(HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}

			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			pSerial->pUART->ErrorCallback = oSerial_ErrorCallback;	/* HAL_UART_Init 이 기본값으로 되돌린다 */
		}
		else if(pSerial->pUART->RxXferSize && pSerial->IdleTimer && pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount == pSerial->IndexOfRxFirst
				&& oTMR_Elapsed(&pSerial->IdleTimer, MATH_MAX(WaitDelay, 100), TICKBASE_SYSTICK)){
			/* IT 모드 idle 타임아웃: 수신 없으면 재시작. 미소비 바이트가 있으면 직전 폴링 뒤에 도착한
			   것이라 재시작하면 버려지므로 건너뛴다 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IdleTimer = 0;
			HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer);
			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount;
	}

	if(pSerial->IndexOfRxLast == pSerial->IndexOfRxFirst){
		pSerial->ReceiveTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		return RESULT_NULL;
	}
	else{
		pSerial->IdleTimer = oTMR_GetTick(TICKBASE_SYSTICK);

		if(!oTMR_Elapsed(&pSerial->ReceiveTimer, MATH_MAX(WaitDelay, 100), TICKBASE_SYSTICK)){
			return RESULT_BUSY;
		}
	}

	memset(pData, 0, SizeOfData);

	while(pSerial->IndexOfRxLast != pSerial->IndexOfRxFirst && cnt < SizeOfData){
		*pData = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		pData++;
		cnt++;
	}

	return RESULT_OK;
}

oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, char *pSplitString)
{
	char *pStr;
	int32_t Cnt = 0;
	int32_t SplitLength = 0;

	if(pSerial == NULL || pData == NULL || pSerial->pUART == NULL || pSerial->pRxBuffer == NULL || pSerial->SizeOfRxBuffer <= 0){
		return RESULT_NULL;
	}
	else if(pSerial->pUART->gState == HAL_UART_STATE_RESET){
		return RESULT_NULL;
	}

	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	if(pSerial->pUART->hdmarx){
		/* DMA 모드 */
		if(pSerial->pUART->ErrorCode & (HAL_UART_ERROR_ORE|HAL_UART_ERROR_FE|HAL_UART_ERROR_NE|HAL_UART_ERROR_PE|HAL_UART_ERROR_DMA)){
			HAL_UART_AbortReceive(pSerial->pUART);
		}

		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			if(!ONE_DMA_IS_CIRCULAR(pSerial->pUART->hdmarx)){
				ONE_DMA_SET_CIRCULAR(pSerial->pUART->hdmarx);
				HAL_DMA_Init(pSerial->pUART->hdmarx);
			}

			if(HAL_UART_Receive_DMA(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}

			/* DMA 재시작으로 NDTR이 돌아가 IndexOfRxLast가 0으로 복귀한다 — 소비 포인터를
			   함께 되돌리지 않으면 First가 Last를 앞지르도록 남아 버퍼 한 바퀴 분량의
			   잔재를 유효 데이터로 내보낸다 (IT idle 재시작 경로와 동일 규약) */
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;

			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			pSerial->pUART->ErrorCallback = oSerial_ErrorCallback;	/* HAL_UART_Init 이 기본값으로 되돌린다 */
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - ONE_DMA_GET_COUNTER(pSerial->pUART->hdmarx);
	}
	else{
		/* 인터럽트 모드 */
		if(pSerial->pUART->RxState == HAL_UART_STATE_READY){
			/* IT 정상 순환은 RxCpltCallback 안에서 재시작되므로 여기 READY 는 첫 시작·ORE 중단·재Init 뒤다.
			   남은 ORE/RDR 을 지우지 않으면 재시작 즉시 다시 중단되고, 인덱스를 되돌리지 않으면 지난 바퀴를 읽는다 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;
			pSerial->IdleTimer = 0;

			if(HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer) != HAL_OK){
				HAL_UART_AbortReceive(pSerial->pUART);
				return RESULT_ERROR;
			}

			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
			pSerial->pUART->ErrorCallback = oSerial_ErrorCallback;	/* HAL_UART_Init 이 기본값으로 되돌린다 */
		}
		else if(pSerial->pUART->RxXferSize && pSerial->IdleTimer && pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount == pSerial->IndexOfRxFirst
				&& oTMR_Elapsed(&pSerial->IdleTimer, 1000, TICKBASE_SYSTICK)){
			/* IT 모드 idle 타임아웃: 1초 이상 수신 없으면 재시작. 미소비 바이트가 있으면 직전 폴링 뒤에
			   도착한 것이라 재시작하면 버려지므로 건너뛴다 */
			HAL_UART_AbortReceive(pSerial->pUART);
			pSerial->IndexOfRxFirst = 0;
			pSerial->IndexOfRxLast = 0;
			pSerial->IndexOfRxFinder = 0;
			pSerial->FindedCount = 0;
			pSerial->IdleTimer = 0;

			HAL_UART_Receive_IT(pSerial->pUART, (uint8_t *)pSerial->pRxBuffer, pSerial->SizeOfRxBuffer);
			pSerial->pUART->RxCpltCallback = oSerial_RxCpltCallback;
		}

		pSerial->IndexOfRxLast = pSerial->pUART->RxXferSize - pSerial->pUART->RxXferCount;
	}

	//split string check
	if(pSplitString != NULL){
		SplitLength = strlen(pSplitString);
	}
	else{
		pSerial->FindedCount = 0;
	}

	if(pSerial->IndexOfRxLast == pSerial->IndexOfRxFirst){
		return RESULT_NULL;
	}

	/* 종결자로 메시지 끝을 판정하므로 oSerial_Read 식 수신 대기를 두지 않는다 — 대기 중 쌓인
	   연속 수신이 링버퍼를 한 바퀴 넘긴다 */
	pSerial->IdleTimer = oTMR_GetTick(TICKBASE_SYSTICK);

	while(pSerial->IndexOfRxLast != pSerial->IndexOfRxFinder && pSerial->FindedCount < SplitLength){
		if(*(pSplitString + pSerial->FindedCount) == *(pSerial->pRxBuffer + pSerial->IndexOfRxFinder)){
			pSerial->FindedCount++;
		}
		else{
			pSerial->FindedCount = 0;
		}

		pSerial->IndexOfRxFinder = (pSerial->IndexOfRxFinder+1) % pSerial->SizeOfRxBuffer;
	}

	if(pSerial->FindedCount != SplitLength){
		return RESULT_NULL;
	}

	//copy to
	memset(pData, 0, SizeOfData);

	/* 마지막 1바이트는 memset이 깔아둔 NUL 자리로 남긴다 — 가득 채우면 뒤따르는
	   strstr이 pData 밖을 읽고, 매치 시 memset이 버퍼 밖에 쓴다 */
	while(pSerial->IndexOfRxFinder != pSerial->IndexOfRxFirst && Cnt < (int32_t)SizeOfData - 1){
		*(pData+Cnt) = *(pSerial->pRxBuffer + pSerial->IndexOfRxFirst);
		*(pSerial->pRxBuffer + pSerial->IndexOfRxFirst) = 0;

		pSerial->IndexOfRxFirst = (pSerial->IndexOfRxFirst+1) % pSerial->SizeOfRxBuffer;
		Cnt++;
	}

	//remove split string
	if(pSplitString != NULL && (pStr=strstr(pData, pSplitString)) != NULL){
		memset(pStr, 0, SplitLength);
	}

	pSerial->FindedCount = 0;

	return RESULT_OK;
}


oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData)
{
	return oSerial_ReadSplit(pSerial, pData, SizeOfData, "\r\n");
}

void oSerial_PutChar(oSerialHandler_t *pSerial, char* pChar)
{
	if(pSerial == NULL || pChar == NULL || pSerial->pUART == NULL){
		return;
	}

	/* gState 체크 안 함 — DMA TX 진행 중이라도 oSerial_Write 가 링버퍼에 적재, callback 이 이어 전송 */
	if(!pSerial->IsRegistered){
		oSerial_SetRegister(pSerial);
	}

	oSerial_Write(pSerial, pChar, strlen(pChar));
}

void oSerial_vPrint(oSerialHandler_t *pSerial, const char* format, va_list args)
{
    char Buffer[256];
    int len;

    if (pSerial == NULL || pSerial->pUART == NULL) return;

    len = vsnprintf(Buffer, sizeof(Buffer), format, args);
    if (len <= 0) return;
    if (len >= (int)sizeof(Buffer)) len = sizeof(Buffer) - 1;

    oSerial_Write(pSerial, Buffer, (uint32_t)len);
}


void oSerial_Printf(oSerialHandler_t *pSerial, const char* format, ...)
{
    if(pSerial == NULL || pSerial->pUART == NULL){
        return;
    }

    va_list args;
    va_start(args, format);

    oSerial_vPrint(pSerial, format, args);

    va_end(args);
}

void oSerial_PrintLine(oSerialHandler_t *pSerial, const char* format, ...)
{
    char Buffer[256];
    int len;
    int has_crlf;

    if(pSerial == NULL || pSerial->pUART == NULL){
        return;
    }

    va_list args;
    va_start(args, format);

    has_crlf = (strstr(format, "\r\n") != NULL);
    len = vsnprintf(Buffer, sizeof(Buffer), format, args);
    if(len < 0) len = 0;
    if(len >= (int)sizeof(Buffer)) len = sizeof(Buffer) - 1;
    if(!has_crlf && len < (int)sizeof(Buffer) - 2){
        Buffer[len++] = '\r';
        Buffer[len++] = '\n';
    }

    oSerial_Write(pSerial, Buffer, (uint32_t)len);

    va_end(args);
}

void oSerial_LogEnagle(oSerialHandler_t *pSerial, uint8_t Enable)
{
	if(!Enable || !pSerial || !pSerial->pUART){
		LogSerial = NULL;
	}
	else{
		LogSerial = pSerial;
	}
}

void oSerial_Log(char* Title, const char* format, ...)
{
    va_list args;
    char Buffer[256];
    int len;
    int has_crlf;

	if(LogSerial && strlen(Title) && strlen(format)){
		va_start(args, format);

		has_crlf = (strstr(format, "\r\n") != NULL);
		len = snprintf(Buffer, sizeof(Buffer), "Log | %s | ", Title);
		if(len < 0) len = 0;
		if(len >= (int)sizeof(Buffer)) len = sizeof(Buffer) - 1;
		len += vsnprintf(Buffer + len, sizeof(Buffer) - len, format, args);
		if(len >= (int)sizeof(Buffer)) len = sizeof(Buffer) - 1;
		if(!has_crlf && len < (int)sizeof(Buffer) - 2){
			Buffer[len++] = '\r';
			Buffer[len++] = '\n';
		}

		oSerial_Write(LogSerial, Buffer, (uint32_t)len);

		va_end(args);
	}
}
#endif
