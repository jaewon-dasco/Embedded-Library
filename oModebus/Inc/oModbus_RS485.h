/*
 * oModbus_RS485.h
 *
 *  Modbus RTU 슬레이브 (RS485 전용) — 프로젝트 독립 (레지스터 맵은 앱 콜백으로 주입)
 */

#ifndef INC_OMODBUS_RS485_H_
#define INC_OMODBUS_RS485_H_

#define OMODBUS_RS485_VERSION   0.61

#include "ONE_Common.h"
#include "ONE_Serial.h"

#define OMODBUS_ADU_MAXSIZE     256                    /* RTU ADU 최대 (주소1+PDU253+CRC2) */
#define OMODBUS_BROADCAST_ADDR  0x00
#define OMODBUS_DE_POLARITY     UART_DE_POLARITY_HIGH  /* RS485 DE 극성 (보드별 조정) */
#define OMODBUS_T35_MS          2                      /* 프레임 종료 침묵 (SysTick 1ms 분해능 → ≥1.75ms 규격 충족) */
#define OMODBUS_MAX_READ_REGS   125                    /* FC03/04 1요청 최대 레지스터 */
#define OMODBUS_MAX_WRITE_REGS  123                    /* FC16 1요청 최대 레지스터 */

typedef enum{
	oModbus_FC_ReadCoils           = 0x01,
	oModbus_FC_ReadDiscreteInputs  = 0x02,
	oModbus_FC_ReadHolding         = 0x03,
	oModbus_FC_ReadInput           = 0x04,
	oModbus_FC_WriteSingle         = 0x06,
	oModbus_FC_WriteMultiple       = 0x10,
}oModbus_FunctionCode_t;

typedef enum{
	oModbus_Exception_None             = 0x00,
	oModbus_Exception_IllegalFunction  = 0x01,
	oModbus_Exception_IllegalAddress   = 0x02,
	oModbus_Exception_IllegalValue     = 0x03,
	oModbus_Exception_SlaveFailure     = 0x04,
}oModbus_Exception_t;

/* 앱이 데이터 모델을 주입하는 콜백 — 재사용 핵심 (NULL이면 해당 FC 미지원) */
typedef oModbus_Exception_t (*oModbus_ReadReg_t) (uint16_t Address, uint16_t *pValue);
typedef oModbus_Exception_t (*oModbus_WriteReg_t)(uint16_t Address, uint16_t  Value);

typedef struct{
	oModbus_ReadReg_t	ReadHolding;    /* FC03 */
	oModbus_ReadReg_t	ReadInput;      /* FC04 */
	oModbus_WriteReg_t	WriteHolding;  /* FC06 / FC16 */
}oModbus_Callbacks_t;

typedef struct{
	oSerialHandler_t	*pSerial;                   /* 외부 공유 직렬 핸들 (Modbus·Log·Calibration) — 앱 소유, 포인터 참조. 버퍼도 핸들러가 보유 */
	uint8_t				SlaveAddress;
	oModbus_Callbacks_t	Callbacks;

	struct{
		uint8_t		Frame[OMODBUS_ADU_MAXSIZE];  /* 조립 중인 ADU */
		uint16_t	Len;
		uint32_t	T35Timer;                       /* 프레임 침묵 판정 tick */
		uint16_t	RxTail;                         /* 링버퍼 소비 인덱스 */
		uint8_t		Receiving;                   /* 프레임 수신 진행 중 */
	}State;

	struct{
		uint32_t RxFrames;
		uint32_t TxFrames;
		uint32_t CrcErrors;
		uint32_t Exceptions;
	}Stat;

	uint8_t IsInitialized;                          /* oModbus_Init 완료 여부 — Process 진입 게이트 */
}oModbus_t;

extern uint16_t  oModbus_CRC16(const uint8_t *pData, uint16_t Length);
extern oResult_t oModbus_Init(oModbus_t *pModbus, oSerialHandler_t *pSerial, uint8_t SlaveAddress, const oModbus_Callbacks_t *pCallbacks);
extern oResult_t oModbus_DeInit(oModbus_t *pModbus);
extern oResult_t oModbus_Process(oModbus_t *pModbus);

#endif /* INC_OMODBUS_RS485_H_ */

/* History

2026-07-08 | v0.1
	- baseline — RTU 슬레이브 (FC03/04/06/16), 콜백형 레지스터 맵, oSerial DMA 바인딩
2026-07-08 | v0.2
	- oModbus_Init에 콜백 인자 통합 + UART 재초기화, oModbus_SetCallbacks 제거
	- oModbus_DeInit 추가 (수신 중단·UART DeInit)
2026-07-08 | v0.3
	- UART 초기화 위임: oModbus_Init에 pfUartInit 함수포인터 추가 (전송계층 앱 결정)
	  → RS485 DE 등 보드 특화 init을 모듈에서 분리 (재사용성)
2026-07-08 | v0.4
	- RS485 전용 확정 — pfUartInit 제거, oModbus_Init이 HAL_RS485Ex_Init 직접 호출
	  DE 극성은 OMODBUS_DE_POLARITY 매크로 (보드별 조정)
2026-07-09 | v0.5
	- 직렬 핸들 외부화: oModbus_t가 oSerialHandler_t를 내장하지 않고 포인터(pSerial)로 참조
	  → Modbus·Log·Calibration이 단일 공유 핸들러 병렬 소비. RS485 init·RX DMA arm은 oModbus_Init에 유지
2026-09-03 | v0.6
	- IsInitialized 플래그 추가 — oModbus_Init 이 세우고 oModbus_Process 가 진입 조건으로 검사한다.
	  Init 을 건너뛴 구성(예: 로그 전용 빌드)에서 Process 가 미바인딩 핸들을 만지는 것을 막는다.
	  oModbus_DeInit 에서도 0 으로 내린다 — UART 를 내려놓고 플래그만 남으면 게이트가 무의미해진다

2026-10-20 | v0.61
	- 브로드캐스트 응답 억제 판정을 응답 주소 → 요청 주소 기준으로 수정. 읽기 응답과 예외 응답은
	  resp[0] 에 자기 주소를 넣으므로 기존 pFrame[0] 검사만으로는 브로드캐스트 요청에도 응답했다
	  (규약 위반이자, 대상 개체가 둘이면 버스 충돌)
*/
