#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <LAN_WIZ750SR.h>
#include <Mi_Main.h>
#include "ONE_Common.h"
#include "ONE_Time.h"


#define LAN_TCP_KEEPALIVE_TIME			1000

#define LAN_ATCMD_AT_MODE				"+++"
#define LAN_ATCMD_GW_MODE				"EX\r\n"
#define LAN_ATCMD_REBOOT				"RT\r\n"		//"REBOOT"
#define LAN_ATCMD_STATUS				"ST\r\n"		//제품의 동작 상태(Status) 확인
#define LAN_ATCMD_OP_PORT_1883			"LP1883\r\n"
#define LAN_ATCMD_DATAPACK_TIME			"PT500\r\n"		//0: 사용하지 않음 / 1 ~ 65535: 데이터 패킹 시간 (Unit: millisecond)
#define LAN_ATCMD_TCP_INACTIVITY_TIME	"IT0\r\n"		//0: 사용하지 않음 / 1 ~ 65535: 비활성 타이머 값 (Unit: second)
#define LAN_ATCMD_TCP_KEEPALIVE_ACTIVE	"KI7000\r\n"	//0: 사용하지 않음 / 1 ~ 65535: 초기 Keep-alive 패킷 전송 간격 (Unit: millisecond)

uint8_t LAN_IsInitialized;
uint8_t LAN_IsAllocatedAddress;
uint8_t LAN_IsTCPConnected;
uint8_t LAN_IsPHYLink;
int8_t LAN_IsATMode;

static ATCommend_t *pLanAT;

LANStatus_t LAN_Status;

static void LAN_ATReceiveDataCallback(char *pData, uint32_t Length)
{
	char *pStr;

	if((pStr = strstr(pData, "ERNOCOMMAND:++")) != NULL){
		LAN_IsATMode = 1;
		return;
	}

	if((pStr = strstr((char *)pData, "REBOOT")) != NULL){
		LAN_IsATMode = 0;
		return;
	}
}

static void LAN_ATResponseCallback(char *Request, char *Response, char *Filter)
{
	if(strstr(Response, "ST") != NULL){
		if(strstr(Response, "BOOT") != NULL){
			LAN_Status = LANState_BOOT;
			return;
		}
		if(strstr(Response, "OPEN") != NULL){
			LAN_Status = LANState_OPEN;
			return;
		}
		if(strstr(Response, "CONNECT") != NULL){
			LAN_Status = LANState_CONNECT;
			return;
		}
		if(strstr(Response, "UPGRADE") != NULL){
			LAN_Status = LANState_UPGRADE;
			return;
		}
		if(strstr(Response, "ATMODE") != NULL){
			LAN_Status = LANState_ATMODE;
			LAN_IsATMode = 1;
			return;
		}
		if(strstr(Response, "UDP") != NULL){
			LAN_Status = LANState_UDP;
			return;
		}
	}

	if(strstr(Response, "REBOOT") != NULL){
		LAN_IsATMode = 0;
		return;
	}
}

uint8_t LAN_SetATMode (uint8_t Enable)
{
	static uint8_t LAN_ATMode_Step = 0;

	char Request[100] = {0,};
	char Filter[100] = {0,};
	uint8_t result = RESULT_RUN;

	if(pLanAT == NULL) return 0;

	if(Enable){
		sprintf(Request, "%s", LAN_ATCMD_AT_MODE);
		sprintf(Filter, "SEG:AT");
	}
	else{
		sprintf(Request, "%s", LAN_ATCMD_GW_MODE);
		sprintf(Filter, "SEG:GW");
	}

	switch(LAN_ATMode_Step)
	{
		case 0:
			if((Enable && LAN_IsATMode > 0) || (!Enable && LAN_IsATMode == 0)){
				result = RESULT_OK;
				break;
			}
			else{
				LAN_ATMode_Step++;
				pLanAT->ReceiveCallback = LAN_ATReceiveDataCallback;
			} // @suppress("No break at end of case")
		case 1:
			switch(AT_TransmitCommend(pLanAT, Request, Filter, 1500, 5, false))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					if(LAN_IsATMode == Enable){
						result = RESULT_OK;
					}
					else{
						LAN_ATMode_Step++;
					}
					break;
				case AT_RESULT_OK:
					result = RESULT_OK;
					break;
			}
			break;
		case 2:
			switch(AT_TransmitCommend(pLanAT, "ST", "ST", 1000, 1, true))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					if(!Enable){
						result = RESULT_OK;
					}
					else{
						LAN_ATMode_Step = 0;
					}
					break;
				case AT_RESULT_OK:
					LAN_ATMode_Step = 0;
					break;
			}
			break;
	}

	if(result != RESULT_RUN){
		LAN_IsATMode = Enable ? 1 : 0;
		LAN_ATMode_Step = 0;

		if(!LAN_IsATMode){
			pLanAT->ReceiveCallback = NULL;
		}
	}

	return result != RESULT_RUN ? 1 : 0;
}

