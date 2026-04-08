/*
 * UART-3 GPIO PB10/PB11 핀 스웹함. (2024-08-27)
 */

#include "ONE_Serial.h"
#include "ONE_SerialTransceiver.h"
#include <ONE_Common.h>
#include <LoRa_NML35.h>

#define LORA_TXBUFFER_COUNT			(LoRaDevice.Buffer.TxLastIndex < LoRaDevice.Buffer.TxFirstIndex ? (LORA_UART_TX_BUFF_MAXSIZE-LoRaDevice.Buffer.TxFirstIndex+LoRaDevice.Buffer.TxLastIndex) : (LoRaDevice.Buffer.TxLastIndex-LoRaDevice.Buffer.TxFirstIndex))
#define LORA_RXBUFFER_COUNT			(LoRaDevice.Buffer.RxLastIndex < LoRaDevice.Buffer.RxFirstIndex ? (LORA_UART_RX_BUFF_MAXSIZE-LoRaDevice.Buffer.RxFirstIndex+LoRaDevice.Buffer.RxLastIndex) : (LoRaDevice.Buffer.RxLastIndex-LoRaDevice.Buffer.RxFirstIndex))
#define LORA_TXBUFFER_SETDATA(x)	LoRaDevice.Buffer.TxBuffer[LoRaDevice.Buffer.TxLastIndex] = (uint8_t)(x);\
									LoRaDevice.Buffer.TxLastIndex = (LoRaDevice.Buffer.TxLastIndex+1) % LORA_UART_TX_BUFF_MAXSIZE;

//Public
oSerialTransceiver_t Transceiver;
oSerialStack_t SeialStack = {LoRa_SerialWrite, LoRa_SerialRead, LoRa_SerialReset};
uint32_t LoRaTransmitTimer = 0;
LoRaDevice_t LoRaDevice;

uint32_t LoRa_SerialRead(uint8_t *pData, uint32_t SizeOfData)
{
	uint32_t readcnt = 0;

	LoRaDevice.Buffer.RxLastIndex = (LORA_UART_RX_BUFF_MAXSIZE - LoRaDevice.pUART->hdmarx->Instance->CNDTR) % LORA_UART_RX_BUFF_MAXSIZE;

	if(pData == NULL || SizeOfData <= 0 || LORA_RXBUFFER_COUNT < SizeOfData){
		return 0;
	}

	while(readcnt < SizeOfData)
	{
		*pData = LoRaDevice.Buffer.RxBuffer[LoRaDevice.Buffer.RxFirstIndex];

		LoRaDevice.Buffer.RxFirstIndex = (LoRaDevice.Buffer.RxFirstIndex+1) % LORA_UART_RX_BUFF_MAXSIZE;
		pData++;
		readcnt++;
	}

	return readcnt;
}

uint32_t LoRa_SerialWrite(uint8_t *pData, uint32_t SizeOfData)
{
	uint32_t cnt = 0;


	if(LORA_TXBUFFER_COUNT + SizeOfData > LORA_UART_TX_BUFF_MAXSIZE){
		return 0;
	}

	while(cnt < SizeOfData){
		LORA_TXBUFFER_SETDATA(*pData);
		pData++;
		cnt++;
	}
	return cnt;

	/*
	if(HAL_UART_Transmit(LoRaDevice.pUART, pData, SizeOfData, 100) == HAL_OK){
		return SizeOfData;
	}
	else{
		return 0;
	}
	*/
}

void LoRa_SerialReset()
{
	static uint32_t ResetTimer = 0;

	if(LoRaDevice.Status.IsOpen == 0){
		return;
	}
	else if(LoRaDevice.pUART->RxState == HAL_UART_STATE_READY){
		if(oTMR_Trigger(&ResetTimer, 500, 1)){
			if(HAL_UART_Receive_DMA(LoRaDevice.pUART, (uint8_t *)&LoRaDevice.Buffer.RxBuffer, LORA_UART_RX_BUFF_MAXSIZE) != HAL_OK){
				return;
			}
		}
	}
	else{
		oTMR_ResetNow(&ResetTimer);
	}
}

