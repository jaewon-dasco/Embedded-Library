#ifndef INC_LORA_NML35_H_
#define INC_LORA_NML35_H_

#define LORA_NML35_VERSION                               0.1

#include "ONE_Common.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"

/*************************************************************************************/
/*Data packet define ****************************************************************/
/*************************************************************************************/
#define LORA_PARAMTER_INITIALIZER(Network,Address,Mode)  {Network,Address,LORA_DEFAULT_SERVER_ADDRESS,Mode}

#define LORA_MODE_STANBY()                               LoRaDevice.Parameter.OperatingMode |= LoRaOperMode_Stanby
#define LORA_MODE_OPERATING()                            LoRaDevice.Parameter.OperatingMode &= ((uint8_t)~LoRaOperMode_Stanby)

#define LORA_RADIO_BROADCAST_MAXSIZE                     10
#define LORA_ADDRESS_MAX                                 0xFFFFFEFE
#define LORA_DEFAULT_SERVER_ADDRESS                      (LORA_BROADCAST_ADDRESS-1)
#define LORA_BROADCAST_ADDRESS                           LORA_ADDRESS_MAX

#define LORA_RADIO_TRANSFER_MAX_SIZE                     2048
#define LORA_UART_DATA_MAX_SIZE                          (LORA_RADIO_TRANSFER_MAX_SIZE+20)
#define LORA_UART_TX_BUFF_MAXSIZE                        (200)
#define LORA_UART_RX_BUFF_MAXSIZE                        (200)

// PACKET_FLAG
#define LORA_PACKET_STARTBIT                             0x3C
#define LORA_PACKET_ENDBIT                               0x3E

// PACKET_PECKET_PID
#define PID_MASK                                         0x80
#define PID_WAKEUP                                       (PID_MASK|0x01)
#define PID_HARTBEAT                                     (PID_MASK|0x02)
#define PID_RESPONSE                                     (PID_MASK|0x03)
#define PID_RECEIVED_RADIO_DATA                          (PID_MASK|0x04)
#define PID_TRANSMIT_RADIO_DATA                          (PID_MASK|0x05)
#define PID_PARAMETER_READ                               (PID_MASK|0x06)
#define PID_PARAMETER_WRITE                              (PID_MASK|0x07)
#define PID_PARAMETER_DATA                               (PID_MASK|0x08)
#define PID_NOTSUPPORT                                   (PID_MASK|0x0F)
#define PID_MAX                                          PID_NOTSUPPORT

#define PARAMETER_DEVINFO                                0x01
#define PARAMETER_NETWORK                                0x02
#define PARAMETER_ADDRESS                                0x03
#define PARAMETER_SERVERADDRESS                          0x04
#define PARAMETER_CHANNEL                                0x05
#define PARAMETER_OPERATING_MODE                         0x06
#define PARAMETER_SAVE                                   0xFE

#define RESPONSE_OK                                      64
#define RESPONSE_ERROR                                   128
#define RESPONSE_NOTSUPPORT                              255

// TypeDef
typedef enum{
	LoRaOperMode_Null      = 0x00,
	LoRaOperMode_Stanby    = 0x01,
	LoRaOperMode_Node      = 0x02,
	LoRaOperMode_Server    = 0x04,
	LoRaOperMode_Repeater  = 0x08
}LoRaOperatingMode_t;

typedef enum{
	LORA_RESULT_NULL            = 0,
	LORA_RESULT_INIT,
	LORA_RESULT_BUSY,
	LORA_RESULT_RUN,
	LORA_RESULT_OK,
	LORA_RESULT_DONE,
	LORA_RESULT_ABORT,
	LORA_RESULT_ERROR,
	LORA_RESULT_TIMEOUT,
	LORA_RESULT_SERIAL_TIMEOUT,
	LORA_RESULT_RADIO_FAIL,
	LORA_RESULT_NOTSUPPORT,
	LORA_RESULT_OFFLINE,
	LORA_RESULT_RECEIVED_NACK,
	LORA_RESULT_IS_NOT_MINE,
	LORA_RESULT_MAILBOX_FULL,
	LORA_RESULT_LOT_OF_TRAFFIC,
}LoRaResult_t;

typedef void (*RadioReceiveHandler_t)(uint32_t, uint16_t, uint8_t*); //(SrcAddress, Length, Payload)

