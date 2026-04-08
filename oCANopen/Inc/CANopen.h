/**
 * Main CANopen stack file.
 *
 * It combines Object dictionary (CO_OD) and all other CANopen source files.
 * Configuration information are read from CO_OD.h file. This file may be
 * customized for different CANopen configuration. (One or multiple CANopen
 * device on one or multiple CAN modules.)
 *
 * @file        CANopen.h
 * @ingroup     CO_CANopen
 * @version     SVN: \$Id: CANopen.h 46 2013-08-24 09:18:16Z jani22 $
 * @author      Janez Paternoster
 * @author      Uwe Kindler
 * @copyright   2010 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CANopen_H
#define CANopen_H

/* Includes ------------------------------------------------------------------*/
#include "CO_Common.h"
#include "CO_Driver.h"
#include "CO_OD.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "CO_NMT_Heartbeat.h"
#include "CO_SYNC.h"
#include "CO_PDO.h"
#include "CO_HBconsumer.h"
   
 #if CO_NO_SDO_CLIENT == 1
  #include "CO_SDOmaster.h"
 #endif

#define CO_COMMUNICATION_OBJECT_INITIALIZER {\
	.COB_ID_SYNCMessage=CO_CAN_ID_SYNC,\
	.communicationCyclePeriod=300000,\
	.synchronousWindowLength=5000,\
	.inhibitTimeEMCY=100,\
	.producerHeartbeatTime=300,\
	.synchronousCounterOverflowValue=240,\
	.identity = {0xFFFFFF01, 1, 1000, 0},\
	.NMTStartup=1,\
	.NodeID=125,\
	.BitRate=250,\
	.RPDOCommunicationParameter = {[0 ... CO_NO_RPDO-1] = CO_OD_RPDO_COMMUNICATION_PARAMETER_INITIALIZER},\
	.TPDOCommunicationParameter = {[0 ... CO_NO_TPDO-1] = CO_OD_TPDO_COMMUNICATION_PARAMETER_INITIALIZER},\
}

#define CANOPEN_INITIALIZER(ext) {\
	.OD.ManufacturerEntry		= CO_OD_ENTRY_INITIALIZER(ext),\
	.CommunicationObejct		= CO_COMMUNICATION_OBJECT_INITIALIZER,\
}

typedef struct{
	/*1  - 1000*/ UNSIGNED32						deviceType;
	/*2  - 1001*/ UNSIGNED8							errorRegister;
	/*3  - 1003*/ UNSIGNED32						preDefinedErrorField[8];
	/*4  - 1005*/ UNSIGNED32						COB_ID_SYNCMessage;
	/*5  - 1006*/ UNSIGNED32						communicationCyclePeriod;
	/*6  - 1007*/ UNSIGNED32						synchronousWindowLength;
	/*7  - 100A*/ VISIBLE_STRING					ManufacturerSoftwareVersion[4];
	/*8  - 100B*/ VISIBLE_STRING					ManufacturerHardwareVersion[4];
	/*9  - 1010*/ UNSIGNED32						storeParameters;
	/*10 - 1011*/ UNSIGNED32						restoreDefaultParameters;
	/*12 - 1014*/ UNSIGNED32						COB_ID_EMCY;
	/*13 - 1015*/ UNSIGNED16     					inhibitTimeEMCY;
	/*14 - 1017*/ UNSIGNED16						producerHeartbeatTime;
	/*15 - 1018*/ OD_identity_t						identity;
	/*16 - 1019*/ UNSIGNED8							synchronousCounterOverflowValue; //1-240
	/*17 - 1029*/ UNSIGNED8							errorBehavior[6];
	/*18 - 1200*/ OD_SDOServerParameter_t			SDOServerParameter;

	/*19 - 1F80*/ UNSIGNED32						NMTStartup;					//0x00 = PreOperation | 0x01=Operation
	/*20 - 2101*/ UNSIGNED8      					NodeID;
	/*21 - 2102*/ UNSIGNED16     					BitRate;

	/*22 - 1400*/OD_RPDOCommunicationParameter_t	RPDOCommunicationParameter[CO_NO_RPDO];
	/*23 - 1600*/OD_RPDOMappingParameter_t 			RPDOMappingParameter[CO_NO_RPDO];
	/*24 - 1800*/OD_TPDOCommunicationParameter_t	TPDOCommunicationParameter[CO_NO_TPDO];
	/*25 - 1A00*/OD_TPDOMappingParameter_t 			TPDOMappingParameter[CO_NO_TPDO];
} CO_CommunicationObejct_t;

/**
 * CANopen stack object combines pointers to all CANopen objects.
 */
typedef struct{	
    CO_CANmodule_t		CANmodule;		/**< CAN module objects */
    CO_SDO_t			SDO;            /**< SDO object */
    CO_EM_t				em;             /**< Emergency report object */
    CO_EMpr_t			emPr;           /**< Emergency process object */
    CO_NMT_t			NMT;            /**< NMT object */
    CO_SYNC_t			SYNC;           /**< SYNC object */
    CO_RPDO_t			RPDO[CO_NO_RPDO];/**< RPDO objects */
    CO_TPDO_t			TPDO[CO_NO_TPDO];/**< TPDO objects */
    //CO_HBconsumer_t    HBcons;         /**<  Heartbeat consumer object*/

    CO_OD_t				OD;
#if CO_NO_SDO_CLIENT == 1
    CO_SDOclient_t     SDOclient;      /**< SDO client object */
#endif

    OCTET_STRING   		errorStatusBits[10];
    uint32_t 			Timestamp_us; /**< Timestamp in microseconds, updated by CO_process() */
    uint32_t 			Timestamp_ms; /**< Timestamp in milliseconds, updated by CO_process() */

    CO_CommunicationObejct_t CommunicationObejct;
}CO_t;

/* Exported variables -----------------------------------------------------------*/

/* Exported functions -----------------------------------------------------------*/

#if CO_NO_NMT_MASTER == 1
extern uint8_t CO_sendNMTcommand(CO_t *CO, uint8_t command, uint8_t nodeID);
#endif

extern CO_ReturnError_t CO_init(CO_t *CO, CAN_HandleTypeDef *CANHandle, CO_OD_Callback_t ParameterSaveCallback);
extern CO_NMT_reset_cmd_t CO_process(CO_t *CO);
extern void CO_delete(void);

#endif    /* CANopen_H */
