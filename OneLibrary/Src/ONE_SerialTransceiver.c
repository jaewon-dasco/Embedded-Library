#include "ONE_Time.h"
#include "ONE_Common.h"
#include "ONE_Memory.h"
#include "ONE_Serial.h"
#include "ONE_SerialTransceiver.h"

#define SERIALTRANSCEIVER_STARTBYTE			0xFE
#define SERIALTRANSCEIVER_PACKET_MAXSIZE	50

oResult_t oSerialTransceiver_SendData(oSerialTransceiver_t *pTransceiver, uint8_t *pData, uint32_t SizeOfData)
{
	uint16_t MaxSequenceNo = 0;

	if(pTransceiver == NULL || pData == NULL || SizeOfData <= 0){
		return RESULT_NULL;
	}
	if(pTransceiver->Transmitter.IsBusy){
		return RESULT_BUSY;
	}
	
	MaxSequenceNo = (SizeOfData/SERIALTRCV_DATASPLIT_SIZE) + MATH_MIN(SizeOfData%SERIALTRCV_DATASPLIT_SIZE, 1);

	if(SizeOfData > SERIALTRCV_TOTALDATA_MAXSIZE){
		return RESULT_ERROR;
	}

	pTransceiver->Transmitter.pBuffer = pData;
	pTransceiver->Transmitter.SizeOfBuffer = SizeOfData;
	pTransceiver->Transmitter.CRCCheck = oMEM_CRC16(pData, SizeOfData, 0);
	pTransceiver->Transmitter.MaxSequenceNo = MaxSequenceNo;
	pTransceiver->Transmitter.ProcessStep = 0;
	pTransceiver->Transmitter.TryCount = 0;
	pTransceiver->Transmitter.IsBusy = 1;
	pTransceiver->Transmitter.IsError = 0;
	pTransceiver->Transmitter.IsOkay = 0;
	
	pTransceiver->Transmitter.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);

	return RESULT_OK;
}

