#ifndef INC_ONE_SERIAL_H_
#define INC_ONE_SERIAL_H_

#define ONE_SERIAL_VERSION  0.5

#include "ONE_Common.h"

typedef struct{
	uint32_t (*Write)(uint8_t *pData, uint32_t SizeOfData);
	uint32_t (*Read)(uint8_t *pData, uint32_t SizeOfData);
	void (*Reset)(void);
}oSerialStack_t;

typedef struct{
#if defined(__HAL_UART_ENABLE)
	UART_HandleTypeDef *pUART;
#else
	void *pUART;
#endif

	uint8_t *pTxBuffer;
	uint32_t SizeOfTxBuffer;
	uint32_t IndexOfTxFirst;   /* DMA/IT consumer 포인터 (송신 시작 위치) */
	uint32_t IndexOfTxLast;    /* producer 포인터 (다음 쓰기 위치) */
	uint32_t CountOfTxBuffer;  /* 링버퍼 점유 바이트 (write +, callback -) */
	uint32_t TxChunkSize;      /* 진행 중 DMA/IT chunk 크기 — 완료 시 First 전진용 */
	uint32_t TxCount;
	uint32_t TxErrorCount;
	uint8_t IsTxBusy;

	uint8_t *pRxBuffer;
	uint32_t SizeOfRxBuffer;
	uint32_t IndexOfRxFirst;
	uint32_t IndexOfRxLast;
	uint32_t IndexOfRxFinder;
	uint32_t CountOfRxBuffer;
	uint32_t RxErrorCount;     /* 수신 오류(ORE·FE·NE·PE) 누적 — ErrorCallback 에서 증가 */

	uint32_t FindedCount;

	uint32_t ReceiveTimer;
	uint32_t IdleTimer;

	uint8_t IsRegistered;
} oSerialHandler_t;

/* 링버퍼 점유 바이트 = (Last - First) with wrap */
#define SERIAL_TRANSMIT_BUFFER_COUNT(pSerial)	((pSerial)->IndexOfTxLast < (pSerial)->IndexOfTxFirst ? \
													((pSerial)->SizeOfTxBuffer - (pSerial)->IndexOfTxFirst + (pSerial)->IndexOfTxLast) : \
													((pSerial)->IndexOfTxLast - (pSerial)->IndexOfTxFirst))

#define SERIAL_RECEIVE_BUFFER_COUNT(pSerial)	((pSerial)->IndexOfRxLast < (pSerial)->IndexOfRxFirst ? \
													((pSerial)->SizeOfRxBuffer - (pSerial)->IndexOfRxFirst + (pSerial)->IndexOfRxLast) : \
													((pSerial)->IndexOfRxLast - (pSerial)->IndexOfRxFirst))

#if defined(__HAL_UART_ENABLE)
extern void oSerial_RxCpltCallback(struct __UART_HandleTypeDef *huart);
extern void oSerial_TxCpltCallback(struct __UART_HandleTypeDef *huart);
extern void oSerial_ErrorCallback(struct __UART_HandleTypeDef *huart);
#endif

extern oResult_t oSerial_Write(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData);
extern oResult_t oSerial_Read(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, uint32_t WaitDelay);
extern oResult_t oSerial_ReadSplit(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData, char* pSplitString);
extern oResult_t oSerial_ReadLine(oSerialHandler_t *pSerial, char *pData, uint32_t SizeOfData);

extern void oSerial_PutChar(oSerialHandler_t *pSerial, char* pChar);
extern void oSerial_vPrint(oSerialHandler_t *pSerial, const char* format, va_list args);
extern void oSerial_Printf(oSerialHandler_t *pSerial, const char* format, ...);

extern void oSerial_LogEnagle(oSerialHandler_t *pSerial, uint8_t Enable);
extern void oSerial_Log(char* Title, const char* format, ...);

#endif /* INC_ONE_TIMER_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_Serial.h)

2026-08-20 | v0.2
	- oSerial_Read·oSerial_ReadSplit DMA 재시작 시 RX 인덱스 리셋 추가
	  (IndexOfRxFirst·Last·Finder·FindedCount) — 재시작이 IndexOfRxLast를 0으로
	  되돌리는데 소비 포인터가 남아 버퍼 한 바퀴 분량 잔재를 읽어냈다

2026-09-14 | v0.3
	- oSerial_ReadSplit 복사 상한을 SizeOfData-1 로 — 가득 채우면 NUL 종단이 사라져
	  뒤따르는 strstr 이 pData 밖을 읽고, 매치 시 memset 이 버퍼 밖에 쓴다

2026-10-06 | v0.4
	- IT 수신 READY 재시작(ORE 중단·재Init 뒤)에 AbortReceive + RX 인덱스 리셋 — 기존엔 남은
	  ORE 로 재시작 즉시 재중단되고, 소비 포인터가 남아 지난 바퀴를 읽어 다음 명령까지 잃었다
	- READY 재시작마다 ErrorCallback 재등록 (HAL_UART_Init 이 콜백을 기본값으로 되돌림)
	- IT idle 재시작은 미소비 바이트가 없을 때만 — 판정 직전에 도착한 바이트를 버리던 경합 차단
	- RxErrorCount 추가 (ORE·FE·NE·PE). ErrorCallback 의 IsTxBusy 해제는 gState READY 일 때만

2026-10-06 | v0.5
	- oSerial_ReadSplit 의 100ms 수신 대기 제거 (RESULT_BUSY 반환 없음) — 대기 기준이 버퍼가 빈
	  폴링에서만 갱신돼 연속 수신의 첫 100ms(115200bps 약 1150B)가 1000B 링버퍼를 넘겨 앞 메시지
	  4~5건이 응답 없이 사라졌다 (VMX3264 실기: Set/ChannelConfig 64건 연속 송신 59 OK)
*/