static void LoRa_SerialReceiveCallback(oResult_t Result, uint8_t*pData, uint32_t SizeOfData)
{
	if(Result == RESULT_OK){
		LoRaDevice.Status.UARTReceiveCount = MATH_MAX(LoRaDevice.Status.UARTReceiveCount+1, 1);
		oTMR_ResetNow(&LoRaDevice.Status.UARTReceiveTimestamp);

		switch(*pData)
		{
			case PID_HARTBEAT:
				if(SizeOfData == 11){
					memcpy(&LoRaDevice.Status.Heartbeat.SequenceNumber, pData+1, 1);
					memcpy(&LoRaDevice.Status.Heartbeat.RadioReceiveCount, pData+2, 1);
					memcpy(&LoRaDevice.Status.Heartbeat.Sign, pData+3, 1);
					memcpy(&LoRaDevice.Status.Radio, pData+4, 2);
					memcpy(&LoRaDevice.Status.RSSI, pData+6, 2);
					memcpy(&LoRaDevice.Status.Transporter, pData+8, 2);
					memcpy(&LoRaDevice.Status.Beacon, pData+10, 1);

					oTMR_ResetNow(&LoRaDevice.Status.Heartbeat.Timestamp);
				}
				break;
			case PID_RECEIVED_RADIO_DATA:
				if(LoRaDevice.ReceiveRadioDataCallback && SizeOfData >= 5){
					uint32_t srcAddr;
					memcpy(&srcAddr, pData+1, sizeof(srcAddr));
					LoRaDevice.ReceiveRadioDataCallback(srcAddr, SizeOfData-5, pData+5);
				}
				break;
		}
	}
}


LoRaResult_t LoRa_TransmitUART()
{
	static uint8_t LoRaTransmitStep = 0;
	static uint32_t LoRaBufferTxTmer = 0;
	uint32_t Count = 0;
	LoRaResult_t result = LORA_RESULT_RUN;

	if(LoRaDevice.pUART->gState != HAL_UART_STATE_READY){
		return LORA_RESULT_BUSY;
	}

	if(LORA_TXBUFFER_COUNT <= 0){
		LoRaTransmitStep = 0;
		return LORA_RESULT_NULL;
	}

	switch(LoRaTransmitStep)
	{
		case 0:
			if(oTMR_Trigger(&LoRaBufferTxTmer, 100, 1)){
				LoRaTransmitStep++;
			}
			break;
		case 1:
			if(LoRaDevice.Buffer.TxFirstIndex + LORA_TXBUFFER_COUNT > LORA_UART_TX_BUFF_MAXSIZE){
				Count = LORA_UART_TX_BUFF_MAXSIZE - LoRaDevice.Buffer.TxFirstIndex;
			}
			else{
				Count = LORA_TXBUFFER_COUNT;
			}

			if(HAL_UART_Transmit_DMA(LoRaDevice.pUART, (uint8_t *)&LoRaDevice.Buffer.TxBuffer[LoRaDevice.Buffer.TxFirstIndex], Count) == HAL_OK){
				LoRaTransmitStep++;
				LoRaDevice.Buffer.TxFirstIndex = (LoRaDevice.Buffer.TxFirstIndex+Count) %  LORA_UART_TX_BUFF_MAXSIZE;
			}
			break;
		case 2:
			if(LoRaDevice.pUART->gState == HAL_UART_STATE_READY || LoRaDevice.pUART->hdmatx->Instance->CNDTR == 0){
				if(LORA_TXBUFFER_COUNT > 0){
					LoRaTransmitStep = 0;
				}
				else{
					result = LORA_RESULT_OK;
					LoRaDevice.Buffer.TxFirstIndex = 0;
					LoRaDevice.Buffer.TxLastIndex = 0;
				}
			}
			else if(oTMR_Elapsed(&LoRaBufferTxTmer, 3000)){
				result = LORA_RESULT_SERIAL_TIMEOUT;
			}
			break;

	}

	if(result != LORA_RESULT_RUN){
		LoRaTransmitStep = 0;
	}

	return result;
}

