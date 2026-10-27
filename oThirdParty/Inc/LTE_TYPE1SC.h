#ifndef INC_LTE_TYPE1SC_H_
#define INC_LTE_TYPE1SC_H_

#define LTE_TYPE1SC_VERSION     0.1

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "ONE_Common.h"
#include "ONE_Time.h"
#include "ATCommend.h"
#include "Network.h"
/*************************************************************************************
 Define
 *************************************************************************************/
#define NIBBLE_TO_HEX_CHAR(i)   ((i <= 9) ? ('0' + i) : ('A' - 10 + i))
#define HIGH_NIBBLE(i)          ((i >> 4) & 0x0F)
#define LOW_NIBBLE(i)           (i & 0x0F)
#define HEX_CHAR_TO_NIBBLE(c)   ((c >= 'A') ? (c - 'A' + 0x0A) : (c - '0'))
#define HEX_PAIR_TO_BYTE(h, l)  ((HEX_CHAR_TO_NIBBLE(h) << 4) + HEX_CHAR_TO_NIBBLE(l))

/*************************************************************************************/
typedef enum{
	MQTT_TLS_Mutual  = 0,
	MQTT_TLS_Client  = 1,
	MQTT_TLS_Server  = 2,
	MQTT_TLS_Off     = 255,
} MQTT_TLS_t;

#pragma pack(1)
#pragma pack()

typedef void (*LTE_SubscribeCallback_t)(char*, uint8_t*, uint32_t);

/*************************************************************************************
 Function
 *************************************************************************************/
extern Result_t LTE_GetNetworkProperties(NetworkProperties_t *pProperties);
extern Result_t LTE_GetUrlIPAddress(char *URL, char *pIPAddress);
extern Result_t LTE_GetTime(DateAndTime_t *pDT);
extern Result_t LTE_GetRSSI(int16_t *pRSSI);
extern Result_t LTE_MQTTPublish(int QoS, char *Topic, char *pData, int SizeOfData);
extern Result_t LTE_MQTTSubscribe(int QoS, char *Topic, LTE_SubscribeCallback_t Callback);
extern Result_t LTE_MQTTConnect(char* ClientID, char *Address, uint16_t Port);
extern Result_t LTE_Init (ATCommend_t *pATCommend, NetworkProperties_t *pProperties,  NetworkConnection_t *pConnection);
#endif

/* History

2026-06-26 | v0.1
	- baseline (LTE_TYPE1SC.h)
*/
