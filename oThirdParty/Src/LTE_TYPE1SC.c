/*
 * LTE_TYPE1SC.c
 *
 *  Version: 0.1 (2026-06-29)
 */
﻿#include "Network.h"
#include "ONE_Serial.h"
#include "ONE_ATCommend.h"
#include "ONE_Memory.h"
#include "ONE_Time.h"
#include "ONE_Common.h"
#include "LTE_TYPE1SC.h"
#include "Mi_Serial.h"

/* Define - GPIO */
#define GPIO_LTE_RESET 			DO_LTE_RESET_GPIO_Port, DO_LTE_RESET_Pin
#define LTE_RETRY_MAXCOUNT		2

LTE_Device_t LTEDevice;
NetworkConnection_t *LTE_pNetConnection;
LTE_SubscribeCallback_t LTE_SubscribeReceiveCallback = NULL;

void LTE_StatusMessage(char *Message)
{
	memset(&LTEDevice.Status.Message, 0, sizeof(LTEDevice.Status.Message));

	if(Message != NULL && strlen(Message) > 0){
		snprintf(LTEDevice.Status.Message, sizeof(LTEDevice.Status.Message), "%s", Message);

		MiSerial_DebugLog("LTE | %s", Message);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LTE_MQTTSubscribeMessage(char *Topic, uint8_t *pData, uint32_t SizeOfData, LTE_SubscribeCallback_t Callback)
{
	uint8_t *pBuffer = NULL;
	uint32_t Length = 0;
	uint32_t i;

	if(strlen(Topic) <= 0){
		return;
	}

	if(SizeOfData > 0){
		Length = SizeOfData;
		pBuffer = malloc(Length);
		if(pBuffer == NULL){
			return;
		}
		memset(pBuffer, 0, Length);

		for(i=0; i<Length; i+=2){
			*(pBuffer +(i/2)) = HEX_PAIR_TO_BYTE(pData[i], pData[i+1]);
		}
	}

	Callback(Topic, pBuffer, Length);

	if(pBuffer != NULL){
		free(pBuffer);
	}
}

static void LTE_ATReceiveDataCallback(char *pData, uint32_t Length)
{
	uint8_t i=0;
	char *pBuffer = NULL;
	char ReceiveBuffer[1000] = {0,};
	char Filter[100] ={0,};
	char Topic[255] = {0,};
	uint32_t PayloadLength = 0;
	char PayloadBuffer[1000] = {0,};

	Length = limit(Length, 0, ArrayLen(ReceiveBuffer));
	oAT_ReceiveCopyTo(LTEDevice.pAT, (uint8_t *)&ReceiveBuffer, Length);

	if(strlen(ReceiveBuffer) <= 0){
		return;
	}

	snprintf(Filter, sizeof(Filter), "+CEREG: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);

		if ((pBuffer = strstr(pBuffer, ",")) != NULL) {
			pBuffer++;

			// ready to connect. CEREG equals to 1 (home) nor 5 (roaming)
			if (*pBuffer == '1' || *pBuffer == '5'){
				LTEDevice.Status.IsPHYOkay = 1;
			}
			else if (*pBuffer == '3') {
				LTEDevice.Status.IsPHYOkay = 0;
				LTEDevice.Status.IsConnected = 0;
			}
		}

		goto EXIT;
	}

	snprintf(Filter, sizeof(Filter), "%%CMATT: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);

		// 0=detach, 1=attach
		if (*pBuffer == '0') {
			LTEDevice.Status.IsPHYOkay = 0;
			LTEDevice.Status.IsConnected = 0;
		}

		goto EXIT;
	}

	snprintf(Filter, sizeof(Filter), "USIM: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);
		LTEDevice.Status.IsUSIMOkay = strncmp(pBuffer, "REAL USIM", strlen("REAL USIM")) == 0 ? 1 : 0;
		goto EXIT;
	}

	snprintf(Filter, sizeof(Filter), "%%CSQ: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);

		if ((pBuffer = strtok(pBuffer, ",")) != NULL) {
			LTEDevice.Status.SignalQuality.Power = 0;
			LTEDevice.Status.SignalQuality.ErrorRate = 0;
			LTEDevice.Status.SignalQuality.Quality = 0;

			i = atoi(pBuffer);

			if(i >= 0 && i < 99){
				i = limit(i, 0, 31);

				LTEDevice.Status.SignalQuality.Power = (uint16_t)(-113 + (i*2));
			}


			if((pBuffer = strtok(NULL, ",")) != NULL){
				i = atoi(pBuffer);

				if(i >= 0 && i < 99){
					LTEDevice.Status.SignalQuality.ErrorRate = (uint8_t)i;
				}
			}

			if((pBuffer = strtok(NULL, ",")) != NULL){
				i = atoi(pBuffer);

				if(i >= 0 && i < 99){
					LTEDevice.Status.SignalQuality.Quality = (uint8_t)i;
				}
			}
		}

		goto EXIT;
	}

	if(LTEDevice.Status.IsConnected){
		snprintf(Filter, sizeof(Filter), "%%MQTTEVU:\"PUBRCV\",%d,", LTEDevice.SocketNumber);
		if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
			pBuffer += strlen(Filter);
			pBuffer = strtok(pBuffer, ",");

			while(i <= 3 && pBuffer != NULL)
			{
				switch(i){
					case 0://message_id
						break;
					case 1:
						strncpy(Topic, pBuffer, sizeof(Topic) - 1);
					Topic[sizeof(Topic) - 1] = '\0';
						break;
					case 2:
						PayloadLength = atoi(pBuffer);
						break;
					case 3:
						if(PayloadLength > 0 && PayloadLength <= sizeof(PayloadBuffer)){
							memcpy(&PayloadBuffer, pBuffer, PayloadLength);
						}
						break;
				}

				pBuffer = strtok(NULL, ",");
				i++;
			}

			if(LTE_SubscribeReceiveCallback && strlen(Topic) > 0){
				LTE_MQTTSubscribeMessage(Topic, (uint8_t *)&PayloadBuffer, PayloadLength, LTE_SubscribeReceiveCallback);
			}

			goto EXIT;
		}
	}

	snprintf(Filter, sizeof(Filter), "+CGCONTRDP: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);

		if(LTE_StringToNetworkProperties(&LTEDevice.NetProperties, pBuffer) == RESULT_OK){
			LTEDevice.Status.IsConnected &= LTEDevice.NetProperties.IPAddress[0] != 0 && LTEDevice.NetProperties.IPAddress[1] != 0 && LTEDevice.NetProperties.IPAddress[2] != 0 && LTEDevice.NetProperties.IPAddress[3] != 0;
		}
		goto EXIT;
	}

	snprintf(Filter, sizeof(Filter), "MEAS: Signal Quality: ");
	if((pBuffer = strstr(ReceiveBuffer, Filter)) != NULL){
		pBuffer += strlen(Filter);

		if((pBuffer = strstr(pBuffer, "RSSI = ")) != NULL){
			pBuffer += strlen("RSSI = ");
			LTEDevice.Status.RSSI = (int16_t)(atoi(pBuffer));
		}

		goto EXIT;
	}

EXIT:;
}

oResult_t LTE_StringToNetworkProperties(NetworkProperties_t *pProperties, char *pStr)
{
	uint8_t IsEmpty = 0;
	uint8_t i = 0;
	int Num[8];

	//+CGCONTRDP: 1,5,\"simplio.apn.mnc028.mcc901.gprs\",10.1.249.160. 255.255.255.192,10.1.249.129,176.95.16.250,176.95.16.251,,,,,,,,,\r\n\r\nOK\r\n"

	if((pStr = strstr(pStr, "\"")) == NULL){
		return RESULT_ERROR;
	}

	pStr++;
	if((pStr = strstr(pStr, ",")) == NULL){
		return RESULT_ERROR;
	}

	pStr++;
	sscanf(pStr, "%d.%d.%d.%d.%d.%d.%d.%d", &Num[0],&Num[1],&Num[2],&Num[3],&Num[4],&Num[5],&Num[6],&Num[7]);
	for(i=0; i<4; i++){
		pProperties->IPAddress[i] = (uint8_t)Num[i];
		pProperties->SubnetMask[i] = (uint8_t)Num[4+i];
	}

	if((pStr = strstr(pStr, ",")) == NULL){
		return RESULT_ERROR;
	}
	pStr++;
	sscanf(pStr, "%d.%d.%d.%d", &Num[0],&Num[1],&Num[2],&Num[3]);
	for(i=0; i<4; i++){
		pProperties->Gateway[i] = (uint8_t)Num[i];
	}


	if((pStr = strstr(pStr, ",")) == NULL){
		return RESULT_ERROR;
	}
	pStr++;
	sscanf(pStr, "%d.%d.%d.%d", &Num[0],&Num[1],&Num[2],&Num[3]);
	for(i=0; i<4; i++){
		pProperties->DNSServer[i] = (uint8_t)Num[i];
	}

	IsEmpty |= pProperties->IPAddress[0] == 0 && pProperties->IPAddress[1] == 0 && pProperties->IPAddress[2] == 0 && pProperties->IPAddress[3] == 0;
	IsEmpty |= pProperties->Gateway[0] == 0 && pProperties->Gateway[1] == 0 && pProperties->Gateway[2] == 0 && pProperties->Gateway[3] == 0;
	IsEmpty |= pProperties->DNSServer[0] == 0 && pProperties->DNSServer[1] == 0 && pProperties->DNSServer[2] == 0 && pProperties->DNSServer[3] == 0;

	if(IsEmpty){
		return RESULT_ERROR;
	}

	return RESULT_OK;
}

oResult_t LTE_GetNetworkProperties(NetworkProperties_t *pProperties)
{
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CGCONTRDP\r\n", "+CGCONTRDP: ", 1000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		case AT_RESULT_TIMEOUT:
			result = RESULT_TIMEOUT;
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			result = LTE_StringToNetworkProperties(pProperties, LTEDevice.pAT->pResponseData);
			break;
	}

	return result;
}