uint8_t oSerialTransceiver_Decode(oSerialTransceiver_t *pTransceiver, uint8_t *pData, uint32_t SizeOfData)
{
	uint8_t Count = 0;
	uint16_t CRCCheck = 0;
	uint8_t Buffer[300] = {0,};

	if(pTransceiver->SerialStack.Read == NULL){
		return Count;
	}

	switch(pTransceiver->Modulation.Seqeunce)
	{
		default:
			pTransceiver->Modulation.Seqeunce = 0;
		case 0:
			pTransceiver->Modulation.Seqeunce++;
			pTransceiver->Modulation.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			break;
		case 1:
			if(pTransceiver->SerialStack.Read((uint8_t *)&Buffer, 1)){
				if(Buffer[0] == SERIALTRANSCEIVER_STARTBYTE){
					pTransceiver->Modulation.Seqeunce++;
					pTransceiver->Modulation.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				}
			}
			break;
		case 2:
			if(pTransceiver->SerialStack.Read((uint8_t *)&Buffer, 1)){
				if(Buffer[0] == (uint8_t)(~SERIALTRANSCEIVER_STARTBYTE)){
					pTransceiver->Modulation.Seqeunce++;
					pTransceiver->Modulation.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				}
				else{
					pTransceiver->Modulation.Seqeunce = 0;
				}
			}
			break;
		case 3:
			if(pTransceiver->SerialStack.Read((uint8_t *)&Buffer, 1)){
				if(Buffer[0] <= SizeOfData){
					pTransceiver->Modulation.DataCount = (uint32_t)Buffer[0];
					pTransceiver->Modulation.Seqeunce++;
					pTransceiver->Modulation.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				}
				else{
					pTransceiver->Modulation.Seqeunce = 0;
				}
			}
			break;
		case 4:
			if(pTransceiver->SerialStack.Read((uint8_t *)&Buffer, pTransceiver->Modulation.DataCount+2) == pTransceiver->Modulation.DataCount+2){

				memcpy(pData, &Buffer, pTransceiver->Modulation.DataCount);

				CRCCheck = oMEM_CRC16((uint8_t *)&pTransceiver->Modulation.DataCount, 1, CRCCheck);
				CRCCheck = oMEM_CRC16(pData, pTransceiver->Modulation.DataCount, CRCCheck);

				if(memcmp(&CRCCheck, &Buffer[pTransceiver->Modulation.DataCount], 2) == 0){
					Count = pTransceiver->Modulation.DataCount;
				}

				pTransceiver->Modulation.Seqeunce = 0;
				pTransceiver->Modulation.Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			break;
	}

	if(oTMR_Trigger(&pTransceiver->Modulation.Timestamp, 500, 0, TICKBASE_SYSTICK)){
		pTransceiver->Modulation.Seqeunce = 0;
	}

	return Count;
}

oResult_t oSerialTransceiver_Encode(oSerialTransceiver_t *pTransceiver, uint8_t *pData, uint32_t SizeOfData)
{
	uint16_t CRCCheck = 0;
	uint8_t Buffer[300] = {0,};

	if(pTransceiver->SerialStack.Write == NULL || pData == NULL || SizeOfData == 0 || SizeOfData > 255){
		return 0;
	}

	Buffer[0] = SERIALTRANSCEIVER_STARTBYTE;
	Buffer[1] = (uint8_t)(~SERIALTRANSCEIVER_STARTBYTE);
	Buffer[2] = (uint8_t)(SizeOfData);

	memcpy(&Buffer[3], pData, SizeOfData);

	CRCCheck = oMEM_CRC16(&Buffer[2], SizeOfData+1, 0);
	Buffer[SizeOfData+3] = (uint8_t)(CRCCheck);
	Buffer[SizeOfData+4] = (uint8_t)(CRCCheck>>8);

	if(pTransceiver->SerialStack.Write((uint8_t *)&Buffer, SizeOfData+5)){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

void oSerialTransceiver_Init(oSerialTransceiver_t *pTransceiver, uint8_t *pReceveBuffer, uint32_t SizeOfReceveBuffer, oSerialStack_t *pSerialStack, TransceiverCallback_t ReceiveCallback, TransceiverCallback_t TransmitCallback)
{
	if(pTransceiver != NULL && pSerialStack != NULL && pReceveBuffer != NULL && SizeOfReceveBuffer > 0){
		pTransceiver->SerialStack = *pSerialStack;

		pTransceiver->Receiver.pBuffer = pReceveBuffer;
		pTransceiver->Receiver.SizeOfBuffer = SizeOfReceveBuffer;
		pTransceiver->Receiver.ProcessStep = 0;
		pTransceiver->Receiver.IsBusy = 0;
		pTransceiver->Receiver.Callback = ReceiveCallback;

		pTransceiver->Transmitter.ProcessStep = 0;
		pTransceiver->Transmitter.IsBusy = 0;
		pTransceiver->Transmitter.Callback = TransmitCallback;
	}
}

oResult_t oSerialTransceiver_ReadPacket(oSerialTransceiver_t *pTransceiver, oSerialTransceiverPacket_t *pData)
{
	uint8_t DLC = 0;

	if((DLC = oSerialTransceiver_Decode(pTransceiver, (uint8_t *)&pData->Buffer, SERIALTRCV_PACKET_MAXSIZE)) > 2){
		pData->SizeOfPayload = DLC-2;
		return 1;
	}

	return 0;
}

oResult_t oSerialTransceiver_SendPacket(oSerialTransceiver_t *pTransceiver, oSerialTransceiverPacket_t *pData)
{
	return oSerialTransceiver_Encode(pTransceiver, (uint8_t *)&pData->Buffer, pData->SizeOfPayload+2);
}

oResult_t oSerialTransceiver_SendAck(oSerialTransceiver_t *pTransceiver, uint8_t Number, uint8_t Ack)
{
	oSerialTransceiverPacket_t Data;

	Data.Number = Number;
	Data.Type = SERIALTRCV_MESSAGE_TYPE_ACK;
	Data.SizeOfPayload = 1;
	Data.Payload[0] = Ack;

	return oSerialTransceiver_SendPacket(pTransceiver, &Data);
}

oResult_t oSerialTransceiver_SendPacketWaitAck(oSerialTransceiver_t *pTransceiver, oSerialTransceiverPacket_t *pData, uint32_t Timeout)
{
	oResult_t result = RESULT_RUN;

	if(!pTransceiver->IsWaitAck){
		pData->Number = MATH_MAX(++pTransceiver->MessageNumber, 1);

		if(oSerialTransceiver_SendPacket(pTransceiver, pData) == RESULT_OK){
			pTransceiver->WaitAckTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			pTransceiver->IsWaitAck = 1;
			pTransceiver->ReceiveAck = 0;
		}
	}
	else{
		if(oTMR_Elapsed(&pTransceiver->WaitAckTimer, Timeout, TICKBASE_SYSTICK)){
			result = RESULT_TIMEOUT;
			if(pTransceiver->SerialStack.Reset != NULL){
				pTransceiver->SerialStack.Reset();
			}
		}
		else if(pTransceiver->ReceiveAck == SERIALTRCV_ACK_OK){
			result = RESULT_OK;
		}
		else if(pTransceiver->ReceiveAck == SERIALTRCV_ACK_ERROR){
			result = RESULT_ERROR;
		}
	}

	if(result != RESULT_RUN){
		pTransceiver->IsWaitAck = 0;
		pTransceiver->ReceiveAck = 0;
	}

	return result;
}

void oSerialTransceiver_ReceiveProcess(oSerialTransceiver_t *pTransceiver)
{
	uint16_t SqNum = 0;
	oSerialTransceiverPacket_t Data;

	if(oSerialTransceiver_ReadPacket(pTransceiver, &Data)){
		pTransceiver->ReceiveTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);

		switch(Data.Type)
		{
			case SERIALTRCV_MESSAGE_TYPE_RXSTART:
				if(pTransceiver->Transmitter.pBuffer && pTransceiver->Transmitter.SizeOfBuffer){
					pTransceiver->Transmitter.SequenceNo = 1;

					memcpy(&Data.Payload[0], &pTransceiver->Transmitter.SizeOfBuffer, 2);
					memcpy(&Data.Payload[2], &pTransceiver->Transmitter.MaxSequenceNo, 2);
					memcpy(&Data.Payload[4], &pTransceiver->Transmitter.CRCCheck, 2);
					Data.Type = SERIALTRCV_MESSAGE_TYPE_TXSTART;
					Data.SizeOfPayload = 6;

					oSerialTransceiver_SendPacket(pTransceiver, &Data);
				}
				else{
					oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_ERROR);
				}
				break;
			case SERIALTRCV_MESSAGE_TYPE_RXDATA:
				memcpy(&SqNum, &Data.Payload[0], 2);

				if(SqNum > 0 && SqNum <= pTransceiver->Transmitter.MaxSequenceNo){
					pTransceiver->Transmitter.SequenceNo = SqNum;

					Data.Type = SERIALTRCV_MESSAGE_TYPE_TXDATA;

					pTransceiver->Transmitter.SizeOfSended = (pTransceiver->Transmitter.SequenceNo-1) * SERIALTRCV_DATASPLIT_SIZE;
					Data.SizeOfPayload = (uint8_t)MATH_MIN(pTransceiver->Transmitter.SizeOfBuffer-pTransceiver->Transmitter.SizeOfSended, SERIALTRCV_DATASPLIT_SIZE);

					memcpy(&Data.Payload[0], &pTransceiver->Transmitter.SequenceNo, 2);
					memcpy(&Data.Payload[2], pTransceiver->Transmitter.pBuffer+pTransceiver->Transmitter.SizeOfSended, Data.SizeOfPayload);

					Data.SizeOfPayload += 2;

					oSerialTransceiver_SendPacket(pTransceiver, &Data);
				}
				else{
					oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_ERROR);
				}
				break;
			case SERIALTRCV_MESSAGE_TYPE_TXSTART:
				if(Data.SizeOfPayload == 6){
					pTransceiver->Receiver.SequenceNo = 1;
					memcpy(&pTransceiver->Receiver.MaxSize, 		&Data.Payload[0], 2);
					memcpy(&pTransceiver->Receiver.MaxSequenceNo, 	&Data.Payload[2], 2);
					memcpy(&pTransceiver->Receiver.CRCCheck, 		&Data.Payload[4], 2);
				}

				if(Data.SizeOfPayload == 6 && pTransceiver->Receiver.MaxSize && pTransceiver->Receiver.MaxSequenceNo){
					pTransceiver->Receiver.IsBusy = 1;

					oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_OK);
				}
				else{
					oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_ERROR);
				}
				break;
			case SERIALTRCV_MESSAGE_TYPE_TXDATA:
				memcpy(&SqNum, &Data.Payload[0], 2);

				if(SqNum > 0 && SqNum <= pTransceiver->Receiver.MaxSequenceNo && Data.SizeOfPayload > 2){
					uint32_t offset = (SqNum-1) * SERIALTRCV_DATASPLIT_SIZE;
					uint8_t copyLen = Data.SizeOfPayload - 2;
					pTransceiver->Receiver.SequenceNo = SqNum;
					if(offset + copyLen <= pTransceiver->Receiver.SizeOfBuffer){
						memcpy((uint8_t *)(pTransceiver->Receiver.pBuffer + offset), &Data.Payload[2], copyLen);
					}

					if(pTransceiver->Receiver.SequenceNo == pTransceiver->Receiver.MaxSequenceNo){
						if(pTransceiver->Receiver.CRCCheck == oMEM_CRC16(pTransceiver->Receiver.pBuffer, pTransceiver->Receiver.MaxSize, 0)){
							if(pTransceiver->Receiver.Callback){
								pTransceiver->Receiver.Callback(RESULT_OK, pTransceiver->Receiver.pBuffer, pTransceiver->Receiver.MaxSize);
							}
							oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_OK);
						}
						else{
							oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_ERROR);
						}
					}
					else{
						oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_OK);
					}
				}
				else{
					oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_ERROR);
				}
				break;
			case SERIALTRCV_MESSAGE_TYPE_ACK:
				if(pTransceiver->IsWaitAck && pTransceiver->MessageNumber == Data.Number){
					pTransceiver->ReceiveAck = Data.Payload[0];
				}
				break;
			case SERIALTRCV_MESSAGE_TYPE_CHCK:
				oSerialTransceiver_SendAck(pTransceiver, Data.Number, SERIALTRCV_ACK_OK);
				break;
		}
	}
}

