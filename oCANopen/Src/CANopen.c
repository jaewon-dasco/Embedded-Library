/*
 * Main CANopen stack file. It combines Object dictionary (CO_OD) and all other
 * CANopen source files. Configuration information are read from CO_OD.h file.
 *
 * @file        CANopen.c
 * @ingroup     CO_CANopen
 * @version     SVN: \$Id: CANopen.c 46 2013-08-24 09:18:16Z jani22 $
 * @author      Janez Paternoster
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



 // start CAN data out : id 00  data 01 00

/* Includes ------------------------------------------------------------------*/
#include <CANopen.h>
#include <CO_Common.h>

/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private variable ----------------------------------------------------------*/
/* Private function ----------------------------------------------------------*/


/* If defined, global variables will be used, otherwise CANopen objects will
   be generated with malloc(). */
/* #define CO_USE_GLOBALS */

#define CO_USE_GLOBALS

#ifndef CO_USE_GLOBALS
    #include <stdlib.h> /*  for malloc, free */
    static uint32_t CO_memoryUsed = 0; /* informative */
#endif


/**
 * Function CO_sendNMTcommand() is simple function, which sends CANopen message.
 * This part of code is an example of custom definition of simple CANopen
 * object. Follow the code in CANopen.c file. If macro CO_NO_NMT_MASTER is 1,
 * function CO_sendNMTcommand can be used to send NMT master message.
 *
 * @param CO CANopen object.
 * @param command NMT command.
 * @param nodeID Node ID.
 *
 * @return 0: Operation completed successfully.
 * @return other: same as CO_CANsend().
 */
#if CO_NO_NMT_MASTER == 1
    CO_CANtx_t *NMTM_txBuff = 0;
    /* Helper function for using: */
uint8_t CO_sendNMTcommand(CO_t *CO, uint8_t command, uint8_t nodeID)
{
        if(NMTM_txBuff == 0)
        {
            /* error, CO_CANtxBufferInit() was not called for this buffer. */
            return CO_ERROR_TX_UNCONFIGURED; /* -11 */
        }
        NMTM_txBuff->data[0] = command;
        NMTM_txBuff->data[1] = nodeID;
        return CO_CANsend(CO->CANmodule[0], NMTM_txBuff); /* 0 = success */
    }
#endif


/* CAN node ID - Object dictionary function ***********************************/
static int CO_ODF_nodeId(void *arg)
{
    uint8_t value;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;
    int ret = CO_SDO_AB_NONE;

    value = ODF_arg->data[0];

    if(!ODF_arg->reading)
    {
        if(value < 1U)
        {
            ret = CO_SDO_AB_VALUE_LOW;
        }
        else if(value > 127U)
        {
            ret = CO_SDO_AB_VALUE_HIGH;
        }
        else
        {
            ret = CO_SDO_AB_NONE;
        }
    }

    return ret;
}

/* CAN bit rate - Object dictionary function **********************************/
static int CO_ODF_bitRate(void *arg)
{
    uint16_t value;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;
    int ret = CO_SDO_AB_NONE;

    value = CO_getUint16(ODF_arg->data);

    if(!ODF_arg->reading)
    {
        switch(value)
        {
            case 10U:
            case 20U:
            case 50U:
            case 125U:
            case 250U:
            case 500U:
            case 800U:
            case 1000U:
                break;
            default:
                ret = CO_SDO_AB_INVALID_VALUE;
        }
    }

    return ret;
}

void CO_DefaultObject(CO_t *CO)
{
	if(CO->CommunicationObejct.producerHeartbeatTime == 0){
		CO->CommunicationObejct.producerHeartbeatTime = 300;
	}
}

/**
 * Initialize CANopen stack.
 *
 * Function must be called in the communication reset section.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_OUT_OF_MEMORY, CO_ERROR_ILLEGAL_BAUDRATE
 */