oResult_t LTE_GetUrlIPAddress(char *URL, char *pIPAddress)
{
	oResult_t result = RESULT_RUN;
	char *pStr;
	char StrCmd[512] ={0,};

	snprintf(StrCmd, sizeof(StrCmd), "AT%%DNSRSLV=0,\"%s\"\r\n", URL);

	switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "%DNSRSLV:0,\"", 3000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			if((pStr = strtok(LTEDevice.pAT->pResponseData, "\"")) != NULL){
				memcpy(pIPAddress, pStr, strlen(pStr));
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	return result;
}

oResult_t LTE_GetUSIMStatus()
{
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%STATUS=\"USIM\"\r\n", "USIM: ", 3000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		case AT_RESULT_TIMEOUT:
			result = RESULT_TIMEOUT;
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			LTEDevice.Status.IsUSIMOkay = strncmp(LTEDevice.pAT->pResponseData, "REAL USIM", strlen("REAL USIM")) == 0 ? 1 : 0;

			if(LTEDevice.Status.IsUSIMOkay){
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	return result;
}

oResult_t LTE_GetICCID()
{
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%CCID\r\n", "%CCID: ", 3000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			strncpy(LTEDevice.Information.ICCID, LTEDevice.pAT->pResponseData, sizeof(LTEDevice.Information.ICCID) - 1);
			LTEDevice.Information.ICCID[sizeof(LTEDevice.Information.ICCID) - 1] = '\0';

			result = RESULT_OK;
			break;
	}

	return result;
}

oResult_t LTE_GetIMEI()
{
	char *pStr;
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%DEVCFG=\"RD\",\"IMEI\"\r\n", "%DEVCFG:\"", 3000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			strncpy(LTEDevice.Information.IMEI, LTEDevice.pAT->pResponseData, sizeof(LTEDevice.Information.IMEI) - 1);
			LTEDevice.Information.IMEI[sizeof(LTEDevice.Information.IMEI) - 1] = '\0';

			if((pStr=strstr(LTEDevice.Information.IMEI,"\"")) != NULL){
				*pStr = 0;
			}
			result = RESULT_OK;
			break;
	}

	return result;
}