LoRaResult_t LoRa_ParameterWrite(uint8_t Index, uint8_t *pData, uint16_t DLC)
{
	static uint8_t LoRaWriteParameterStep = 0;

	LoRaResult_t result = LORA_RESULT_RUN;

	if(pData == NULL || DLC <= 0 || DLC + 2 > LORA_UART_DATA_MAX_SIZE){
		return LORA_RESULT_ERROR;
	}

	switch(LoRaWriteParameterStep){
		case 0:
			if(!Transceiver.Transmitter.IsBusy){
				LoRaWriteParameterStep++;
				LoRaDevice.Status.IsUARTBusy = 1;
			}
			break;
		case 1:
			LoRaDevice.Buffer.TransmitData[0] = PID_PARAMETER_WRITE;
			LoRaDevice.Buffer.TransmitData[1] = Index;
			memcpy(&LoRaDevice.Buffer.TransmitData[2], pData, DLC);

			if(oSerialTransceiver_SendData(&Transceiver, (uint8_t *)&LoRaDevice.Buffer.TransmitData, DLC+2) == RESULT_OK){
				oTMR_ResetNow(&LoRaTransmitTimer);
				LoRaWriteParameterStep++;
			}
			else{
				result = LORA_RESULT_ERROR;
			}
			break;
		case 2:
			if(!Transceiver.Transmitter.IsBusy){
				if(Transceiver.Transmitter.IsOkay){
					result = LORA_RESULT_OK;
				}
				else if(Transceiver.Transmitter.IsError){
					result = LORA_RESULT_ERROR;
				}
			}

			if(oTMR_Elapsed(&LoRaTransmitTimer, 2000) > 0){
				result = LORA_RESULT_ABORT;
				LoRaDevice.Status.UARTFaultCount++;
			}
			break;
	}

	if(result != LORA_RESULT_RUN){
		LoRaDevice.Status.IsUARTBusy = 0;
		LoRaWriteParameterStep = 0;
	}

	LoRa_TransmitUART();

	return result;
}

LoRaResult_t LoRa_SentRadioData(uint32_t DestAddress, uint32_t DLC, uint8_t *pData)
{
	static uint8_t LoRaRadioSerialTryCount = 0;
	static uint8_t LoRaRadioSentStep = 0;

	LoRaResult_t result = LORA_RESULT_RUN;

	if(pData == NULL || DLC == 0 || DLC + 5 > LORA_UART_DATA_MAX_SIZE){
		LoRaRadioSentStep = 0;
		return LORA_RESULT_ERROR;
	}
	else if(LoRaDevice.pUART == NULL || !LoRaDevice.Status.IsOpen || !LoRaDevice.Status.IsWakeup || !LoRaDevice.Status.IsParameterSync){
		LoRaRadioSentStep = 0;
		return LORA_RESULT_ERROR;
	}
	else if(LORA_BROADCAST_ADDRESS == DestAddress && DLC > LORA_RADIO_BROADCAST_MAXSIZE){
		LoRaRadioSentStep = 0;
		return LORA_RESULT_ERROR;
	}

	switch(LoRaRadioSentStep)
	{
		case 0:
			if(!Transceiver.Transmitter.IsBusy){
				LoRaDevice.Status.IsUARTBusy = 1;
				LoRaRadioSentStep++;
				LoRaRadioSerialTryCount = 0;
			}
			break;
		case 1:
			LoRaDevice.Buffer.TransmitData[0] = PID_TRANSMIT_RADIO_DATA;
			memcpy(&LoRaDevice.Buffer.TransmitData[1], &DestAddress, 4);
			memcpy(&LoRaDevice.Buffer.TransmitData[5], pData, DLC);

			if(oSerialTransceiver_SendData(&Transceiver, (uint8_t *)&LoRaDevice.Buffer.TransmitData, DLC+5) == RESULT_OK){
				oTMR_ResetNow(&LoRaTransmitTimer);
				LoRaRadioSerialTryCount++;
				LoRaRadioSentStep++;
			}
			else{
				result = LORA_RESULT_ERROR;
			}
			break;
		case 2:
			if(!Transceiver.Transmitter.IsBusy){
				if(Transceiver.Transmitter.IsOkay){
					oTMR_ResetNow(&LoRaTransmitTimer);
					LoRaRadioSentStep++;
				}
			}

			if(oTMR_Elapsed(&LoRaTransmitTimer, 3000)){
				if(LoRaRadioSerialTryCount >= 3){
					result = LORA_RESULT_SERIAL_TIMEOUT;
				}
				else{
					LoRaRadioSentStep--;
				}
			}
			break;
		case 3:
			if(LoRaDevice.Status.Transporter.IsSuccess){
				result = LORA_RESULT_OK;
			}
			else if(LoRaDevice.Status.Transporter.IsFault){
				result = LORA_RESULT_RADIO_FAIL;
			}
			else if(oTMR_Elapsed(&LoRaTransmitTimer, 10000) && !LoRaDevice.Status.Transporter.IsTransmitMode){
				result = LORA_RESULT_RADIO_FAIL;
			}
			break;
	}

	if(result != LORA_RESULT_RUN){
		LoRaRadioSentStep = 0;
	}

	LoRa_TransmitUART();

	return result;
}

