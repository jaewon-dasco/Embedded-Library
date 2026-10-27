
/*
 * CAN module object for Microchip STM32F103 microcontroller.
 *
 * @file        CO_driver.c
 * @version     SVN: \$Id: co_driver.c 42 2013-07-09 11:14:12Z jani22 $
 * @author      Janez Paternoster
 * @author      Ondrej Netik
 * @copyright   2004 - 2013 Janez Paternoster, Ondrej Netik
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

/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "CO_Driver.h"
#include "CO_OD.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "CANOpen.h"
#include "ONE_CAN.h"
/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private variable ----------------------------------------------------------*/
/* Private function ----------------------------------------------------------*/

/******************************************************************************/
// Interrupt from Receiver
void CO_CANinterrupt_Rx(CO_CANmodule_t *CANmodule, oCANMessage_t* Message)
{        
        uint16_t index;
        uint8_t msgMatched = 0;
        CO_CANrx_t *msgBuff = CANmodule->rxArray;

        //uint16_t msg = (Message->ID << 2) | (Message->RTR ? 2 : 0);
		uint16_t msg = (Message->ID << 2);

        for (index = 0; index < ArrayLen(CANmodule->rxArray); index++)
        {
            if (((msg ^ msgBuff->ident) & msgBuff->mask) == 0)
            {
            	CANmodule->IsReceived = MATH_MIN(CANmodule->IsReceived+1, 254);
                msgMatched = 1;
                break;
            }
            msgBuff++;
        }
        
        //Call specific function, which will process the message
        if (msgMatched && msgBuff->pFunct){
        	msgBuff->pFunct(msgBuff->object, Message->DLC, (uint8_t *)&Message->Data);
        }
}

void oCANReceiveCallback(oCANMessage_t *Message, uint32_t Argument)
{
    if (Argument == 0) {
        return;
    }

    CO_TASK_SUSPEND;

    CO_CANinterrupt_Rx((CO_CANmodule_t *)Argument, Message);

    CO_TASK_RESUME;
}

/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, CO_OD_t	*ODEntryList, CAN_HandleTypeDef *CANHandle)
{
	int i;
	const CO_OD_EntryItem_t *object;
	uint8_t *nodeId = NULL;
    uint16_t *bitRate = NULL;
    //uint16_t index;

	CANmodule->bufferInhibitFlag = 0;
	CANmodule->firstCANtxMessage = 1;
	CANmodule->CANtxCount = 0;
	CANmodule->em = 0;

	for (i = 0; i < ArrayLen(CANmodule->rxArray); i++)
	{
		CANmodule->rxArray[i].ident = 0;
		CANmodule->rxArray[i].pFunct = 0;
	}
	for (i = 0; i < ArrayLen(CANmodule->txArray); i++)
	{
		CANmodule->txArray[i].bufferFull = 0;
	}

	if((object=CO_OD_find(ODEntryList, 0x2101)) != NULL){
		nodeId = (uint8_t *)CO_OD_getDataPointer(object, 0);
	}

	if((object=CO_OD_find(ODEntryList, 0x2102)) != NULL){
		bitRate = (uint16_t *)CO_OD_getDataPointer(object, 0);
	}

	if(nodeId == NULL || *nodeId == 0 || bitRate == NULL || *bitRate == 0){
		return CO_ERROR_PARAMETERS;
	}

    if(oCAN_Open(&CANmodule->pVxd, CAN1,  *bitRate) == RESULT_OK){
        oCAN_SetReceiveCallback(CANmodule->pVxd, oCANReceiveCallback, (uint32_t)CANmodule);
    }
    else{
        return CO_ERROR_PARAMETERS;
    }

    // Set CAN module parameters
    CANmodule->useCANrxFilters = 1;
    uint32_t IDs[4];

    IDs[0] = CO_CAN_ID_NMT_SERVICE;
    IDs[1] = CO_CAN_ID_SYNC;
    IDs[2] = CO_CAN_ID_TIME_STAMP;
    IDs[3] = CO_CAN_ID_RSDO | *nodeId;
    oCAN_SetIdFilter(CANmodule->pVxd, 0, IDs, 4, 0);

    CANmodule->IsOpen = 1;

	return CO_ERROR_NO;
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    oCAN_Close(CANmodule->pVxd);
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        uint16_t                mask,
        int8_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, uint8_t dlc, uint8_t *pData) )
{
    CO_CANrx_t *rxBuffer;	
    uint16_t RXF, RXM;

    //safety
    if (!CANmodule || !object || !pFunct || CANmodule->rxArraySize >= ArrayLen(CANmodule->rxArray))
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    //buffer, which will be configured
    rxBuffer = &CANmodule->rxArray[CANmodule->rxArraySize++];

    //Configure object variables
    rxBuffer->object = object;
    rxBuffer->pFunct = pFunct;

    //CAN identifier and CAN mask, bit aligned with CAN module registers
    RXF = (ident & 0x07FF) << 2;
    if (rtr) RXF |= 0x02;
    RXM = (mask & 0x07FF) << 2;
    RXM |= 0x02;

    //configure filter and mask
    if (RXF != rxBuffer->ident || RXM != rxBuffer->mask)
    {
        rxBuffer->ident = RXF;
        rxBuffer->mask = RXM;
    }

    return CO_ERROR_NO;
}