oResult_t LTE_GetTime(oDateAndTime_t *pDT, int32_t Timezone)
{
	int bff[7] = {0,};
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CCLK?\r\n", "+CCLK: ", 1000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
				//25/02/03,11:17:31+36
				sscanf(LTEDevice.pAT->pResponseData, "\"%d/%d/%d,%d:%d:%d+%d\"", &bff[0], &bff[1], &bff[2], &bff[3],&bff[4], &bff[5], &bff[6]);

			   if(bff[0] != 0 && bff[1] != 0 && bff[2] != 0){
				   pDT->Year = (uint16_t)bff[0] + 2000;
				   pDT->Month = (uint8_t)bff[1];
				   pDT->Day = (uint8_t)bff[2];
				   pDT->Hour = (uint8_t)bff[3];
				   pDT->Minute = (uint8_t)bff[4];
				   pDT->Second = (uint8_t)bff[5];

				   bff[6] /= 4;

				   if((int32_t)(bff[6]) != Timezone){
					   oDT_AddHour(pDT, Timezone-(int32_t)(bff[6]));
				   }

				   result = RESULT_OK;
			   }
			   else{
				   result = RESULT_ERROR;
			   }
			break;
	}

	return result;
}

oResult_t LTE_GetRSSI(int16_t *pRSSI)
{
	oResult_t result = RESULT_RUN;
	char *pBuffer;

	if(pRSSI == NULL){
		return RESULT_NULL;
	}

	switch(oAT_TransmitCommend(LTEDevice.pAT, "AT%MEAS=\"8\"\r\n", "%MEAS: Signal Quality: ", 1000, 1, True))
	{
		case AT_RESULT_RUN:
			break;
		default:
			result = RESULT_ERROR;
			break;
		case AT_RESULT_OK:
			//RSRP = -130, RSRQ = -15, SINR = -4, RSSI = -114
			if((pBuffer = strstr(LTEDevice.pAT->pResponseData, "RSSI = ")) != NULL){
				pBuffer += strlen("RSSI = ");
				*pRSSI = (int16_t)(atoi(pBuffer));
				result = RESULT_OK;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
	}

	return result;
}

oResult_t LTE_GetSignalQuality()
{
	char* pChar;
	int i = 0;
	oResult_t result = RESULT_RUN;

	switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%CSQ\r\n", "%CSQ: ", 5000, 1, True))
	{
		case AT_RESULT_TIMEOUT:
			result = RESULT_TIMEOUT;
			break;
		case AT_RESULT_OK:
			if ((pChar = strtok(LTEDevice.pAT->pResponseData, ",")) != NULL) {
				LTEDevice.Status.SignalQuality.Power = 0;
				LTEDevice.Status.SignalQuality.ErrorRate = 0;
				LTEDevice.Status.SignalQuality.Quality = 0;

				i = atoi(pChar);

				if(i >= 0 && i < 99){
					i = limit(i, 0, 31);

					LTEDevice.Status.SignalQuality.Power = (uint16_t)(-113 + (i*2));
				}


				if((pChar = strtok(NULL, ",")) != NULL){
					i = atoi(pChar);

					if(i >= 0 && i < 99){
						LTEDevice.Status.SignalQuality.ErrorRate = (uint8_t)i;
					}
				}

				if((pChar = strtok(NULL, ",")) != NULL){
					i = atoi(pChar);

					if(i >= 0 && i < 99){
						LTEDevice.Status.SignalQuality.Quality = (uint8_t)i;
					}
				}

				if(LTEDevice.Status.SignalQuality.Power == 0 || LTEDevice.Status.SignalQuality.Quality == 0){
					result = RESULT_ERROR;
				}
				else if(LTEDevice.Status.SignalQuality.Power < -100 || LTEDevice.Status.SignalQuality.Quality <= 11){
					result = RESULT_ERROR;
				}
				else{
					result = RESULT_OK;
				}
			}
			else{
				result = RESULT_NULL;
			}
			break;
		default:
			break;
	}

	return result;
}

oResult_t LTE_MQTTPublish(int QoS, char *Topic, char *pData, int SizeOfData)
{
	static uint32_t LTEMQTTPublishTimer = 0;
	static uint8_t LTEMQTTPublishStep = 0;
	char Str[1000] ={0,};
	char StrFilter[30] ={0,};
	oResult_t result = RESULT_RUN;

	switch(LTEMQTTPublishStep)
	{
		default:
			LTEMQTTPublishStep = 0;
		case 0:
			if(LTEDevice.pAT == NULL || strlen(Topic) <= 0 || !LTEDevice.Status.IsConnected){
				return RESULT_NULL;
			}

			oAT_TransmitSerialize(LTEDevice.pAT, (uint8_t *)AT_STRING_CRLF, 2);

			snprintf(Str, sizeof(Str), "AT%%MQTTCMD=\"PUBLISH\",%d,%d,0,\"%s\",%d\r", LTEDevice.SocketNumber, QoS, Topic, (int)(MATH_MAX(SizeOfData, 1)));
			oAT_TransmitSerialize(LTEDevice.pAT, (uint8_t *)Str, strlen(Str));

			if(pData != NULL && SizeOfData > 0){
				oAT_TransmitSerialize(LTEDevice.pAT, (uint8_t *)pData, SizeOfData);
			}

			oAT_TransmitSerialize(LTEDevice.pAT, (uint8_t *)AT_STRING_LF, 1);
			oAT_TransmitSerialize(LTEDevice.pAT, (uint8_t *)AT_STRING_CRLF, 2);

			LTEMQTTPublishStep++;
			LTEMQTTPublishTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		case 1:
			if(oAT_TransmitBuffer(LTEDevice.pAT) == AT_RESULT_OK){
				LTEMQTTPublishStep++;
			}
			else if(oTMR_Elapsed(&LTEMQTTPublishTimer, 500)){
				result = RESULT_ERROR;
			}
			break;
		case 2:
			snprintf(StrFilter, sizeof(StrFilter), "MQTTEVU:\"PUBCONF\",%d", LTEDevice.SocketNumber);
			switch(oAT_WaitForResponse(LTEDevice.pAT, StrFilter, SECOND_TO_MS(30)))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					result = RESULT_OK;
					MiMqtt_PublichTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
					LTE_StatusMessage("MQTT publish okay");
					break;
				default:
				case AT_RESULT_ERROR:
					LTE_StatusMessage("MQTT publish error");
					result = RESULT_ERROR;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("MQTT publish timeout");
					result = RESULT_TIMEOUT;
					break;
			}
			break;
	}

	if(result != RESULT_RUN){
		LTEMQTTPublishStep = 0;
	}

	return result;
}