LoRaResult_t LoRa_SetOperatingMode(LoRaOperatingMode_t Mode)
{
	LoRaResult_t result = LORA_RESULT_RUN;

	switch(result = LoRa_ParameterWrite(PARAMETER_OPERATING_MODE, &Mode, 1))
	{
		default:
			break;
		case LORA_RESULT_OK:
			LoRaDevice.Parameter.OperatingMode = Mode;
			break;
	}

	return result;
}

LoRaResult_t LoRa_SetParameter()
{
	static uint8_t LoraParameterStep = 0;
	static uint32_t LoraParameterTimer = 0;
	LoRaResult_t result = LORA_RESULT_RUN;

	if(!oTMR_Trigger(&LoraParameterTimer, 5, 1)){
		return result;
	}

	if(LoRaDevice.Status.IsWakeup){
		switch (LoraParameterStep)
		{
			case 0:
				if(LoRaDevice.Status.Heartbeat.Timestamp > 0){
					LoRaDevice.Signature = oMEM_CRC8((uint8_t *)&LoRaDevice.Parameter, sizeof(LoRaDevice.Parameter), 0);

					if(LoRaDevice.Signature == LoRaDevice.Status.Heartbeat.Sign){
						LoRaDevice.Status.IsParameterSync = 1;
						result = LORA_RESULT_OK;
					}
					else{
						LoraParameterStep++;
					}
				}
				break;
			case 1:
				switch(LoRa_ParameterWrite(PARAMETER_NETWORK, (uint8_t *)&LoRaDevice.Parameter.Network, 1)){
					case LORA_RESULT_OK:
						LoraParameterStep++;
						break;
					case LORA_RESULT_ERROR:
					case LORA_RESULT_ABORT:
						break;
					default:
						break;
				}
				break;
			case 2: //Write LoRa short address
				switch(LoRa_ParameterWrite(PARAMETER_ADDRESS, (uint8_t *)&LoRaDevice.Parameter.Address, 4)){
					case LORA_RESULT_OK:
						LoraParameterStep++;
						break;
					case LORA_RESULT_ERROR:
					case LORA_RESULT_ABORT:
						break;
					default:
						break;
				}
				break;
			case 3: //Write LoRa server address
				switch(LoRa_ParameterWrite(PARAMETER_SERVERADDRESS, (uint8_t *)&LoRaDevice.Parameter.ServerAddress, 4)){
					case LORA_RESULT_OK:
						LoraParameterStep++;
						break;
					case LORA_RESULT_ERROR:
					case LORA_RESULT_ABORT:
						break;
					default:
						break;
				}
				break;
			case 4: //Write LoRa operating mode
				if(LoRa_SetOperatingMode(LoRaDevice.Parameter.OperatingMode) == LORA_RESULT_OK){
					LoraParameterStep++;
				}
				break;
			case 5: //Write LoRa operating mode
				switch(LoRa_ParameterWrite(PARAMETER_SAVE, (uint8_t *)"S", 1)){
					case LORA_RESULT_OK:
						LoraParameterStep++;
						break;
					case LORA_RESULT_ERROR:
					case LORA_RESULT_ABORT:
						break;
					default:
						break;
				}
				break;
			case 6:
				if(LoRaDevice.Signature == LoRaDevice.Status.Heartbeat.Sign){
					LoraParameterStep++;
				}
				else{
					LoraParameterStep = 0;
				}
				break;
			default:
				LoRaDevice.Status.IsParameterSync = 1;
				result = LORA_RESULT_OK;
				break;
		}
	}
	else{
		LoraParameterStep = 0;
	}

	if(result != LORA_RESULT_RUN){
		LoraParameterStep = 0;
	}

	return result;
}