uint8_t LAN_Reset(uint8_t Factory, uint32_t DelayTime)
{
	static uint8_t LAN_Reset_Step = 0;
	static uint32_t LAN_Reset_Timer = 0;

	char Bff[50] = {0,};

	switch(LAN_Reset_Step)
	{
		case 0:
			if(LAN_SetATMode(1)){
				LAN_Reset_Step++;
			}
			break;
		case 1:
			if(Factory){
				sprintf(Bff, "FR");
			}
			else{
				sprintf(Bff, "RT");
			}

			if(AT_TransmitCommend(pLanAT, Bff, "REBOOT", 2000, 0, true) == AT_RESULT_OK){
				TMR_ResetNow(&LAN_Reset_Timer);
				LAN_IsATMode = 0;
				LAN_Reset_Step++;
			}
			break;
		case 2:
			if(TMR_Elapsed(&LAN_Reset_Timer, MATH_MAX(DelayTime, 2000))){
				LAN_Reset_Step++;
			}
			break;
		default:
			LAN_Reset_Step = 0;
			return 1;
			break;
	}

	return 0;
}

uint8_t LAN_SetNetworkProperties(NetworkProperties_t *pNetProperties)
{
	static uint8_t LAN_SetNetwork_Step = 0;

	uint8_t result = RESULT_RUN;

	char Bff[200] = {0,};

	if(!LAN_IsATMode){
		LAN_SetNetwork_Step = 0;
	}

	switch(LAN_SetNetwork_Step)
	{
		case 0:
			if(LAN_SetATMode(1)){
				if(pNetProperties->IPAddress[0] == 0 && pNetProperties->IPAddress[1] == 0 && pNetProperties->IPAddress[2] == 0 && pNetProperties->IPAddress[3] == 0){
					pNetProperties->Properties |= 0x01;
				}
				else{
					pNetProperties->Properties &= ~0x01;
				}
				LAN_SetNetwork_Step++;
			}
			break;
		case 1:
			sprintf(Bff, "LI%d.%d.%d.%d", pNetProperties->IPAddress[0], pNetProperties->IPAddress[1], pNetProperties->IPAddress[2], pNetProperties->IPAddress[3]);
			if(AT_TransmitCommend(pLanAT, Bff, NULL, 1000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		case 2:
			sprintf(Bff, "SM%d.%d.%d.%d", pNetProperties->SubnetMask[0], pNetProperties->SubnetMask[1], pNetProperties->SubnetMask[2], pNetProperties->SubnetMask[3]);
			if(AT_TransmitCommend(pLanAT, Bff, NULL, 1000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		case 3:
			sprintf(Bff, "GW%d.%d.%d.%d", pNetProperties->Gateway[0], pNetProperties->Gateway[1], pNetProperties->Gateway[2], pNetProperties->Gateway[3]);
			if(AT_TransmitCommend(pLanAT, Bff, NULL, 1000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		case 4:
			if(pNetProperties->Properties & 0x01){
				sprintf(Bff, "IM1");
			}
			else{
				sprintf(Bff, "IM0");
			}

			if(AT_TransmitCommend(pLanAT, Bff, NULL, 1000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		case 5:
			if(AT_TransmitCommend(pLanAT, "RH0.0.0.0", NULL, 2000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		case 6:
			if(AT_TransmitCommend(pLanAT, "RP0", NULL, 2000, 0, true) == AT_RESULT_OK){
				LAN_SetNetwork_Step++;
			}
			break;
		default:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		LAN_SetNetwork_Step = 0;
	}

	return result != RESULT_RUN ? 1 : 0;
}

uint8_t LAN_GetNetworkProperties (NetworkProperties_t *pNetProperties)
{
	static uint8_t LAN_GetNetwork_Step = 0;
	static NetworkProperties_t Network;
	static uint8_t IsEmpty, TryCount;

	int32_t D32[6];
	uint8_t i = 0;
	uint8_t result = RESULT_RUN;

	if(!LAN_IsATMode){
		LAN_GetNetwork_Step = 0;
	}

	switch(LAN_GetNetwork_Step)
	{
		case 0:
			TryCount = 0;
			LAN_GetNetwork_Step++; // @suppress("No break at end of case")
		case 1:
			if(LAN_SetATMode(1)){
				IsEmpty = 0;
				TryCount++;
				LAN_GetNetwork_Step++;
			}
			break;
		case 2:
			if(AT_TransmitCommend(pLanAT, "IM", "IM", 1000, 0, true) == AT_RESULT_OK){
				LAN_GetNetwork_Step++;

				if(strstr(pLanAT->pResponseData, "IM0") != NULL){
					Network.Properties &= ~0x01;
				}
				else if(strstr(pLanAT->pResponseData, "IM1") != NULL){
					Network.Properties |= 0x01;
				}
			}
			break;
		case 3:
			if(AT_TransmitCommend(pLanAT, "LI", "LI", 1000, 0, true) == AT_RESULT_OK){
				LAN_GetNetwork_Step++;
				sscanf(pLanAT->pResponseData, "%d.%d.%d.%d", (int *)&D32[0],(int *)&D32[1],(int *)&D32[2],(int *)&D32[3]);

				for(i=0; i<4; i++){
					Network.IPAddress[i] = (uint8_t)D32[i];
				}

				IsEmpty |= Network.IPAddress[0] == 0 && Network.IPAddress[1] == 0 && Network.IPAddress[2] == 0 && Network.IPAddress[3] == 0 ? 1 : 0;
			}
			break;
		case 4:
			if(AT_TransmitCommend(pLanAT, "SM", "SM", 1000, 0, true) == AT_RESULT_OK){
				LAN_GetNetwork_Step++;
				sscanf(pLanAT->pResponseData, "%d.%d.%d.%d", (int *)&D32[0],(int *)&D32[1],(int *)&D32[2],(int *)&D32[3]);

				for(i=0; i<4; i++){
					Network.SubnetMask[i] = (uint8_t)D32[i];
				}

				IsEmpty |= Network.SubnetMask[0] == 0 && Network.SubnetMask[1] == 0 && Network.SubnetMask[2] == 0 && Network.SubnetMask[3] == 0 ? 1 : 0;
			}
			break;
		case 5:
			if(AT_TransmitCommend(pLanAT, "GW", "GW", 1000, 0, true) == AT_RESULT_OK){
				LAN_GetNetwork_Step++;
				sscanf(pLanAT->pResponseData, "%d.%d.%d.%d", (int *)&D32[0],(int *)&D32[1],(int *)&D32[2],(int *)&D32[3]);

				for(i=0; i<4; i++){
					Network.Gateway[i] = (uint8_t)D32[i];
				}
			}
			break;
		case 6:
			if(AT_TransmitCommend(pLanAT, "MC", "MC", 1000, 0, true) == AT_RESULT_OK){
				LAN_GetNetwork_Step++;

				sscanf(pLanAT->pResponseData, "%2x:%2x:%2x:%2x:%2x:%2x", (int *)&D32[0],(int *)&D32[1],(int *)&D32[2],(int *)&D32[3],(int *)&D32[4],(int *)&D32[5]);

				for(i=0; i<6; i++){
					Network.MACAdress[i] = (uint8_t)D32[i];
				}
			}
			break;
		case 7:
			if(IsEmpty){
				LAN_GetNetwork_Step = 1;
			}
			else{
				result = RESULT_OK;
			}
			break;
	}

	if(result != RESULT_RUN){
		LAN_GetNetwork_Step = 0;
		Network.IsValid = !IsEmpty;
		LAN_IsAllocatedAddress = !IsEmpty;

		if(pNetProperties != NULL){
			*pNetProperties = Network;
		}
	}

	return result != RESULT_RUN  ? 1 : 0;
}

uint8_t LAN_SetOperationMode (LANOperatingMode_t Mode)
{
	char Bff[200] = {0,};

	/* 0=TCPCLIENT / 1=TCPSERVER / 2=TCPMIX / 3=UDP */

	if(!LAN_IsATMode){
		LAN_SetATMode(1);
	}
	else{
		sprintf(Bff, "OP%d", (uint8_t)Mode);

		if(AT_TransmitCommend(pLanAT, Bff, NULL, 100, 0, true) == AT_RESULT_OK){
			return 1;
		}
	}

	return 0;
}

uint8_t LAN_SetTCPConfig (uint16_t KeepAliveTime)
{
	static uint8_t LAN_TCPConfig_Step = 0;

	uint8_t result = RESULT_RUN;
	char Bff[100] = {0,};

	if(!LAN_IsATMode){
		LAN_TCPConfig_Step = 0;
	}

	switch(LAN_TCPConfig_Step)
	{
		case 0:
			if(LAN_SetATMode(1)){
				LAN_TCPConfig_Step++;
			}
			else{
				break;
			} // @suppress("No break at end of case")
		case 1:
			sprintf(Bff, "RI%d", 1000); //TCP 클라이언트 재연결 간격

			if(AT_TransmitCommend(pLanAT, Bff, NULL, 100, 0, true) == AT_RESULT_OK){
				LAN_TCPConfig_Step++;
			}
			break;
		case 2:
			if(KeepAliveTime > 0){
				sprintf(Bff, "KA%d", KeepAliveTime); //TCP Keep-alive 허용 설정

				if(AT_TransmitCommend(pLanAT, Bff, NULL, 100, 0, true) == AT_RESULT_OK){
					LAN_TCPConfig_Step++;
				}
			}
			else{
				result = RESULT_OK;
			}
			break;
		case 3:
			sprintf(Bff, "KI%d", KeepAliveTime/2); //TCP Keep-alive 초기 간격

			if(AT_TransmitCommend(pLanAT, Bff, NULL, 100, 0, true) == AT_RESULT_OK){
				LAN_TCPConfig_Step++;
			}
			break;
		case 4:
			sprintf(Bff, "KE%d", KeepAliveTime); //TCP Keep-alive 재시도 간격

			if(AT_TransmitCommend(pLanAT, Bff, NULL, 100, 0, true) == AT_RESULT_OK){
				LAN_TCPConfig_Step++;
			}
			break;
		default:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		LAN_TCPConfig_Step = 0;
	}

	return result != RESULT_RUN ? 1 : 0;
}

uint8_t LAN_GetConnectionState()
{
	static uint8_t LAN_ConnectionState_Step = 0;

	uint8_t result = RESULT_RUN;

	switch(LAN_ConnectionState_Step)
	{
		case 0:
			if(LAN_SetATMode(1)){
				LAN_ConnectionState_Step++;
			}
			break;
		case 1:
			switch(AT_TransmitCommend(pLanAT, "ST", "ST", 1000, 0, true))
			{
				default:
					break;
				case AT_RESULT_ERROR:
				case AT_RESULT_TIMEOUT:
					LAN_ConnectionState_Step = 0;
					break;
				case AT_RESULT_OK:
					LAN_ConnectionState_Step++;
					break;
			}
			break;
		case 2:
			switch(AT_TransmitCommend(pLanAT, "S0", "S0", 2000, 0, true)) //DTR - Phycal layer state
			{
				default:
					break;
				case AT_RESULT_ERROR:
				case AT_RESULT_TIMEOUT:
					LAN_ConnectionState_Step = 0;
					break;
				case AT_RESULT_OK:
					if(strstr(pLanAT->pResponseData, "S00") != NULL){
						LAN_IsPHYLink = 1;
					}
					else if(strstr(pLanAT->pResponseData, "S01") != NULL){
						LAN_IsPHYLink = 0;
					}
					LAN_ConnectionState_Step++;
					break;
			}
			break;
		case 3:
			switch(AT_TransmitCommend(pLanAT, "S1", "S1", 2000, 0, true)) //DSR - TCP connection state
			{
				default:
					break;
				case AT_RESULT_ERROR:
				case AT_RESULT_TIMEOUT:
					LAN_ConnectionState_Step = 0;
					break;
				case AT_RESULT_OK:
					if(strstr(pLanAT->pResponseData, "S10") != NULL){
						LAN_IsTCPConnected = 1;
					}
					else if(strstr(pLanAT->pResponseData, "S11") != NULL){
						LAN_IsTCPConnected = 0;
					}
					LAN_ConnectionState_Step++;
					break;
			}
			break;
		default:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		LAN_ConnectionState_Step = 0;
	}

	return result != RESULT_RUN ? 1 : 0;
}

uint8_t LAN_Config(NetworkProperties_t *pNetProperties, LANOperatingMode_t Mode)
{
	static uint8_t LAN_Config_Step = 0;
	static uint32_t LAN_Config_Timer = 0;

	uint8_t result = RESULT_RUN;

	AT_Proc(pLanAT);

	switch(LAN_Config_Step)
	{
		case 0:
			LAN_IsInitialized = 0;

			if(LAN_SetATMode(1)){
				LAN_Config_Step++;
			}
			break;
		case 1:
			if(AT_TransmitCommend(pLanAT, "SC00", NULL, 1000, 0, true) == AT_RESULT_OK){
				LAN_Config_Step++;
				TMR_ResetNow(&LAN_Config_Timer);
			}
			break;
		case 2:
			if(LAN_SetNetworkProperties(pNetProperties)){
				LAN_Config_Step++;
			}
			break;
		case 3:
			if(LAN_SetOperationMode(Mode)){
				LAN_Config_Step++;
			}
			break;
		case 4:
			if(LAN_SetTCPConfig(LAN_TCP_KEEPALIVE_TIME)){
				LAN_Config_Step++;
			}
			break;
		case 5:
			if(AT_TransmitCommend(pLanAT, "SV", NULL, 500, 0, true) == AT_RESULT_OK){
				LAN_Config_Step++;
				TMR_ResetNow(&LAN_Config_Timer);
			}
			break;
		case 6:
			if(LAN_Reset(0, 2000)){
				LAN_Config_Step++;
			}
			break;
		case 7:
			if(LAN_GetNetworkProperties(pNetProperties)){
				if(pNetProperties->IsValid){
					LAN_Config_Step++;
				}
			}
			break;
		case 8:
			if(LAN_SetATMode(0)){
				LAN_Config_Step++;
			}
			break;
		default:
			result = RESULT_DONE;
			LAN_IsInitialized = 1;
			LAN_Config_Step = 0;
			break;
	}

	return result != RESULT_RUN ? 1 : 0;
}

int LAN_SocketConnected(void)
{
	return (int)LAN_IsPHYLink;
}

int LAN_Connect(char* Address, int Port)
{
	static uint8_t LAN_Connect_Step = 0;
	static uint32_t LAN_Connect_Timer = 0;

	int result = 0;
	char Buffer[300] = {0,};

	AT_Proc(pLanAT);

	if(!LAN_IsInitialized){
		LAN_Connect_Step = 0;
		return -1;
	}

	switch(LAN_Connect_Step)
	{
		case 0:
			if(LAN_SetATMode(1)){
				LAN_Connect_Step++;
			}
			break;
		case 1:
			switch(AT_TransmitCommend(pLanAT, "S0", "S0", 2000, 5, true))
			{
				default:
					break;
				case AT_RESULT_ERROR:
				case AT_RESULT_TIMEOUT:
					LAN_Connect_Step = 0;
					break;
				case AT_RESULT_OK:
					if(strstr(pLanAT->pResponseData, "1") != NULL){
						LAN_IsPHYLink = 0;
						result = -1;
					}
					else if(strstr(pLanAT->pResponseData, "0") != NULL){
						LAN_Connect_Step++;
						LAN_IsPHYLink = 1;
						TMR_ResetNow(&LAN_Connect_Timer);
					}
					break;
			}
			break;
		case 2:
			if(strlen(Address) == 4){
				sprintf(Buffer, "RH%d.%d.%d.%d", Address[0], Address[1], Address[2], Address[3]);
			}
			else if(strstr(Address, ".") != NULL){
				sprintf(Buffer, "RH%s", Address);
			}

			switch(AT_TransmitCommend(pLanAT, Buffer, NULL, 2000, 0, true))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					LAN_Connect_Step = 0;
					break;
				case AT_RESULT_OK:
					LAN_Connect_Step++;
					break;
			}
			break;
		case 3:
			sprintf(Buffer, "RP%d", Port);

			switch(AT_TransmitCommend(pLanAT, Buffer, NULL, 2000, 0, true))
			{
				default:
					break;
				case AT_RESULT_TIMEOUT:
					LAN_Connect_Step = 0;
					break;
				case AT_RESULT_OK:
					LAN_Connect_Step++;
					break;
			}
			break;
		case 4:
			if(LAN_SetATMode(0)){
				LAN_Connect_Step++;
			}
			break;
		case 5:
			if(TMR_Elapsed(&LAN_Connect_Timer, 2000)){
				LAN_Connect_Step++;
				result = 1;
			}
			break;
	}

	if(result != 0){
		LAN_Connect_Step = 0;
	}

	return result;
}

int LAN_WriteSocket(char *pBuffer, int Length, int Timeout)
{
	AT_Proc(pLanAT);

	if(!LAN_IsInitialized || LAN_IsATMode || Length<= 0){
		return -1;
	}

	if(AT_TransmitData(pLanAT, (uint8_t *)pBuffer, Length) == AT_RESULT_OK){
		return Length;
	}

	return 0;
}

int LAN_ReadSocket(char *pBuffer, int Length, int Timeout)
{
	int result = 0;

	AT_Proc(pLanAT);

	if(!LAN_IsInitialized || LAN_IsATMode || (int)AT_RECEIVE_BUFFER_COUNT(pLanAT) < Length || Timeout <= 0){
		return -1;
	}
	else if(pLanAT->IsRxBusy){
		return 0;
	}

	result = (int)AT_ReceiveCopyTo(pLanAT, (uint8_t *)pBuffer, (uint32_t)Length);

	AT_RECEIVE_BUFFER_REMOVE(pLanAT, result);

	return result;
}

uint8_t LAN_Init(ATCommend_t *pATCommend, NetworkProperties_t *pProperties, NetworkConnection_t *pConnection, LANOperatingMode_t Mode)
{
	static uint8_t LAN_Init_Step = 0;

	if(pATCommend == NULL|| pATCommend->pUART == NULL || pProperties == NULL){
		return RESULT_ERROR;
	}

	if(pConnection != NULL){
		pConnection->Connect = LAN_Connect;
		pConnection->Write = LAN_WriteSocket;
		pConnection->Read = LAN_ReadSocket;
		pConnection->IsConnected = LAN_SocketConnected;
	}

	pLanAT = pATCommend;

	switch (LAN_Init_Step) {
		case 0:
			pLanAT->IndexOfRxFirst = 0;
			pLanAT->IndexOfRxLast = 0;
			pLanAT->ResponseCallback = LAN_ATResponseCallback;
			pLanAT->ReceiveCallback = NULL;

			LAN_Init_Step++;
			break;
		case 1:
			if(LAN_Config(pProperties, Mode)){
				LAN_Init_Step++;
			}
			break;
		default:
			LAN_Init_Step = 0;
			 return RESULT_OK;
			break;
	}
	return RESULT_RUN;
}