oResult_t LTE_MQTTSubscribe(int QoS, char *Topic, LTE_SubscribeCallback_t Callback)
{
	char StrCmd[128] ={0,};
	char StrFilter[128] ={0,};
	char *pChar;
	char *pChar2;
	oResult_t result = RESULT_RUN;

	if(LTEDevice.pAT == NULL || !LTEDevice.Status.IsConnected){
		return RESULT_NULL;
	}

	snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCMD=\"SUBSCRIBE\",%d,%d,\"%s\"\r\n", LTEDevice.SocketNumber, QoS, Topic);
	snprintf(StrFilter, sizeof(StrFilter), "MQTTEVU:\"SUBCONF\",%d", LTEDevice.SocketNumber);

	switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, StrFilter, 10000, 1, True))
	{
		case AT_RESULT_OK:
			if ((pChar = strstr(LTEDevice.pAT->pResponseData, ",")) != NULL) {
				pChar++;
				pChar2 = pChar;
				if ((pChar = strstr(pChar2, ",")) != NULL) {
					pChar++;
					if (*pChar == '0') {
						LTE_SubscribeReceiveCallback = Callback;
						result = RESULT_OK;
						LTE_StatusMessage("MQTT subscribe okay");
					}
				}
			}
			break;
		case AT_RESULT_ERROR:
		case AT_RESULT_TIMEOUT:
			LTE_StatusMessage("MQTT subscribe error");
			result = RESULT_ERROR;
			break;
		default:
			break;
	}

	return result;
}

oResult_t LTE_MQTTUnsubscribe(char *Topic)
{
	oResult_t result = RESULT_RUN;
	char StrCmd[512] ={0,};
	char StrFilter[100] ={0,};
	char *pStr;

	if(LTEDevice.pAT == NULL){
		return RESULT_NULL;
	}

	if(!LTEDevice.Status.IsConnected){
		return RESULT_OK;
	}

	snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCMD=\"UNSUBSCRIBE\",%d,\"%s\"\r\n", LTEDevice.SocketNumber, Topic);
	snprintf(StrFilter, sizeof(StrFilter), "MQTTEVU:\"UNSCONF\",%d,", LTEDevice.SocketNumber);

	switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, StrFilter, 20000, 1, True))
	{
		case AT_RESULT_OK:
			//%MQTTEVU:"UNSCONF",1,3,0
			if ((pStr = strtok(LTEDevice.pAT->pResponseData, ",")) != NULL) {
				if ((pStr = strtok(NULL, ",")) != NULL) {
					if(*pStr == '0'){
						LTE_StatusMessage("MQTT unsubscribe okay");
						result = RESULT_OK;
					}

				}
			}
			break;
		case AT_RESULT_TIMEOUT:
			LTE_StatusMessage("MQTT unsubscribe error");
			result = RESULT_TIMEOUT;
			break;
		default:
			break;
	}

	return result;
}