/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit ( CO_CANmodule_t	*CANmodule,
				uint16_t		ident,
				int8_t			rtr,
				uint8_t			noOfBytes,
				int8_t			syncFlag )
{
    uint32_t TXF;
    CO_CANtx_t *buffer;

    //safety
    if (!CANmodule || ArrayLen(CANmodule->txArray) <= CANmodule->txArraySize) return 0;

    //get specific buffer
    buffer = &CANmodule->txArray[CANmodule->txArraySize++];

    //CAN identifier, bit aligned with CAN module registers

    TXF = ident << 21;
    TXF &= 0xFFE00000;
    if (rtr) TXF |= 0x02;

    //write to buffer
    buffer->ident = TXF;
    buffer->DLC = noOfBytes;
    buffer->bufferFull = 0;
    buffer->syncFlag = syncFlag ? 1 : 0;

    return buffer;
}

/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    /* See generic driver for implemetation. */
}

/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule)
{
       /* See generic driver for implemetation. */       
}


/******************************************************************************/
static CO_ReturnError_t CO_CANsendToModule(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    oCANMessage_t oMsg = CAN_MESSAGE_INITIALIZER(0,8,0);

    if(!CANmodule || !buffer || buffer->DLC > 8 || buffer->DLC == 0){
        return CO_ERROR_DATA_CORRUPT;
    }

    if(oCAN_IsError(CANmodule->pVxd) == RESULT_OK){
    	CAN_HandleTypeDef *Handle = (CAN_HandleTypeDef *)oCAN_GetHandler(CANmodule->pVxd);
    	CANmodule->IsBusOff = Handle->ErrorCode & HAL_CAN_ERROR_BOF ? 1 : 0;
    	CANmodule->IsBusPassive = Handle->ErrorCode & HAL_CAN_ERROR_EPV ? 1 : 0;

    	return CO_ERROR_TX_UNCONFIGURED;
    }

    if(oCAN_IsMailBoxFull(CANmodule->pVxd) == RESULT_OK){
    	buffer->bufferFull = 1;
    	return CO_ERROR_TX_OVERFLOW;
    }

    CO_TASK_SUSPEND;

    oMsg.ID = (buffer->ident >> 21);
    oMsg.DLC = buffer->DLC;
    memcpy(&oMsg.Data, &buffer->data, 8);

    if(oCAN_Transmit(CANmodule->pVxd, &oMsg) == RESULT_OK){
    	CANmodule->IsTransmitted = MATH_MIN(CANmodule->IsTransmitted+1, 254);
    }

	buffer->bufferFull = 0;
	CANmodule->IsBusOff = 0;
	CANmodule->IsBusPassive = 0;
	CANmodule->CANtxCount++;

    CO_TASK_SUSPEND;

    return CO_ERROR_NO;
}

/******************************************************************************/
// Interrupt from Transeiver
void CO_CANinterrupt_Tx(CO_CANmodule_t *CANmodule)
{
    /* First CAN message (bootup) was sent successfully */
   CANmodule->firstCANtxMessage = 0;
   /* clear flag from previous message */
   CANmodule->bufferInhibitFlag = 0;
   /* Are there any new messages waiting to be send */
   if(CANmodule->CANtxCount > 0)
   {
       uint16_t i;             /* index of transmitting message */

       /* first buffer */
       CO_CANtx_t *buffer = CANmodule->txArray;
       /* search through whole array of pointers to transmit message buffers. */
       for(i = ArrayLen(CANmodule->txArray); i > 0; i--)
       {
           /* if message buffer is full, send it. */
           if(buffer->bufferFull)
           {
               buffer->bufferFull = 0;
               CANmodule->CANtxCount--;
               /* Copy message to CAN buffer */
               CANmodule->bufferInhibitFlag = buffer->syncFlag;
               CO_CANsendToModule(CANmodule, buffer);
               break;                      /* exit for loop */
           }
           buffer++;
       }/* end of for loop */

       /* Clear counter if no more messages */
       if(i == 0) CANmodule->CANtxCount = 0;
   }

}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;

    err = CO_CANsendToModule(CANmodule, buffer);

    if(err == CO_ERROR_NO){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
    }
    else if(err == CO_ERROR_TX_OVERFLOW && buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage)/* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
    }

    return err;
}/******************************************************************************/


/* CO- VJ Change End */