#pragma pack(1)
typedef struct LoRaStatusDef{
	union{
		struct{
			uint8_t		RadioHeavyTraffic			: 1;   //IntterupCount > 20 in 3sec
			uint8_t		ServerExists				: 1;
			uint8_t		FrozenReceive				: 1;
			uint8_t		ReceptionSensitivityHigh	: 1;    //listen before talk sensitivity to high.
			uint8_t		DetectedRadioNoise			: 1;  //DecodeFailCount > 5 in 3sec
			uint8_t		DetectedOtherNetwork		: 1;    //OtherNetworkDetectCount > 1 in 3sec
			uint8_t		NetworkOnline				: 1;   //Beacon active state
			uint8_t		MailboxBusy					: 1;
		};

		uint8_t IsFalut : 3;
		uint16_t Bits;
	}Radio;

	struct{
		uint8_t		IsBusy : 1;
		uint8_t		IsReceiveMode : 1;
		uint8_t		IsTransmitMode : 1;
		uint8_t		IsFault : 1;
		uint8_t		IsSuccess : 1;
		uint8_t		IsLock : 1;
		uint8_t		IsNewData : 1;
		uint8_t		IsReady : 1;
		uint8_t		IsWaitBeacon : 1;
		uint8_t		: 7;
	}Transporter;

	struct{
		uint8_t	Okay : 1;
		uint8_t	Busy : 1;
		uint8_t	LockedTransporter : 1;
		uint8_t	Stanby : 1;
		uint8_t	TimeOut : 1;
		uint8_t		: 3;
	}Beacon;

	struct{
		uint8_t		SequenceNumber;
		uint8_t		RadioReceiveCount;
		uint8_t		Sign;
		uint32_t 	Timestamp;

		uint8_t 	IsTimeout;
		uint8_t 	IsFrozen;
	}Heartbeat;

	int16_t 	RSSI;

	uint8_t 	IsOpen				: 1;
	uint8_t 	IsParameterSync		: 1;
	uint8_t 	IsWakeup			: 1;
	uint8_t 	IsUARTBusy			: 1;
	uint8_t 	IsUartFault			: 1;
	uint8_t 	IsSleepTimeout		: 1;
	uint8_t 	IsOkay				: 1;

	uint32_t 	UARTReceiveTimestamp;
	uint8_t 	UARTReceiveCount;
	uint8_t 	UARTFaultCount;
} LoRaStatus_t;

typedef struct LoRaParameterDef{
	uint8_t					Network;
	uint32_t				Address;
	uint32_t				ServerAddress;
	LoRaOperatingMode_t		OperatingMode;
} LoRaParameter_t;

typedef struct LoRa_DeviceTypeDef{
	UART_HandleTypeDef		*pUART;
	struct{
		uint8_t RxBuffer[LORA_UART_RX_BUFF_MAXSIZE];
		uint8_t TxBuffer[LORA_UART_TX_BUFF_MAXSIZE];

		uint16_t RxFirstIndex;
		uint16_t RxLastIndex;
		uint16_t TxFirstIndex;
		uint16_t TxLastIndex;

		uint8_t ReceivedData[LORA_UART_DATA_MAX_SIZE];
		uint8_t TransmitData[LORA_UART_DATA_MAX_SIZE];
	}Buffer;

	LoRaStatus_t 			Status;
	LoRaParameter_t			Parameter;
	uint8_t					Signature;
	uint8_t 				PrameterIndex;

	RadioReceiveHandler_t	ReceiveRadioDataCallback;
} LoRaDevice_t;
#pragma pack()

//Variable
extern LoRaDevice_t LoRaDevice;

//Function
extern uint32_t LoRa_SerialRead(uint8_t *pData, uint32_t SizeOfData);
extern uint32_t LoRa_SerialWrite(uint8_t *pData, uint32_t SizeOfData);
extern void LoRa_SerialReset();
extern LoRaResult_t LoRa_ReceiveCopyTo(uint8_t *pDest, uint8_t *pSrc, uint32_t StartIndex, uint32_t Count);
extern LoRaResult_t LoRa_SetParameter();
extern LoRaResult_t LoRa_SentRadioData(uint32_t DestAddress, uint32_t DLC, uint8_t *pData);
extern LoRaResult_t LoRa_Sleep(uint32_t Second);
extern LoRaResult_t LoRa_SetOperatingMode(LoRaOperatingMode_t Mode);
extern LoRaResult_t LoRa_Open(UART_HandleTypeDef *pUART, LoRaParameter_t *pParameter, RadioReceiveHandler_t Callback);
extern LoRaResult_t LoRa_Close();
extern void LoRaRun();
#endif

/* History

2026-06-26 | v0.1
	- baseline (LoRa_NML35.h)
*/