oResult_t LTE_MQTTConnect(char* ClientID, char *Address, uint16_t Port)
{
	static uint8_t LTE_MQTTConnectStep = 0;
	static uint32_t LTE_MQTTConnectTimer = 0;
	static uint8_t LTE_MQTTConeectTryCount = 0;

	oResult_t result = RESULT_RUN;
	int Num[4];
	uint8_t i;
	char *pChar;
	char StrCmd[512] ={0,};
	char StrFilter[100] ={0,};

	if(LTEDevice.pAT == NULL){
		LTE_StatusMessage("MQTT connect AT is null");
		return RESULT_NULL;
	}

	switch (LTE_MQTTConnectStep)
	{
		case 0:
			LTE_MQTTConeectTryCount = 0;
			LTE_MQTTConnectStep++;
		case 1:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CEREG?\r\n", "+CEREG: ", 3000, 1, True))
			{
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("LTE registry timeout");
					LTE_MQTTConeectTryCount++;
					break;
				case AT_RESULT_OK:
					if ((pChar = strstr(LTEDevice.pAT->pResponseData, ",")) != NULL) {
						pChar++;

						/*
						 * | 0 | not registered. no searched.	|
						 * | 1 | registered						|
						 * | 2 | searching						|
						 * | 3 | rejected						|
						 * | 4 | unknown						|
						 * | 5 | roaming						|
						*/
						if (*pChar == '1' || *pChar == '5'){
							LTE_StatusMessage("MQTT lte network registered");
							LTE_MQTTConnectStep++;
							LTE_MQTTConeectTryCount = 0;
						}
						else if(oTMR_Trigger(&LTE_MQTTConnectTimer, 1000, 1)){
							if (*pChar == '0' || *pChar == '3') {
								if(*pChar == '0'){
									LTE_StatusMessage("MQTT lte network not registered");
								}
								else if(*pChar == '3'){
									LTE_StatusMessage("MQTT lte network rejected");
								}

								LTE_MQTTConeectTryCount++;
							}
							else if (*pChar == '2'){
								LTE_StatusMessage("MQTT lte network searching");
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case 2:
			LTEDevice.Status.IsConnecting = 1;
			LTEDevice.Status.IsConnected = 0;
			LTEDevice.SocketNumber = 0;

			snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTEV=\"ALL\",%d\r\n", 1);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 10000, 1, True))
			{
				case AT_RESULT_OK:
					LTE_StatusMessage("MQTT connect start");
					LTE_MQTTConnectStep++;
					LTE_MQTTConeectTryCount = 0;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			break;
		case 3:
			//sprintf(StrCmd, "AT%%MQTTCFG=\"NODES\",0,\"%s\",\"%s\",test,test", ClientID, Address);
			snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCFG=\"NODES\",%d,\"%s\",\"%s\"\r\n", LTEDevice.SocketNumber, ClientID, Address);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 10000, 1, True))
			{
				case AT_RESULT_OK:
					sscanf(Address, "%d.%d.%d.%d", &Num[0],&Num[1],&Num[2],&Num[3]);
					for(i=0; i<4; i++){
						LTEDevice.NetSocket.Address[i] = (uint8_t)Num[i];
					}

					LTE_StatusMessage("MQTT set address");
					LTE_MQTTConnectStep++;
					LTE_MQTTConeectTryCount = 0;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			break;
		case 4:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCFG=\"IP\",%d,,0,%d\r\n", LTEDevice.SocketNumber, (int)Port);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 10000, 1, True))
			{
				case AT_RESULT_OK:
					LTE_StatusMessage("MQTT set port");
					LTEDevice.NetSocket.Port = Port;
					LTE_MQTTConnectStep++;
					LTE_MQTTConeectTryCount = 0;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			break;
		case 5:
			LTE_MQTTConnectStep++;
			/*
			//sprintf(StrCmd, "AT%%MQTTCFG=\"TLS\",%d,2,0", LTE_SocketNo);
			//sprintf(StrCmd, "AT%%MQTTCFG=\"TLS\",%d,1,0", LTE_SocketNo);
			//sprintf(StrCmd, "AT%%MQTTCFG=\"TLS\",%d,0,0", LTE_SocketNo);

			switch (AT_TransmitCommend(pAT, StrCmd, "OK", 10000, 1))
			{
				case AT_RESULT_OK:
					LTE_MQTTConeectStep++;
					LTE_MQTTConeectTryCount = 0;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			*/
			break;
		case 6:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCFG=\"PROTOCOL\",%d,0,%u,1\r\n", LTEDevice.SocketNumber, (int)MINUTE_TO_SECOND(1));

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 10000, 1, True))
			{
				case AT_RESULT_OK:
					LTE_StatusMessage("MQTT set protocol");
					LTE_MQTTConnectStep++;
					LTE_MQTTConeectTryCount = 0;
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			break;
		case 7:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCMD=\"CONNECT\",%d\r\n", LTEDevice.SocketNumber);
			snprintf(StrFilter, sizeof(StrFilter), "MQTTEVU:\"CONCONF\",%d,", LTEDevice.SocketNumber);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, StrFilter, 20000, 1, True))
			{
				case AT_RESULT_OK:
					if (LTEDevice.pAT->pResponseData[0] == '0' || LTEDevice.Status.IsConnected) {
						result = RESULT_OK;
						LTEDevice.Status.IsReady = 0;
						LTEDevice.Status.IsConnected = 1;

						LTE_StatusMessage("MQTT connected");
						LTE_MQTTConeectTryCount = 0;
					}
					else{
						LTE_MQTTConeectTryCount++;
					}
					break;
				case AT_RESULT_TIMEOUT:
					LTE_MQTTConeectTryCount++;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if(LTE_MQTTConeectTryCount > 10){
		result = RESULT_ERROR;
	}

	if(result != RESULT_RUN){
		LTE_MQTTConnectStep = 0;
		LTEDevice.Status.IsConnecting = 0;
	}

	return result;
}

oResult_t LTE_MQTTDisconnect()
{
	oResult_t result = RESULT_RUN;
	char StrCmd[512] ={0,};
	char StrFilter[100] ={0,};

	if(LTEDevice.pAT == NULL){
		return RESULT_NULL;
	}

	if(!LTEDevice.Status.IsConnected){
		return RESULT_OK;
	}

	snprintf(StrCmd, sizeof(StrCmd), "AT%%MQTTCMD=\"DISCONNECT\",%d\r\n", LTEDevice.SocketNumber);
	snprintf(StrFilter, sizeof(StrFilter), "MQTTEVU:\"DISCONF\",%d,", LTEDevice.SocketNumber);

	switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, StrFilter, 20000, 1, True))
	{
		case AT_RESULT_OK:
			if(*LTEDevice.pAT->pResponseData == '0'){
				LTE_StatusMessage("Disconnected");
				LTEDevice.Status.IsConnected = 0;
				result = RESULT_OK;
			}
			break;
		case AT_RESULT_TIMEOUT:
			LTE_StatusMessage("Disconnect timeout");
			result = RESULT_TIMEOUT;
			break;
		default:
			break;
	}

	return result;
}

int LTE_IsConnected()
{
	static uint8_t MQTTConnectCheckStep = 0;
	static uint32_t MQTTConnectCheckTimer = 0;
	char StrCmd[100] ={0,};

	if(LTEDevice.pAT->State.IsWaitResponse || !LTEDevice.Status.IsConnected){
		MQTTConnectCheckTimer = oTMR_GetTick(TICKBASE_SYSTICK);
	}
	else if(oTMR_Trigger(&MQTTConnectCheckTimer, 1500, 1)){
		switch(MQTTConnectCheckStep)
		{
			default:
				MQTTConnectCheckStep = 0;
			case 0:
				sprintf(StrCmd, "\r\nAT+CEREG?\r\n\r\n");
				MQTTConnectCheckStep++;
				break;
			case 1:
				sprintf(StrCmd, "\r\nAT%%CMATT?\r\n\r\n");
				MQTTConnectCheckStep++;
				break;
			case 2:
				sprintf(StrCmd, "\r\nAT%%MEAS=\"8\"\r\n\r\n");
				break;
			case 3:
				sprintf(StrCmd, "\r\nAT+CGCONTRDP\r\n\r\n");
				break;
		}

		if(strlen(StrCmd) > 0){
			oAT_TransmitData(LTEDevice.pAT, (uint8_t *)&StrCmd, strlen(StrCmd));
		}
	}

	return (int)LTEDevice.Status.IsConnected;
}