oResult_t oSerialTransceiver_TransmitProcess(oSerialTransceiver_t *pTransceiver, uint32_t Timeout, uint8_t Retry)
{
	oSerialTransceiverPacket_t Data;
	oResult_t result = RESULT_RUN;

	switch(pTransceiver->Transmitter.ProcessStep)
	{
		default:
			pTransceiver->Transmitter.ProcessStep = 0;
		case 0:
			if(pTransceiver->Transmitter.IsBusy && pTransceiver->Transmitter.SizeOfBuffer && pTransceiver->Transmitter.pBuffer){
				pTransceiver->Transmitter.ProcessStep++;
			}
			else{
				result = RESULT_NULL;
				break;
			}
		case 1:
			if(!pTransceiver->IsWaitAck){
				Data.Type = SERIALTRCV_MESSAGE_TYPE_TXSTART;
				memcpy(&Data.Payload[0], &pTransceiver->Transmitter.SizeOfBuffer,	2);
				memcpy(&Data.Payload[2], &pTransceiver->Transmitter.MaxSequenceNo,	2);
				memcpy(&Data.Payload[4], &pTransceiver->Transmitter.CRCCheck,		2);
				Data.SizeOfPayload = 6;
			}

			switch(oSerialTransceiver_SendPacketWaitAck(pTransceiver, &Data, Timeout))
			{
				default:
					break;
				case RESULT_OK:
					pTransceiver->Transmitter.SizeOfSended = 0;
					pTransceiver->Transmitter.TryCount = 0;
					pTransceiver->Transmitter.SequenceNo = 1;
					pTransceiver->Transmitter.ProcessStep++;
					break;
				case RESULT_TIMEOUT:
				case RESULT_ERROR:
					if(++pTransceiver->Transmitter.TryCount > Retry){
						result = RESULT_ERROR;
					}
					break;
			}
			break;
		case 2:
			if(!pTransceiver->IsWaitAck){
				Data.Type = SERIALTRCV_MESSAGE_TYPE_TXDATA;
				Data.SizeOfPayload = (uint8_t)MATH_MIN(pTransceiver->Transmitter.SizeOfBuffer-pTransceiver->Transmitter.SizeOfSended, SERIALTRCV_DATASPLIT_SIZE);

				memcpy(&Data.Payload[0], &pTransceiver->Transmitter.SequenceNo, 2);
				memcpy(&Data.Payload[2], pTransceiver->Transmitter.pBuffer+pTransceiver->Transmitter.SizeOfSended, Data.SizeOfPayload);

				Data.SizeOfPayload += 2;
			}

			switch(oSerialTransceiver_SendPacketWaitAck(pTransceiver, &Data, Timeout))
			{
				default:
					break;
				case RESULT_OK:
					pTransceiver->Transmitter.TryCount = 0;

					if(pTransceiver->Transmitter.SequenceNo < pTransceiver->Transmitter.MaxSequenceNo){
						pTransceiver->Transmitter.SizeOfSended += SERIALTRCV_DATASPLIT_SIZE;
						pTransceiver->Transmitter.SequenceNo++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				case RESULT_TIMEOUT:
				case RESULT_ERROR:
					if(++pTransceiver->Transmitter.TryCount > Retry){
						result = RESULT_ERROR;
					}
					break;
			}
			break;

	}

	if(result != RESULT_RUN){
		pTransceiver->Transmitter.ProcessStep = 0;
		pTransceiver->Transmitter.IsBusy = 0;

		if(result != RESULT_NULL){
			pTransceiver->Transmitter.IsError = result == RESULT_ERROR ? 1 : 0;
			pTransceiver->Transmitter.IsTimeout = result == RESULT_TIMEOUT ? 1 : 0;
			pTransceiver->Transmitter.IsOkay = result == RESULT_OK ? 1 : 0;
		}
	}

	return result;
}

void oSerialTransceiver(oSerialTransceiver_t *pTransceiver, uint32_t Timeout, uint8_t Retry)
{
	oSerialTransceiverPacket_t Data;

	oSerialTransceiver_ReceiveProcess(pTransceiver);
	oSerialTransceiver_TransmitProcess(pTransceiver, Timeout, Retry);

	if(oTMR_Elapsed(&pTransceiver->ReceiveTimestamp, Timeout, TICKBASE_SYSTICK)){
		pTransceiver->IsTimeout = oTMR_Elapsed(&pTransceiver->ReceiveTimestamp, Timeout*Retry, TICKBASE_SYSTICK);

		if(oTMR_Trigger(&pTransceiver->CheckTimer, Timeout, 1, TICKBASE_SYSTICK)){
			Data.Type = SERIALTRCV_MESSAGE_TYPE_CHCK;
			Data.SizeOfPayload = 0;
			oSerialTransceiver_SendPacket(pTransceiver, &Data);
		}
	}
	else{
		pTransceiver->IsTimeout = 0;
	}
}