CO_ReturnError_t CO_init(CO_t *CO, CAN_HandleTypeDef *CANHandle, CO_OD_Callback_t ParameterSaveCallback)
{
    uint16_t i;
    CO_OD_EntryItem_t *object;
    uint8_t nodeId;
    CO_ReturnError_t err = CO_ERROR_PARAMETERS;
    

	CO->OD.CommunicationEntry.Length = ArrayLen(CO->OD.CommunicationODItems);
    CO->OD.CommunicationEntry.Items = (CO_OD_EntryItem_t *)CO->OD.CommunicationODItems;

	CO_OD_addVariable(&CO->OD, 0x1000, 0, 0x85, 4, &CO->CommunicationObejct.deviceType);
	CO_OD_addVariable(&CO->OD, 0x1001, 0, 0x46, 1, &CO->CommunicationObejct.errorRegister);  // CoS 감지 추가
	CO_OD_addVariable(&CO->OD, 0x1003, 8, 0x8E, 4, &CO->CommunicationObejct.preDefinedErrorField);
	CO_OD_addVariable(&CO->OD, 0x1005, 0, 0x8D, 4, &CO->CommunicationObejct.COB_ID_SYNCMessage);
	CO_OD_addVariable(&CO->OD, 0x1006, 0, 0x8D, 4, &CO->CommunicationObejct.communicationCyclePeriod);
	CO_OD_addVariable(&CO->OD, 0x1007, 0, 0x8D, 4, &CO->CommunicationObejct.synchronousWindowLength);
	CO_OD_addVariable(&CO->OD, 0x100A, 0, 0x85, 10, &CO->CommunicationObejct.ManufacturerSoftwareVersion);
	CO_OD_addVariable(&CO->OD, 0x100B, 0, 0x85, 10, &CO->CommunicationObejct.ManufacturerHardwareVersion);
	CO_OD_addVariable(&CO->OD, 0x1010, 1, 0x8E, 4, &CO->CommunicationObejct.storeParameters);
	CO_OD_addVariable(&CO->OD, 0x1011, 1, 0x8E, 4, &CO->CommunicationObejct.restoreDefaultParameters);
	CO_OD_addVariable(&CO->OD, 0x1014, 0, 0x85, 4, &CO->CommunicationObejct.COB_ID_EMCY);
	CO_OD_addVariable(&CO->OD, 0x1015, 0, 0x8D, 2, &CO->CommunicationObejct.inhibitTimeEMCY);
	CO_OD_addVariable(&CO->OD, 0x1017, 0, 0x8D, 2, &CO->CommunicationObejct.producerHeartbeatTime);
	CO_OD_addRecode(&CO->OD, 0x1018, 4, (CO_OD_Record_t *)&OD_recodeItem1018, &CO->CommunicationObejct.identity);
	CO_OD_addVariable(&CO->OD, 0x1019, 0, 0x0E, 1, &CO->CommunicationObejct.synchronousCounterOverflowValue);  // Writeable 추가
	CO_OD_addVariable(&CO->OD, 0x1029, 6, 0x8E, 1, &CO->CommunicationObejct.errorBehavior);  // Writeable 추가
	CO_OD_addRecode(&CO->OD, 0x1200, 3, (CO_OD_Record_t *)&OD_recodeItem1200, &CO->CommunicationObejct.SDOServerParameter);

	for(i=0; i<CO_NO_RPDO; i++){
		CO_OD_addRecode(&CO->OD, 0x1400+i, 2, (CO_OD_Record_t *)&OD_recodeItem1400, &CO->CommunicationObejct.RPDOCommunicationParameter[i]);
		CO_OD_addRecode(&CO->OD, 0x1600+i, 8, (CO_OD_Record_t *)&OD_recodeItem1600, &CO->CommunicationObejct.RPDOMappingParameter[i]);
	}

	for(i=0; i<CO_NO_TPDO; i++){
		CO_OD_addRecode(&CO->OD, 0x1800+i, 5, (CO_OD_Record_t *)&OD_recodeItem1800, &CO->CommunicationObejct.TPDOCommunicationParameter[i]);
		CO_OD_addRecode(&CO->OD, 0x1A00+i, 8, (CO_OD_Record_t *)&OD_recodeItem1A00, &CO->CommunicationObejct.TPDOMappingParameter[i]);
	}


	CO_OD_addVariable(&CO->OD, 0x1F80, 0, 0x85, 4, &CO->CommunicationObejct.NMTStartup);

	//NodeId Init
	if((object=CO_OD_find(&CO->OD, 0x2101)) == NULL){
		CO_OD_addVariable(&CO->OD, 0x2101, 0, 0x0F, 1, &CO->CommunicationObejct.NodeID);
	}
	else if(*((uint8_t *)CO_OD_getDataPointer(object, 0)) != 0){
		CO->CommunicationObejct.NodeID = *((uint8_t *)CO_OD_getDataPointer(object, 0));
	}
	else{
		*((uint8_t *)CO_OD_getDataPointer(object, 0)) = CO->CommunicationObejct.NodeID;
	}

	//BitRate Init
	if((object=CO_OD_find(&CO->OD, 0x2102)) == NULL){
		CO_OD_addVariable(&CO->OD, 0x2102, 0, 0x8F, 2, &CO->CommunicationObejct.BitRate);
	}
	else if(*((uint8_t *)CO_OD_getDataPointer(object, 0)) != 0){
		CO->CommunicationObejct.BitRate = *((uint8_t *)CO_OD_getDataPointer(object, 0));
	}
	else{
		*((uint8_t *)CO_OD_getDataPointer(object, 0)) = CO->CommunicationObejct.BitRate;
	}


    /* Read CANopen Node-ID and CAN bit-rate from object dictionary */
    err = CO_CANmodule_init(&CO->CANmodule, &CO->OD, CANHandle);

    if(err){CO_delete(); return err;}


    nodeId= CO->CommunicationObejct.NodeID;

    err = CO_SDO_init(
           &CO->SDO,
           CO_CAN_ID_RSDO + nodeId,
           CO_CAN_ID_TSDO + nodeId,
           OD_H1200_SDO_SERVER_PARAM,
           0,
		   &CO->OD,
           nodeId,
           &CO->CANmodule,
           &CO->CANmodule);

    if(err){CO_delete(); return err;}

    err = CO_EM_init(
           &CO->em,
           &CO->emPr,
           &CO->SDO,
           (uint8_t *)&CO->errorStatusBits,
           ArrayLen(CO->errorStatusBits),
           &CO->CommunicationObejct.errorRegister,
		   (uint32_t *)&CO->CommunicationObejct.preDefinedErrorField,
		   ArrayLen(CO->CommunicationObejct.preDefinedErrorField),
           &CO->CANmodule,
           CO_CAN_ID_EMERGENCY + nodeId);

    if(err){CO_delete(); return err;}

    err = CO_NMT_init(
            &CO->NMT,
            &CO->emPr,
            nodeId,
            100,
            &CO->CANmodule,
            CO_CAN_ID_NMT_SERVICE,
            &CO->CANmodule,
            CO_CAN_ID_HEARTBEAT + nodeId);

    if(err){CO_delete(); return err;}


#if CO_NO_NMT_MASTER == 1
    NMTM_txBuff = CO_CANtxBufferInit(/* return pointer to 8-byte CAN data buffer, which should be populated */
            CO->CANmodule[0], /* pointer to CAN module used for sending this message */
            CO_TXCAN_NMT,     /* index of specific buffer inside CAN module */
            0x0000,           /* CAN identifier */
            0,                /* rtr */
            2,                /* number of data bytes */
            0);               /* synchronous message flag bit */
#endif

    err = CO_SYNC_init(
            &CO->SYNC,
            &CO->em,
            &CO->SDO,
			&CO->NMT.operatingState,
            &CO->CANmodule,
            &CO->CANmodule);

    if(err){CO_delete(); return err;}

    /*
    err = CO_HBconsumer_init(
            &CO->HBcons,
            &CO->em,
            &CO->SDO,
			&OD_consumerHeartbeatTime[0],
            CO_HBcons_monitoredNodes,
            CO_NO_HB_CONS,
            &CO->CANmodule,
            CO_RXCAN_CONS_HB);

    if(err){CO_delete(); return err;}
	*/

#if CO_NO_SDO_CLIENT == 1
    err = CO_SDOclient_init(
            CO->SDOclient,
            CO->SDO,
            (CO_SDOclientPar_t*) &OD_SDOClientParameter[0],
            CO->CANmodule[0],
            CO_RXCAN_SDO_CLI,
            CO->CANmodule[0],
            CO_TXCAN_SDO_CLI);

    if(err){CO_delete(); return err;}
#endif


    /* Configure Object dictionary entry at index 0x2101 and 0x2102 */
    CO_OD_configure(&CO->OD, 0x1010, ParameterSaveCallback, 0, 0, 0);
    CO_OD_configure(&CO->OD, 0x1011, ParameterSaveCallback, 0, 0, 0);
    CO_OD_configure(&CO->OD, 0x2101, CO_ODF_nodeId, 0, 0, 0);
    CO_OD_configure(&CO->OD, 0x2102, CO_ODF_bitRate, 0, 0, 0);

    return CO_ERROR_NO;
}