int LTE_SocektConnect(char* Address, int Port)
{
	static uint8_t LTE_SocketConeectStep = 0;
	int result = 0;
	char StrCmd[512] ={0,};

	switch (LTE_SocketConeectStep)
	{
		case 0:
			if(!LTEDevice.Status.IsReady){
				LTE_SocketConeectStep = 0;
				return -1;
			}

			LTEDevice.Status.IsConnecting = 1;

			snprintf(StrCmd, sizeof(StrCmd), "AT%%HTTPEV=\"ALL\",%d\r\n", 1);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 3000, 1, True))
			{
				case AT_RESULT_OK:
					LTE_SocketConeectStep++;
					break;
				default:
					break;
			}
			break;
		case 1:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%SOCKETCMD=\"ALLOCATE\",0,\"TCP\",\"OPEN\",\"%s\",%d,1500\r\n", Address, (int)Port);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "SOCKETCMD: ", 20000, 1, True))
			{
				case AT_RESULT_OK:
					LTEDevice.SocketNumber = atoi(LTEDevice.pAT->pResponseData); //number of socket
					LTE_SocketConeectStep++;
					break;
				default:
					break;
			}
			break;
		case 2: //TLS
			LTE_SocketConeectStep++;

			/*
			sprintf(szCmd, "AT%%SOCKETCMD=\"SSLALLOC\",1,0,%d\r\n", tlsProfileNo);

			switch (AT_TransmitCommend(pAT, szCmd, "OK", 3000, 1))
			{
				case AT_RESULT_OK:
					LTE_SocketConeectStep++;
					break;
				default:
					break;
			}
			break;
			*/
		case 3:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%SOCKETCMD=\"ACTIVATE\",%d\r\n", LTEDevice.SocketNumber);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "OK", 3000, 1, True))
			{
				case AT_RESULT_OK:
					LTE_SocketConeectStep++;
					break;
				default:
					break;
			}
			break;
		case 4:
			snprintf(StrCmd, sizeof(StrCmd), "AT%%SOCKETCMD=\"INFO\",%d\r\n", LTEDevice.SocketNumber);

			switch (oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "SOCKETCMD:\"", 5000, 1, True))
			{
				case AT_RESULT_OK:
					switch(LTEDevice.pAT->pResponseData[0])
					{
						case 'A': //active
							LTEDevice.Status.IsReady = 0;
							LTEDevice.Status.IsConnected = 1;
							result = 1;
							break;
						default: //fail
						case 'D': //deactive
							result = -1;
							break;
					}
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if(result != 0){
		LTE_SocketConeectStep = 0;
		LTEDevice.Status.IsConnecting = 0;
	}

	return result;
}

int LTE_WriteSocket(char *pBuffer, int Length, int Timeout)
{
	char *sendBuffer = (char *)LTEDevice.pAT->pBufferTx;
	char *pSendBuffer = NULL;
	int bufferSize = 0;
	int result = 0;
	int i = 0;

	if(!LTEDevice.Status.IsConnected || Length<= 0){
		return -1;
	}

	bufferSize = (Length * 2) + 64;
	memset(sendBuffer, 0, bufferSize);

	sprintf(sendBuffer, "AT%%SOCKETDATA=\"SEND\",%d,%d,\"\r\n", LTEDevice.SocketNumber, Length);

	pSendBuffer = sendBuffer + strlen(sendBuffer);

	for(i = 0; i < Length; ++i) {
		*pSendBuffer = (NIBBLE_TO_HEX_CHAR(HIGH_NIBBLE(pBuffer[i])));
		pSendBuffer++;
		*pSendBuffer = (NIBBLE_TO_HEX_CHAR(LOW_NIBBLE(pBuffer[i])));
		pSendBuffer++;
	}

	*pSendBuffer = '\"';

	switch(oAT_TransmitCommend(LTEDevice.pAT, sendBuffer, "OK", 2000, 0, True))
	{
		default:
			break;
		case AT_RESULT_ERROR:
		case AT_RESULT_TIMEOUT:
			result = -1;
			break;
		case AT_RESULT_OK:
			result = Length;
			break;
	}

	return result;
}

int LTE_ReadSocket(char *pBuffer, int Length, int Timeout)
{
	int SizeOfRead = 0;
	uint32_t i = 0;
	char StrCmd[100] = {0,};
	char *pStr = NULL;

	if(!LTEDevice.Status.IsConnected || Length <= 0 || Timeout <= 0){
		return -1;
	}

	snprintf(StrCmd, sizeof(StrCmd), "AT%%SOCKETDATA=\"RECEIVE\",%d,1000\r\n", LTEDevice.SocketNumber);

	switch(oAT_TransmitCommend(LTEDevice.pAT, StrCmd, "SOCKETDATA: ", 20000, 0, True))
	{
		default:
			break;
		case AT_RESULT_ERROR:
		case AT_RESULT_TIMEOUT:
			SizeOfRead = -1;
			break;
		case AT_RESULT_OK:
			if((pStr = strstr(LTEDevice.pAT->pResponseData, ",")) != NULL){
				pStr++;

				SizeOfRead = atoi(pStr);
			}
			else{
				SizeOfRead = -1;
			}

			if(SizeOfRead > 0 && Length >= SizeOfRead/2){
				if((pStr = strstr(LTEDevice.pAT->pResponseData, "\"")) != NULL){
					memset(pBuffer, 0, Length);

					for(i=0; i<SizeOfRead*2; i+=2){
						*(pBuffer +(i/2)) = HEX_PAIR_TO_BYTE(LTEDevice.pAT->pResponseData[i], LTEDevice.pAT->pResponseData[i + 1]);
					}
				}
				else{
					SizeOfRead = -1;
				}
			}
			break;
	}

	return SizeOfRead;
}