LoRaResult_t LoRa_Open(UART_HandleTypeDef *pUART, LoRaParameter_t *pParameter, RadioReceiveHandler_t Callback)
{
	static uint8_t LoraInitStep = 0;
	LoRaResult_t result = LORA_RESULT_RUN;

	if(pUART == NULL){
		return LORA_RESULT_ERROR;
	}

	switch (LoraInitStep) {
		case 0:
			LoRaDevice.Buffer.TxFirstIndex = 0;
			LoRaDevice.Buffer.TxLastIndex = 0;
			LoRaDevice.Buffer.RxFirstIndex = 0;
			LoRaDevice.Buffer.RxLastIndex = 0;
			LoRaDevice.Status.IsOpen = 0;

			HAL_UART_DMAStop(pUART);
			HAL_UART_DeInit(pUART);
			LoraInitStep++;
			break;
		case 1:
			if(HAL_UART_Init(pUART) == HAL_OK){
				LoraInitStep++;
			}
			else{
				result = LORA_RESULT_ERROR;
			}
			break;
		case 2:
			if (HAL_UART_Receive_DMA(pUART, (uint8_t *)&LoRaDevice.Buffer.RxBuffer, LORA_UART_RX_BUFF_MAXSIZE) == HAL_OK){
				LoraInitStep++;
				LoRaDevice.pUART = pUART;
			}
			else{
				result = LORA_RESULT_ERROR;
			}
			break;
		case 3:
			oSerialTransceiver_Init(&Transceiver, (uint8_t *)&LoRaDevice.Buffer.ReceivedData, LORA_UART_DATA_MAX_SIZE, &SeialStack, LoRa_SerialReceiveCallback, NULL);

			if(pParameter != NULL){
				LoRaDevice.Parameter = *pParameter;
				if(LoRaDevice.Parameter.OperatingMode == LoRaOperMode_Server && LoRaDevice.Parameter.ServerAddress != 0){
					LoRaDevice.Parameter.Address = LoRaDevice.Parameter.ServerAddress;
				}
			}

			if(Callback != NULL){
				LoRaDevice.ReceiveRadioDataCallback = Callback;
			}

			LoRaDevice.Status.IsOpen = 1;
			LoraInitStep++;
			break;
		default:
			result = LORA_RESULT_OK;
			break;
	}

	if(result == LORA_RESULT_OK){
		LoraInitStep = 0;
	}

	return result;
}