/**
 * Delete CANopen object and free memory. Must be called at program exit.
 */
void CO_delete()
{
#ifndef CO_USE_GLOBALS
    int16_t i;
#endif

    //CO_CANsetConfigurationMode(ADDR_CAN1);  // taken care in CAN deinit
    //CO_CANmodule_disable(CO->CANmodule[0]);

#ifndef CO_USE_GLOBALS
  #if CO_NO_SDO_CLIENT == 1
    free(CO->SDOclient);
  #endif
    free(CO_HBcons_monitoredNodes);
    free(CO->HBcons);
    for(i=0; i<CO_NO_RPDO; i++){
        free(CO->RPDO[i]);
    }
    for(i=0; i<CO_NO_TPDO; i++){
        free(CO->TPDO[i]);
    }
    free(CO->SYNC);
    free(CO->NMT);
    free(CO->emPr);
    free(CO->em);
    free(CO_SDO_ODExtensions);
    free(CO->SDO);
    free(CO_CANmodule_txArray0);
    free(CO_CANmodule_rxArray0);
    free(CO->CANmodule[0]);
  #endif
}

/**
 * Process CANopen SYNC and RPDO objects.
 *
 * Function must be called cyclically from synchronous 1ms task. It processes
 * SYNC and receive PDO CANopen objects.
 *
 * @param CO This object
 */
