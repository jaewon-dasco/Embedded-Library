#ifndef INC_LAN_WIZ750SR_H_
#define INC_LAN_WIZ750SR_H_

#include "main.h"
#include "Network.h"
#include "ATCommend.h"

//Typedef
typedef enum{
	LANOpMode_TCPClient = 0,
	LANOpMode_TCPServer,
	LANOpMode_TCPMix,
	LANOpMode_UDP,
}LANOperatingMode_t;
typedef enum{
	LANState_UNKNOW = 0,
	LANState_BOOT,
	LANState_OPEN,
	LANState_CONNECT,
	LANState_UPGRADE,
	LANState_ATMODE,
	LANState_UDP,
}LANStatus_t;

//Variable
extern uint8_t LAN_IsNewData;

//Function
extern uint8_t LAN_Init(ATCommend_t *pATCommend, NetworkProperties_t *pNetProperties, NetworkConnection_t *pNetConnection, LANOperatingMode_t Mode);
#endif