oResult_t LTE_DeInit()
{
	static uint8_t LTE_DeInitStep = 0;
	oResult_t result = RESULT_RUN;

	switch(LTE_DeInitStep)
	{
		default:
			LTE_DeInitStep = 0;
		case 0://LTE Session close - IP 諛섎궔
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%PDNACT=0,1", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("LTE detach timeout");
				case AT_RESULT_OK:
					LTE_DeInitStep++;
					break;
				default:
					break;
			}
			break;
		case 1://LTE Detach
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%CMATT=0\r\n", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("LTE detach timeout");
				case AT_RESULT_OK:
					LTE_DeInitStep++;
					break;
				default:
					break;
			}
			break;
		case 2://Network search stop
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+COPS=2\r\n", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("LTE detach timeout");
				case AT_RESULT_OK:
					LTE_DeInitStep++;
					break;
				default:
					break;
			}
			break;
		case 3://RF off
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CFUN=0\r\n", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTE_StatusMessage("LTE detach timeout");
				case AT_RESULT_OK:
					LTE_DeInitStep++;
					break;
				default:
					break;
			}
			break;
		case 4:
			if(oAT_Close(LTEDevice.pAT) == AT_RESULT_OK){
				LTEDevice.pAT->ReceiveCallback = NULL;
				LTEDevice.pAT = NULL;
				LTEDevice.SocketNumber = 0;

				memset(&LTEDevice.Status, 0, sizeof(LTEDevice.Status));
				//memset(&LTEDevice.NetProperties, 0, sizeof(LTEDevice.NetProperties));
				//memset(&LTEDevice.NetSocket, 0, sizeof(LTEDevice.NetSocket));

				LTEDevice.Status.IsPowerOn = 0; //power off
				LTEDevice.Status.IsResetOn = 0; //reset off

				IO_WRITE_LOWSIDE(IO_LTE_ENABLE, LTEDevice.Status.IsPowerOn);
				IO_WRITE_LOWSIDE(GPIO_LTE_RESET, LTEDevice.Status.IsResetOn);

				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		LTE_DeInitStep = 0;
	}

	return result;
}