void CO_process_RPDO(CO_t *CO)
{
	static int16_t i = 0;
    uint8_t SYNCret = 0;

    if(i == 0){
    	SYNCret = CO_SYNC_process(&CO->SYNC, CO->CommunicationObejct.synchronousWindowLength);
    }
    
    if(SYNCret == 2){
        CO_CANclearPendingSyncPDOs(&CO->CANmodule);
    }

    if(CO->RPDO[i].valid){
    	CO_RPDO_process(&CO->RPDO[i]);
    	i = (i+1)%CO_NO_RPDO;
    }
    else{
    	i = 0;
    }
}

/**
 * Process CANopen TPDO objects.
 *
 * Function must be called cyclically from synchronous 1ms task. It processes
 * transmit PDO CANopen objects.
 *
 * @param CO This object
 */
void CO_process_TPDO(CO_t *CO)
{
    static int16_t i = 0;

	if(CO->TPDO[i].valid){
		/* Verify PDO Change Of State and process PDOs */
		if(!CO->TPDO[i].sendRequest){
			CO->TPDO[i].sendRequest = CO_TPDOisCOS(&CO->TPDO[i]);
		}

		CO_TPDO_process(&CO->TPDO[i], &CO->SYNC);

		i = (i+1) %  CO_NO_TPDO;
	}
	else{
		i = 0;
	}
}

/**
 * Process CANopen objects.
 *
 * Function must be called cyclically. It processes all "asynchronous" CANopen
 * objects. Function returns value from CO_NMT_process().
 *
 * @param CO This object
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 *
 * @return #CO_NMT_reset_cmd_t
 */
CO_NMT_reset_cmd_t CO_process(CO_t *CO)
{
    uint8_t  NMTisPreOrOperational = 0;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    uint16_t timeDifference_100us = 0;
    uint16_t timeDifference_ms = 0;

    if(!CO->CANmodule.IsOpen){
    	return CO_RESET_NOT;
    }

    if(oTMR_Elapsed(&CO->Timestamp_ms, 1, TICKBASE_SYSTICK))
    {
        timeDifference_ms = oTMR_Interval(oTMR_GetTick(TICKBASE_SYSTICK), CO->Timestamp_ms);
        CO->Timestamp_ms += timeDifference_ms;
    }

    if(oTMR_Elapsed(&CO->Timestamp_us, 100, TICKBASE_CORECYCLE_US))
    {
        timeDifference_100us = oTMR_Interval(oTMR_GetTick(TICKBASE_CORECYCLE_US), CO->Timestamp_us) / 10;
        CO->Timestamp_us += timeDifference_100us * 10;
    }

    if(CO->NMT.operatingState == CO_NMT_PRE_OPERATIONAL || CO->NMT.operatingState == CO_NMT_OPERATIONAL)
    {
        NMTisPreOrOperational = 1;
    }

    /* Process CANopen objects */
    CO_process_RPDO(CO);
    CO_process_TPDO(CO);

    CO_SDO_process(
            &CO->SDO,
            NMTisPreOrOperational,
            timeDifference_ms,
            1000); // Time out changed due to change in calling rate 1000 --> 65535

    CO_EM_process(
            &CO->emPr,
            NMTisPreOrOperational,
            timeDifference_ms * 10,
			CO->CommunicationObejct.inhibitTimeEMCY);
    

    reset = CO_NMT_process(
            &CO->NMT,
            timeDifference_ms,
			CO->CommunicationObejct.producerHeartbeatTime,
			CO->CommunicationObejct.NMTStartup,
			CO->CommunicationObejct.errorRegister,
			CO->CommunicationObejct.errorBehavior);

    CO_NMT_blinkingProcess(&CO->NMT);

    /*
    CO_HBconsumer_process(
            &CO->HBcons,
            NMTisPreOrOperational,
            timeDifference_ms);
	*/
    return reset;
}