LoRaResult_t LoRa_Close()
{
	if(LoRaDevice.pUART != NULL){
		HAL_UART_DMAStop(LoRaDevice.pUART);
		HAL_UART_DeInit(LoRaDevice.pUART);
	}

	LoRaDevice.Status.IsOpen = 0;
	memset(&LoRaDevice.Status, 0, sizeof(LoRaDevice.Status));

	return LORA_RESULT_OK;
}

void LoRa_UpdateStatus(void)
{
	static uint8_t PastHeartbeatSeqNumber  = 0;
	static uint32_t StateUpdateTimer  = 0;
	static uint32_t HeartbeatFrozenTimer  = 0;

	if(!oTMR_Trigger(&StateUpdateTimer, 100, 1)){
		return;
	}

	if(LoRaDevice.Status.Heartbeat.SequenceNumber != PastHeartbeatSeqNumber){
		PastHeartbeatSeqNumber = LoRaDevice.Status.Heartbeat.SequenceNumber;
		oTMR_ResetNow(&HeartbeatFrozenTimer);
	}

	LoRaDevice.Status.Heartbeat.IsFrozen = oTMR_Elapsed(&HeartbeatFrozenTimer, 5000);

	if(oTMR_Elapsed(&LoRaDevice.Status.Heartbeat.Timestamp, 6000)){
		if(!LoRaDevice.Status.Heartbeat.IsTimeout){
			memset(&LoRaDevice.Status.Radio, 0, sizeof(LoRaDevice.Status.Radio));
			memset(&LoRaDevice.Status.RSSI, 0, sizeof(LoRaDevice.Status.RSSI));
		}

		LoRaDevice.Status.Heartbeat.IsTimeout = 1;
	}
	else{
		LoRaDevice.Status.Heartbeat.IsTimeout = 0;
	}

	if(oTMR_Elapsed(&LoRaDevice.Status.UARTReceiveTimestamp, 6000)){
		if(LoRaDevice.Status.IsWakeup){
			memset(&LoRaDevice.Status.Transporter, 0, sizeof(LoRaDevice.Status.Transporter));
		}

		LoRaDevice.Status.IsWakeup = 0;
		LoRaDevice.Status.UARTReceiveCount = 0;
	}
	else{
		LoRaDevice.Status.IsWakeup = LoRaDevice.Status.UARTReceiveCount && !LoRaDevice.Status.Heartbeat.IsFrozen;
	}

	LoRaDevice.Signature = oMEM_CRC8((uint8_t *)&LoRaDevice.Parameter, sizeof(LoRaDevice.Parameter), 0);
	LoRaDevice.Status.IsUartFault = LoRaDevice.Status.UARTFaultCount > 5;
	LoRaDevice.Status.IsParameterSync = LoRaDevice.Status.IsParameterSync && LoRaDevice.Status.IsWakeup && LoRaDevice.Signature == LoRaDevice.Status.Heartbeat.Sign;
	LoRaDevice.Status.IsUARTBusy = LoRaDevice.Status.IsUARTBusy && LoRaDevice.Status.IsWakeup;
	LoRaDevice.Status.IsOkay = LoRaDevice.Status.IsWakeup && LoRaDevice.Status.Radio.IsFalut == 0 && LoRaDevice.Status.Radio.NetworkOnline && !LoRaDevice.Status.Heartbeat.IsFrozen;
}

void LoRaRun()
{
	static uint8_t LoRa_UARTReset = 0;

	if(LoRaDevice.pUART == NULL){
		return;
	}

	if(LoRa_UARTReset){
		if(LoRa_Open(LoRaDevice.pUART, NULL, NULL) != LORA_RESULT_RUN){
			LoRa_UARTReset = 0;
		}
	}
	else if(LoRaDevice.Status.IsOpen){
		oSerialTransceiver(&Transceiver, 1000, 3);

		if(LoRaDevice.pUART->ErrorCode != 0){
			LoRa_UARTReset = 1;
		}
		else if(!LoRaDevice.Status.IsParameterSync){
			LoRa_SetParameter();
		}

		LoRa_UpdateStatus();
		LoRa_TransmitUART();
	}
}