oResult_t LTE_Init(oATCommend_t *pATCommend, NetworkProperties_t *pProperties,  NetworkConnection_t *pConnection)
{
	static uint8_t LTE_InitTryCount = 0;
	static uint8_t LTE_InitStep = 0;
	static uint32_t LTE_InitTimer = 0;
	oResult_t result = RESULT_RUN;
	char Message[50] = {0,};
	char *pChar;

	if(pATCommend == NULL){
		return RESULT_NULL;
	}

	LTEDevice.pAT = pATCommend;
	LTEDevice.pAT->ReceiveCallback = LTE_ATReceiveDataCallback;

	if(pConnection != NULL){
		pConnection->Connect = LTE_SocektConnect;
		pConnection->Write = LTE_WriteSocket;
		pConnection->Read = LTE_ReadSocket;
		pConnection->IsConnected = LTE_IsConnected;
	}

	switch (LTE_InitStep)
	{
		default:
			LTE_InitStep = 0;
		case 0:
			memset(&LTEDevice.Status, 0, sizeof(LTEDevice.Status));

			LTEDevice.Status.IsPowerOn = 0; //power off
			LTEDevice.Status.IsResetOn = 0; //reset off
			LTEDevice.Status.IsWarmUp = 1;

			LTE_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);

			LTE_InitTryCount = 0;

			if(oAT_Close(pATCommend) == AT_RESULT_OK){
				LTE_InitStep++;
			}
			break;
		case 1:
			if(oTMR_Elapsed(&LTE_InitTimer, 2000)){
				LTEDevice.Status.IsPowerOn = 1;	 //power on
				LTE_InitStep++;
				LTE_InitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			break;
		case 2:
			if(oTMR_Trigger(&LTE_InitTimer, 500, 1)){
				LTEDevice.Status.IsResetOn = 1; //reset on
				LTE_InitStep++;
			}
			break;
		case 3:
			if(oTMR_Trigger(&LTE_InitTimer, 500, 1)){
				LTEDevice.Status.IsResetOn = 0; //reset off
				LTE_InitStep++;
			}
			break;
		case 4:
			if(oTMR_Trigger(&LTE_InitTimer, 1000, 1)){
				if(oAT_Open(pATCommend) == AT_RESULT_OK){
					LTE_InitStep++;
				}
			}
			break;
		case 5:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CFUN=1,1\r\n", "OK", 5000, 5, True))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					LTE_InitTryCount++;

					if(LTE_InitTryCount > 10){
						LTEDevice.Status.IsSerialFault = 1;
						snprintf(Message, sizeof(Message), "CFUN Error");
					}
					break;
				case AT_RESULT_OK:
					LTE_InitStep++;
					LTE_InitTryCount = 0;
					LTEDevice.Status.IsSerialFault = 0;
					snprintf(Message, sizeof(Message), "CFUN Okay");
					break;
			}
			break;
		case 6:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT\r\n", "OK", 5000, 5, True))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					LTE_InitTryCount++;

					if(LTE_InitTryCount > 10){
						LTEDevice.Status.IsSerialFault = 1;
						snprintf(Message, sizeof(Message), "AT Error");
					}
					break;
				case AT_RESULT_OK:
					LTE_InitStep++;
					LTE_InitTryCount = 0;
					LTEDevice.Status.IsSerialFault = 0;
					break;
			}
			break;
		case 7:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "ATE0\r\n", "ATE0", 5000, 5, True))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					LTE_InitStep++;
					LTE_InitTryCount = 0;
					LTEDevice.Status.IsSerialFault = 0;
					break;
				case AT_RESULT_OK:
					LTE_InitTryCount++;

					if(LTE_InitTryCount > 10){
						LTEDevice.Status.IsSerialFault = 1;
						snprintf(Message, sizeof(Message), "ATE0 Error");
					}
					break;
			}
			break;
		case 8:
			switch(LTE_GetUSIMStatus(True))
			{
				default:
					break;
				case RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
				case RESULT_ERROR:
					LTE_InitTryCount++;

					if(LTE_InitTryCount > 10){
						LTEDevice.Status.IsSerialFault = 1;
						snprintf(Message, sizeof(Message), "USIM Error");
					}
					break;
				case RESULT_OK:
					LTE_InitTryCount = 0;
					LTE_InitStep++;
					LTEDevice.Status.IsSerialFault = 0;
					break;
			}
			break;
		case 9:
			if(LTE_GetICCID() == RESULT_OK){
				LTE_InitStep++;
			}
			break;
		case 10:
			if(LTE_GetIMEI() == RESULT_OK){
				LTE_InitStep++;
			}
			break;
		case 11:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CEREG=2\r\n", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					snprintf(Message, sizeof(Message), "LTE registry commend timeout");
					LTE_InitTryCount++;
					break;
				case AT_RESULT_OK:
					LTEDevice.Status.IsSerialFault = 0;
					LTE_InitStep++;
					LTE_InitTryCount = 0;
					break;
				default:
					break;
			}
			break;
		case 12:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+CEREG?\r\n", "+CEREG: ", 10000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					snprintf(Message, sizeof(Message), "LTE registry timeout");
					LTE_InitTryCount++;
					break;
				case AT_RESULT_OK:
					LTEDevice.Status.IsSerialFault = 0;

					if ((pChar = strstr(LTEDevice.pAT->pResponseData, ",")) != NULL) {
						pChar++;
						/*
						 * | 0 | not registered. no searched.	|
						 * | 1 | registered						|
						 * | 2 | searching						|
						 * | 3 | rejected						|
						 * | 4 | unknown						|
						 * | 5 | roaming						|
						*/
						if (*pChar == '1' || *pChar == '5'){
							snprintf(Message, sizeof(Message), "LTE network registered");
							LTEDevice.Status.IsPHYOkay = 1;
							LTE_InitStep++;
							LTE_InitTryCount = 0;
						}
						else if(oTMR_Trigger(&LTE_InitTimer, 3000, 1)){
							if (*pChar == '0' || *pChar == '3') {
								if(*pChar == '0'){
									snprintf(Message, sizeof(Message), "LTE network not registered");
								}
								else if(*pChar == '3'){
									snprintf(Message, sizeof(Message), "LTE network rejected");
								}

								LTE_InitTryCount++;
							}
							else if (*pChar == '2'){
								snprintf(Message, sizeof(Message), "LTE network searching");
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case 13:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%CMATT=1\r\n", "OK", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					snprintf(Message, sizeof(Message), "LTE try attach timeout");
					LTE_InitTryCount++;
					break;
				case AT_RESULT_OK:
					LTEDevice.Status.IsSerialFault = 0;
					snprintf(Message, sizeof(Message), "LTE try attach");
					LTE_InitStep++;
					LTE_InitTryCount = 0;
					break;
				default:
					break;
			}
			break;
		case 14:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT%CMATT?\r\n", "%CMATT: ", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					snprintf(Message, sizeof(Message), "LTE attach check timeout");
					LTE_InitTryCount++;
					break;
				case AT_RESULT_OK:
					LTEDevice.Status.IsSerialFault = 0;

					if (*LTEDevice.pAT->pResponseData == '1'){
						snprintf(Message, sizeof(Message), "LTE attached");

						LTE_InitStep++;
						LTE_InitTryCount = 0;
					}
					else if(oTMR_Trigger(&LTE_InitTimer, 3000, 1)){
						LTE_InitTryCount++;
						snprintf(Message, sizeof(Message), "LTE retry attach");
					}
					break;
				default:
					break;
			}
			break;
		case 15:
			switch (oAT_TransmitCommend(LTEDevice.pAT, "AT+COPS?\r\n", "+COPS: ", 5000, 5, True))
			{
				case AT_RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					snprintf(Message, sizeof(Message), "APN read timeout");
					LTE_InitTryCount++;
					break;
				case AT_RESULT_OK:
					LTEDevice.Status.IsSerialFault = 0;

					if ((pChar = strtok(LTEDevice.pAT->pResponseData, "\"")) != NULL) {
						if ((pChar = strtok(NULL, "\"")) != NULL) {
							//"0,0,\"DATA ONLY\",7"
							strncpy(LTEDevice.Information.APN, pChar, sizeof(LTEDevice.Information.APN) - 1);
							LTEDevice.Information.APN[sizeof(LTEDevice.Information.APN) - 1] = '\0';
							LTE_InitStep++;
							LTE_InitTryCount = 0;
						}
					}
					break;
				default:
					break;
			}
			break;
		case 16:
			switch(LTE_GetNetworkProperties(&LTEDevice.NetProperties))
			{
				default:
					break;
				case RESULT_TIMEOUT:
					LTEDevice.Status.IsSerialFault = 1;
					LTE_InitTryCount++;
					snprintf(Message, sizeof(Message), "NetProperties timeout");
					break;
				case RESULT_OK:
					if(pProperties != NULL){
						memcpy(pProperties, &LTEDevice.NetProperties, sizeof(LTEDevice.NetProperties));
					}

					LTE_InitStep++;
					LTE_InitTryCount = 0;
					snprintf(Message, sizeof(Message), "network connected");
					LTEDevice.Status.IsSerialFault = 0;
					break;
			}
			break;
		case 17:
			result = RESULT_OK;
			LTEDevice.Status.IsReady = 1;
			break;
	}

	if(LTE_InitTryCount > 10){
		LTE_InitStep = 0;
		result = RESULT_ERROR;

	}

	if(strlen(Message)){
		LTE_StatusMessage(Message);
	}

	if(result != RESULT_RUN){
		LTE_InitStep = 0;
		LTEDevice.Status.IsWarmUp = 0;
	}

	IO_WRITE_LOWSIDE(IO_LTE_ENABLE, LTEDevice.Status.IsPowerOn);
	IO_WRITE_LOWSIDE(GPIO_LTE_RESET, LTEDevice.Status.IsResetOn);

	return result;
}
