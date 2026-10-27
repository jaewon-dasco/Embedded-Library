/*
 * SerialTranceiver.h
 *
 *  Created on: Jan 21, 2025
 *      Author: JONE
 */

#ifndef INC_ONE_SERIALTRANSCEIVER_H_
#define INC_ONE_SERIALTRANSCEIVER_H_

#define ONE_SERIALTRANSCEIVER_VERSION    0.1

#include "ONE_Serial.h"
#include "ONE_Common.h"
#include "ONE_Memory.h"

#define SERIALTRCV_PACKET_MAXSIZE        (SERIALTRCV_PAYLOAD_MAXSIZE+2)
#define SERIALTRCV_PAYLOAD_MAXSIZE       50  //max.255
#define SERIALTRCV_DATASPLIT_SIZE        (SERIALTRCV_PAYLOAD_MAXSIZE-2)
#define SERIALTRCV_TOTALDATA_MAXSIZE     (SERIALTRCV_PAYLOAD_MAXSIZE * 65534)

#define SERIALTRCV_MESSAGE_TYPE_MASK     0xF0
#define SERIALTRCV_MESSAGE_TYPE_CHCK     (SERIALTRCV_MESSAGE_TYPE_MASK | 0x01)
#define SERIALTRCV_MESSAGE_TYPE_RXSTART  (SERIALTRCV_MESSAGE_TYPE_MASK | 0x02)
#define SERIALTRCV_MESSAGE_TYPE_RXDATA   (SERIALTRCV_MESSAGE_TYPE_MASK | 0x03)
#define SERIALTRCV_MESSAGE_TYPE_TXSTART  (SERIALTRCV_MESSAGE_TYPE_MASK | 0x04)
#define SERIALTRCV_MESSAGE_TYPE_TXDATA   (SERIALTRCV_MESSAGE_TYPE_MASK | 0x05)
#define SERIALTRCV_MESSAGE_TYPE_ACK      (SERIALTRCV_MESSAGE_TYPE_MASK | 0x06)


#define SERIALTRCV_CTRL_MASK             0x40
#define SERIALTRCV_CTRL_RX               (SERIALTRCV_CTRL_MASK | 0x01)
#define SERIALTRCV_CTRL_TX               (SERIALTRCV_CTRL_MASK | 0x02)
#define SERIALTRCV_CTRL_START            (SERIALTRCV_CTRL_MASK | 0x03)
#define SERIALTRCV_CTRL_DATA             (SERIALTRCV_CTRL_MASK | 0x04)
#define SERIALTRCV_CTRL_END              (SERIALTRCV_CTRL_MASK | 0x05)

#define SERIALTRCV_FLOW_MASK             0x80
#define SERIALTRCV_FLOW_TX               (SERIALTRCV_FLOW_MASK | 0x01)
#define SERIALTRCV_FLOW_RX               (SERIALTRCV_FLOW_MASK | 0x02)

#define SERIALTRCV_ACK_MASK              0xF0
#define SERIALTRCV_ACK_OK                (SERIALTRCV_ACK_MASK | 0x01)
#define SERIALTRCV_ACK_ERROR             (SERIALTRCV_ACK_MASK | 0x0F)

typedef void (*TransceiverCallback_t)(oResult_t Result, uint8_t*pData, uint32_t SizeOfData);

typedef struct{
	union{
		struct{
			uint8_t Number;
			uint8_t Type;
			uint8_t Payload[SERIALTRCV_PAYLOAD_MAXSIZE];
		};

		uint8_t Buffer[SERIALTRCV_PACKET_MAXSIZE];
	};

	uint8_t SizeOfPayload;
}oSerialTransceiverPacket_t;

typedef struct{
	uint8_t *pBuffer;
	uint32_t SizeOfBuffer;

	TransceiverCallback_t Callback;

	uint8_t ProcessStep;
	uint32_t Timestamp;

	uint16_t SequenceNo;
	uint16_t MaxSize;
	uint16_t  MaxSequenceNo;
	uint16_t CRCCheck;

	uint8_t IsBusy;
}oSerialReceiver_t;

typedef struct{
	uint8_t *pBuffer;

	uint32_t SizeOfBuffer;
	uint32_t SizeOfSended;
	uint16_t  MaxSequenceNo;
	uint16_t CRCCheck;
	uint16_t SequenceNo;

	TransceiverCallback_t Callback;

	uint8_t ProcessStep;
	uint32_t Timestamp;
	uint8_t TryCount;

	uint8_t IsTimeout;
	uint8_t IsBusy;
	uint8_t IsOkay;
	uint8_t IsError;
}oSerialTransmitter_t;

typedef struct{
	uint8_t Seqeunce;
	uint32_t Timestamp;
	uint32_t DataCount;
	uint16_t CRCCehck;
}oSerialModulation_t;

typedef struct{
	oSerialStack_t SerialStack;
	oSerialReceiver_t Receiver;
	oSerialTransmitter_t Transmitter;
	oSerialModulation_t Modulation;;

	uint8_t ReceiveAck;
	uint8_t IsWaitAck;
	uint32_t WaitAckTimer;

	uint8_t MessageNumber;

	uint32_t CheckTimer;
	uint32_t ReceiveTimestamp;
	uint8_t IsTimeout;
}oSerialTransceiver_t;

extern oResult_t oSerialTransceiver_SendData(oSerialTransceiver_t *pTransceiver, uint8_t *pData, uint32_t SizeOfData);
extern void oSerialTransceiver_Init(oSerialTransceiver_t *pTransceiver, uint8_t *pReceveBuffer, uint32_t SizeOfReceveBuffer, oSerialStack_t *pSerialStack, TransceiverCallback_t ReceiveCallback, TransceiverCallback_t TransmitCallback);
extern void oSerialTransceiver(oSerialTransceiver_t *pTransceiver, uint32_t Timeout, uint8_t Retry);

#endif /* INC_ONE_SERIALTRANSCEIVER_H_ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_SerialTransceiver.h)
*/
