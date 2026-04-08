/*
 * CANopen Service Data Object - server.
 *
 * @file        CO_SDO.c
 * @ingroup     CO_SDO
 * @version     SVN: \$Id: CO_SDO.c 46 2013-08-24 09:18:16Z jani22 $
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
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
#include <CO_Common.h>
#include <CO_Driver.h>
#include "CO_OD.h"
#include "CO_SDO.h"

/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* State machine, values of the SDO->state variable */
#define STATE_IDLE                     0x00U
#define STATE_DOWNLOAD_INITIATE        0x11U
#define STATE_DOWNLOAD_SEGMENTED       0x12U
#define STATE_DOWNLOAD_BLOCK_INITIATE  0x14U
#define STATE_DOWNLOAD_BLOCK_SUBBLOCK  0x15U
#define STATE_DOWNLOAD_BLOCK_END       0x16U
#define STATE_UPLOAD_INITIATE          0x21U
#define STATE_UPLOAD_SEGMENTED         0x22U
#define STATE_UPLOAD_BLOCK_INITIATE    0x24U
#define STATE_UPLOAD_BLOCK_INITIATE_2  0x25U
#define STATE_UPLOAD_BLOCK_SUBBLOCK    0x26U
#define STATE_UPLOAD_BLOCK_END         0x27U

/* Client command specifier, see DS301 */
#define CCS_DOWNLOAD_INITIATE          1U
#define CCS_DOWNLOAD_SEGMENT           0U
#define CCS_UPLOAD_INITIATE            2U
#define CCS_UPLOAD_SEGMENT             3U
#define CCS_DOWNLOAD_BLOCK             6U
#define CCS_UPLOAD_BLOCK               5U
#define CCS_ABORT                      0x80U

#if CO_SDO_BUFFER_SIZE < 7
    #error CO_SDO_BUFFER_SIZE must be greater than 7
#endif

/* Private variable ----------------------------------------------------------*/
/* Private function ----------------------------------------------------------*/
static void CO_SDO_abort(CO_SDO_t *SDO, uint32_t code);

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */

static void CO_SDO_receive(void *object, uint8_t dlc, uint8_t *pData)
{
    CO_SDO_t *SDO;

    SDO = (CO_SDO_t*)object;   /* this is the correct pointer type of the first argument */

    /* verify message length and message overflow (previous message was not processed yet) */
    if((dlc == 8U) && (!SDO->CANrxNew))
    {
    	memcpy(&SDO->CANrxData[0], pData, dlc);
        SDO->CANrxNew = 1;
    }
}

/*
 * Function for accessing _SDO server parameter_ (index 0x1200+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */

static int CO_ODF_1200(void *arg)
{
    uint8_t *nodeId;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;

    nodeId = (uint8_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    /* if SDO reading Object dictionary 0x1200, add nodeId to the value */
    if((ODF_arg->reading) && (ODF_arg->subIndex > 0U))
    {
        CO_setUint32(ODF_arg->data, value + *nodeId);
    }

    return ret;
}




/**
 * Initialize SDO object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SDO This object will be initialized.
 * @param COB_IDClientToServer 0x600 + nodeId by default.
 * @param COB_IDServerToClient 0x580 + nodeId by default.
 * @param ObjDictIndex_SDOServerParameter Index in Object dictionary.
 * @param parentSDO Pointer to SDO object, which contains object dictionary and
 * its extension. For first (default) SDO object this argument must be NULL.
 * If this argument is specified, then OD, ODSize and ODExtensions arguments
 * are ignored.
 * @param OD Pointer to @ref CO_SDO_objectDictionary array defined externally.
 * @param ODSize Size of the above array.
 * @param ODExtensions Pointer to the externaly defined array of the same size
 * as ODSize.
 * @param nodeId CANopen Node ID of this device. Value will be added to COB_IDs.
 * @param CANdevRx CAN device for SDO server reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for SDO server transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SDO_init(
        CO_SDO_t               *SDO,
        uint16_t                COB_IDClientToServer,
        uint16_t                COB_IDServerToClient,
        uint16_t                ObjDictIndex_SDOServerParameter,
        CO_SDO_t               *parentSDO,
		CO_OD_t		*ODEntryList,
        uint8_t                 nodeId,
        CO_CANmodule_t         *CANdevRx,
        CO_CANmodule_t         *CANdevTx)
{
	const CO_OD_EntryItem_t *object;
	OD_SDOServerParameter_t *Par = NULL;

	if((object=CO_OD_find(ODEntryList, 0x1200)) != NULL){
		Par = (OD_SDOServerParameter_t *)CO_OD_getDataPointer(object, 1);
	}

    /* configure own object dictionary */
    if(parentSDO == NULL)
    {
        uint16_t i;
        CO_OD_EntryItem_t *item;

        SDO->ownOD = 1;
        SDO->ODEntryList = ODEntryList;

        /* clear pointers in ODExtensions */
        for(i=0x1018U; i<0xA000; i++)
        {
        	if((item=CO_OD_find(SDO->ODEntryList, i)) != NULL){
        		item->extention.pODFunc = NULL;
        		item->extention.object = NULL;
        		item->extention.flags = NULL;
        	}
        }
    }
    /* copy object dictionary from parent */
    else
    {
        SDO->ownOD = 0;
        SDO->ODEntryList = parentSDO->ODEntryList;
    }

    /* Configure object variables */
    SDO->nodeId = nodeId;
    SDO->state = STATE_IDLE;
    SDO->CANrxNew = 0;

    /* Configure Object dictionary entry at index 0x1200 */
    if(ObjDictIndex_SDOServerParameter == OD_H1200_SDO_SERVER_PARAM)
    {
        CO_OD_configure(SDO->ODEntryList, ObjDictIndex_SDOServerParameter, CO_ODF_1200, (void*)&SDO->nodeId, 0U, 0U);
    }

    /* configure SDO server CAN reception */
    CO_CANrxBufferInit(
            CANdevRx,               /* CAN device */
            COB_IDClientToServer,   /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)SDO,             /* object passed to receive function */
            CO_SDO_receive);        /* this function will process received message */

    /* configure SDO server CAN transmission */
    SDO->CANdevTx = CANdevTx;
    SDO->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,               /* CAN device */
            COB_IDServerToClient,   /* CAN identifier */
            0,                      /* rtr */
            8,                      /* number of data bytes */
            0);                     /* synchronous message flag bit */

    if(Par != NULL){
        Par->COB_IDClientToServer = COB_IDClientToServer;
        Par->COB_IDServerToClient = COB_IDServerToClient;
        Par->NodeIDOfServerClient = nodeId;
    }

    return CO_ERROR_NO;
}


/**
 * Initialize SDO transfer.
 *
 * Find object in OD, verify, fill ODF_arg s.
 *
 * @param SDO This object.
 * @param index Index of the object in Object dictionary.
 * @param subIndex subIndex of the object in Object dictionary.
 *
 * @return 0 on success, otherwise #CO_SDO_abortCode_t.
 */
uint32_t CO_SDO_initTransfer(CO_SDO_t *SDO, uint16_t index, uint8_t subIndex)
{

    SDO->ODF_arg.index = index;
    SDO->ODF_arg.subIndex = subIndex;

    /* find object in Object Dictionary */
    SDO->entryItem = CO_OD_find(SDO->ODEntryList, index);
    if(SDO->entryItem == NULL)
    {
        return CO_SDO_AB_NOT_EXIST ;     /* object does not exist in OD */
    }

    /* verify existance of subIndex */
    if(subIndex > SDO->entryItem->maxSubIndex)
    {
        return CO_SDO_AB_SUB_UNKNOWN;     /* Sub-index does not exist. */
    }

    /* pointer to data in Object dictionary */
    SDO->ODF_arg.ODdataStorage = CO_OD_getDataPointer(SDO->entryItem, subIndex);

    /* fill ODF_arg */
    SDO->ODF_arg.object = NULL;
    if(SDO->entryItem->extention.pODFunc)
    {
        CO_OD_extension_t *ext = &SDO->entryItem->extention;
        SDO->ODF_arg.object = ext->object;
    }

    SDO->ODF_arg.data = SDO->databuffer;
    SDO->ODF_arg.dataLength = CO_OD_getLength(SDO->entryItem, subIndex);
    SDO->ODF_arg.attribute = CO_OD_getAttribute(SDO->entryItem, subIndex);
    SDO->ODF_arg.pFlags = CO_OD_getFlagsPointer(SDO->entryItem, subIndex);

    SDO->ODF_arg.firstSegment = 1;
    SDO->ODF_arg.lastSegment = 1;

    /* indicate total data length, if not domain */
    SDO->ODF_arg.dataLengthTotal = (SDO->ODF_arg.ODdataStorage) ? SDO->ODF_arg.dataLength : 0U;

    /* verify length */
    if(SDO->ODF_arg.dataLength > CO_SDO_BUFFER_SIZE)
    {
        return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
    }

    return 0U;
}

/**
 * Read data from @ref CO_SDO_objectDictionary to internal buffer.
 *
 * ODF_arg s must be initialized before with CO_SDO_initTransfer().
 * @ref CO_SDO_OD_function is called if configured.
 *
 * @param SDO This object.
 * @param SDOBufferSize Total size of the SDO buffer.
 *
 * @return 0 on success, otherwise #CO_SDO_abortCode_t.
 */
uint32_t CO_SDO_readOD(CO_SDO_t *SDO, uint16_t SDOBufferSize)
{
    uint8_t *SDObuffer = SDO->ODF_arg.data;
    uint8_t *ODdata = (uint8_t*)SDO->ODF_arg.ODdataStorage;
    uint16_t length = SDO->ODF_arg.dataLength;
    CO_OD_extension_t *ext = 0;


    /* is object readable? */
    if((SDO->ODF_arg.attribute & CO_ODA_READABLE) == 0)
        return CO_SDO_AB_WRITEONLY;     /* attempt to read a write-only object */

    /* find extension */
    if(SDO->entryItem != NULL)
    {
        ext = &SDO->entryItem->extention;
    }

    /* copy data from OD to SDO buffer if not domain */
    if(ODdata != NULL)
    {
    	CO_memcpy(SDObuffer, ODdata, length);
        //while(length--) *(SDObuffer++) = *(ODdata++);
    }
    /* if domain, Object dictionary function MUST exist */
    else
    {
        if(ext->pODFunc == NULL)
        {
            return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
        }
    }

    /* call Object dictionary function if registered */
    SDO->ODF_arg.reading = 1;
    if(ext->pODFunc != NULL)
    {
        uint32_t abortCode = ext->pODFunc(&SDO->ODF_arg);
        if(abortCode != 0U)
        {
            return abortCode;
        }

        /* dataLength (upadted by pODFunc) must be inside limits */
        if((SDO->ODF_arg.dataLength == 0U) || (SDO->ODF_arg.dataLength > SDOBufferSize))
        {
            return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
        }
    }
    SDO->ODF_arg.firstSegment = 0;

    /* swap data if processor is not little endian (CANopen is) */
#ifdef CO_BIG_ENDIAN
    if((SDO->ODF_arg.attribute & CO_ODA_MB_VALUE) != 0)
    {
        uint16_t len = SDO->ODF_arg.dataLength;
        uint8_t *buf1 = SDO->ODF_arg.data;
        uint8_t *buf2 = buf1 + len - 1;

        len /= 2;
        while(len--)
        {
            uint8_t b = *buf1;
            *(buf1++) = *buf2;
            *(buf2--) = b;
        }
    }
#endif

    return 0U;
}

/**
 * Write data from internal buffer to @ref CO_SDO_objectDictionary.
 *
 * ODF_arg s must be initialized before with CO_SDO_initTransfer().
 * @ref CO_SDO_OD_function is called if configured.
 *
 * @param SDO This object.
 * @param length Length of data (received from network) to write.
 *
 * @return 0 on success, otherwise #CO_SDO_abortCode_t.
 */
uint32_t CO_SDO_writeOD(CO_SDO_t *SDO, uint16_t length)
{	
    uint8_t *SDObuffer = SDO->ODF_arg.data;
    uint8_t *ODdata = (uint8_t*)SDO->ODF_arg.ODdataStorage;

    /* is object writeable? */
    if((SDO->ODF_arg.attribute & CO_ODA_WRITEABLE) == 0)
    {
        return CO_SDO_AB_READONLY;     /* attempt to write a read-only object */
    }

    /* length of domain data is application specific and not verified */
    if(ODdata == 0)
    {
        SDO->ODF_arg.dataLength = length;
    }

    /* verify length except for domain data type */
    else if(SDO->ODF_arg.dataLength != length)
    {
        return CO_SDO_AB_TYPE_MISMATCH;     /* Length of service parameter does not match */
    }

    /* swap data if processor is not little endian (CANopen is) */
#ifdef CO_BIG_ENDIAN
    if((SDO->ODF_arg.attribute & CO_ODA_MB_VALUE) != 0)
    {
        uint16_t len = SDO->ODF_arg.dataLength;
        uint8_t *buf1 = SDO->ODF_arg.data;
        uint8_t *buf2 = buf1 + len - 1;

        len /= 2;
        while(len--)
        {
            uint8_t b = *buf1;
            *(buf1++) = *buf2;
            *(buf2--) = b;
        }
    }
#endif

    /* call Object dictionary function if registered */
    SDO->ODF_arg.reading = 0;
    if(SDO->entryItem != NULL)
    {
        CO_OD_extension_t *ext = &SDO->entryItem->extention;

        if(ext->pODFunc != NULL)
        {
            uint32_t abortCode = ext->pODFunc(&SDO->ODF_arg);
            if(abortCode != 0U)
            {
                return abortCode;
            }
        }
    }
    SDO->ODF_arg.firstSegment = 0;

    /* copy data from SDO buffer to OD if not domain */
    if(ODdata != NULL)
    {
        CO_DISABLE_INTERRUPTS();
        while(length--)
        {	
            *(ODdata++) = *(SDObuffer++);
        }
 //       CO_ENABLE_INTERRUPTS();
    }

    return 0;
}

/******************************************************************************/
static void CO_SDO_abort(CO_SDO_t *SDO, uint32_t code)
{
    SDO->CANtxBuff->data[0] = 0x80;
    SDO->CANtxBuff->data[1] = SDO->ODF_arg.index & 0xFF;
    SDO->CANtxBuff->data[2] = (SDO->ODF_arg.index>>8) & 0xFF;
    SDO->CANtxBuff->data[3] = SDO->ODF_arg.subIndex;
    CO_memcpySwap4(&SDO->CANtxBuff->data[4], (uint8_t*)&code);
    SDO->state = STATE_IDLE;
    SDO->CANrxNew = 0;
    CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
}

/**
 * Process SDO communication.
 *
 * Function must be called cyclically.
 *
 * @param SDO This object.
 * @param NMTisPreOrOperational Different than zero, if #CO_NMT_internalState_t is
 * NMT_PRE_OPERATIONAL or NMT_OPERATIONAL.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 * @param SDOtimeoutTime Timeout time for SDO communication in milliseconds.
 *
 * @return 0: SDO server is iddle.
 * @return 1: SDO server is in transfer state.
 * @return -1: SDO abort just occured.
 */
int8_t CO_SDO_process(
        CO_SDO_t               *SDO,
        uint8_t                 NMTisPreOrOperational,
        uint16_t                timeDifference_ms,
        uint16_t                SDOtimeoutTime)
{
    uint8_t state = STATE_IDLE;
    uint8_t timeoutSubblockDownolad = 0;

    /* return if idle */
    if((SDO->state == STATE_IDLE) && (!SDO->CANrxNew))
    {
        return 0;
    }

    /* SDO is allowed to work only in operational or pre-operational NMT state */
    if(!NMTisPreOrOperational){
        SDO->state = STATE_IDLE;
        SDO->CANrxNew = 0;
        return 0;
    }

    /* Is something new to process? */
    if((!SDO->CANtxBuff->bufferFull) && ((SDO->CANrxNew) || (SDO->state == STATE_UPLOAD_BLOCK_SUBBLOCK)))
    {
        uint8_t CCS = SDO->CANrxData[0] >> 5;   /* Client command specifier */

        /* reset timeout */
        if(SDO->state != STATE_UPLOAD_BLOCK_SUBBLOCK)
            SDO->timeoutTimer = 0;

        /* clear response buffer */
        SDO->CANtxBuff->data[0] = SDO->CANtxBuff->data[1] = SDO->CANtxBuff->data[2] = SDO->CANtxBuff->data[3] = 0;
        SDO->CANtxBuff->data[4] = SDO->CANtxBuff->data[5] = SDO->CANtxBuff->data[6] = SDO->CANtxBuff->data[7] = 0;

        /* Is abort from client? */
        if((SDO->CANrxNew) && (SDO->CANrxData[0] == CCS_ABORT))
        {
            SDO->state = STATE_IDLE;
            SDO->CANrxNew = 0;
            return -1;
        }

        /* continue with previous SDO communication or start new */
        if(SDO->state != STATE_IDLE)
        {
            state = SDO->state;
        }
        else
        {
            uint32_t abortCode;

            /* Is client command specifier valid */
            if((CCS != CCS_DOWNLOAD_INITIATE) && (CCS != CCS_UPLOAD_INITIATE) &&
                (CCS != CCS_DOWNLOAD_BLOCK) && (CCS != CCS_UPLOAD_BLOCK))
                {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* init ODF_arg */
            abortCode = CO_SDO_initTransfer(SDO, (uint16_t)SDO->CANrxData[2]<<8 | SDO->CANrxData[1], SDO->CANrxData[3]);
            if(abortCode != 0U)
            {
                CO_SDO_abort(SDO, abortCode);
                return -1;
            }

            /* download */
            if((CCS == CCS_DOWNLOAD_INITIATE) || (CCS == CCS_DOWNLOAD_BLOCK))
            {
                if((SDO->ODF_arg.attribute & CO_ODA_WRITEABLE) == 0U)
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_READONLY); /* attempt to write a read-only object */
                    return -1;
                }

                /* set state machine to normal or block download */
                if(CCS == CCS_DOWNLOAD_INITIATE)
                {
                    state = STATE_DOWNLOAD_INITIATE;
                }
                else
                {
                    state = STATE_DOWNLOAD_BLOCK_INITIATE;
                }
            }

            /* upload */
            else
            {
                abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                if(abortCode != 0U)
                {
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* if data size is large enough set state machine to block upload, otherwise set to normal transfer */
                if((CCS == CCS_UPLOAD_BLOCK) && (SDO->ODF_arg.dataLength > SDO->CANrxData[5]))
                {
                    state = STATE_UPLOAD_BLOCK_INITIATE;
                }
                else
                {
                    state = STATE_UPLOAD_INITIATE;
                }
            }
        }
    }

    /* verify SDO timeout */
    if(SDO->timeoutTimer < SDOtimeoutTime)
    {
        SDO->timeoutTimer += timeDifference_ms;
    }
    if(SDO->timeoutTimer >= SDOtimeoutTime)
    {
        if((SDO->state == STATE_DOWNLOAD_BLOCK_SUBBLOCK) && (SDO->sequence != 0) && (!SDO->CANtxBuff->bufferFull))
        {
            timeoutSubblockDownolad = 1;
            state = SDO->state;
        }
        else
        {
            CO_SDO_abort(SDO, CO_SDO_AB_TIMEOUT); /* SDO protocol timed out */
            return -1;
        }
    }

    /* return immediately if still idle */
    if(state == STATE_IDLE)
    {
        return 0;
    }

    /* state machine (buffer is freed (SDO->CANrxNew = 0;) at the end) */
    switch(state)
    {
        uint32_t abortCode;
        uint16_t len, i;
        uint8_t err;

        case STATE_DOWNLOAD_INITIATE:
        {
            /* default response */
            SDO->CANtxBuff->data[0] = 0x60;
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* Expedited transfer */
            if((SDO->CANrxData[0] & 0x02U) != 0U)
            {
                /* is size indicated? Get message length */
                if((SDO->CANrxData[0] & 0x01U) != 0U)
                {
                    len = 4U - ((SDO->CANrxData[0] >> 2U) & 0x03U);
                }
                else
                {
                    len = 4U;
                }

                /* copy data to SDO buffer */
                SDO->ODF_arg.data[0] = SDO->CANrxData[4];
                SDO->ODF_arg.data[1] = SDO->CANrxData[5];
                SDO->ODF_arg.data[2] = SDO->CANrxData[6];
                SDO->ODF_arg.data[3] = SDO->CANrxData[7];

                /* write data to the Object dictionary */
                abortCode = CO_SDO_writeOD(SDO, len);
                if(abortCode != 0U)
                {
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* finish the communication */
                SDO->state = STATE_IDLE;
                CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            }

            /* Segmented transfer */
            else
            {
                /* verify length if size is indicated */
                if((SDO->CANrxData[0]&0x01) != 0)
                {
                    uint32_t lenRx;
                    CO_memcpySwap4((uint8_t*)&lenRx, &SDO->CANrxData[4]);
                    SDO->ODF_arg.dataLengthTotal = lenRx;

                    /* verify length except for domain data type */
                    if((lenRx != SDO->ODF_arg.dataLength) && (SDO->ODF_arg.ODdataStorage != 0))
                    {
                        CO_SDO_abort(SDO, CO_SDO_AB_TYPE_MISMATCH);  /* Length of service parameter does not match */
                        return -1;
                    }
                }
                SDO->bufferOffset = 0;
                SDO->sequence = 0;
                SDO->state = STATE_DOWNLOAD_SEGMENTED;
                CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            }
            break;
        }

        case STATE_DOWNLOAD_SEGMENTED:
        {
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE0) != 0x00U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify toggle bit */
            i = (SDO->CANrxData[0]&0x10U) ? 1U : 0U;
            if(i != SDO->sequence)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_TOGGLE_BIT);/* toggle bit not alternated */
                return -1;
            }

            /* get size of data in message */
            len = 7U - ((SDO->CANrxData[0] >> 1U) & 0x07U);

            /* verify length. Domain data type enables length larger than SDO buffer size */
            if((SDO->bufferOffset + len) > SDO->ODF_arg.dataLength)
            {
                if(SDO->ODF_arg.ODdataStorage != 0)
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_DATA_LONG);  /* Length of service parameter too high */
                    return -1;
                }
                else
                {
                    /* empty buffer in domain data type */
                    SDO->ODF_arg.lastSegment = 0;
                    abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                    if(abortCode != 0U)
                    {
                        CO_SDO_abort(SDO, abortCode);
                        return -1;
                    }

                    SDO->ODF_arg.dataLength = CO_SDO_BUFFER_SIZE;
                    SDO->bufferOffset = 0;
                }
            }

            /* copy data to buffer */
            for(i=0U; i<len; i++)
                SDO->ODF_arg.data[SDO->bufferOffset++] = SDO->CANrxData[i+1];

            /* If no more segments to be downloaded, write data to the Object dictionary */
            if((SDO->CANrxData[0] & 0x01U) != 0U)
            {
                SDO->ODF_arg.lastSegment = 1;
                abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                if(abortCode != 0U)
                {
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* finish */
                SDO->state = STATE_IDLE;
            }

            /* download segment response and alternate toggle bit */
            SDO->CANtxBuff->data[0] = 0x20 | (SDO->sequence ? 0x10 : 0x00);
            SDO->sequence = (SDO->sequence) ? 0 : 1;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case STATE_DOWNLOAD_BLOCK_INITIATE:
        {
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE1U) != 0xC0U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* prepare response */
            SDO->CANtxBuff->data[0] = 0xA4;
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* blksize */
            SDO->blksize = (CO_SDO_BUFFER_SIZE > (7*127)) ? 127 : (CO_SDO_BUFFER_SIZE / 7);
            SDO->CANtxBuff->data[4] = SDO->blksize;

            /* is CRC enabled */
            SDO->crcEnabled = (SDO->CANrxData[0] & 0x04) ? 1 : 0;
            SDO->crc = 0;

            /* verify length if size is indicated */
            if((SDO->CANrxData[0]&0x02) != 0U)
            {
                uint32_t lenRx;
                CO_memcpySwap4((uint8_t*)&lenRx, &SDO->CANrxData[4]);
                SDO->ODF_arg.dataLengthTotal = lenRx;

                /* verify length except for domain data type */
                if((lenRx != SDO->ODF_arg.dataLength) && (SDO->ODF_arg.ODdataStorage != 0))
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_TYPE_MISMATCH);  /* Length of service parameter does not match */
                    return -1;
                }
            }

            SDO->bufferOffset = 0;
            SDO->sequence = 0;
            SDO->state = STATE_DOWNLOAD_BLOCK_SUBBLOCK;

            /* send response */
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case STATE_DOWNLOAD_BLOCK_SUBBLOCK:
        {
            err = 1;

            /* no new message received, SDO timeout occured, try to response */
            if(timeoutSubblockDownolad)
            {
                SDO->CANrxData[0] = 0U;
            }
            else
            {
                /* if waiting for first segment and received wrong segment just ignore it */
                if((SDO->sequence == 0U) && ((SDO->CANrxData[0] & 0x7fU) != 1U))
                {
                    break;
                }

                /* previous segment is received again, ignore it */
                else if((SDO->CANrxData[0] & 0x7fU) == SDO->sequence)
                {
                    break;
                }

                /* correct segment is received, copy data */
                else if((SDO->CANrxData[0] & 0x7fU) == (SDO->sequence+1U))
                {
                    SDO->sequence++;
                    err = 0;
                    for(i=0U; i<7; i++)
                        SDO->ODF_arg.data[SDO->bufferOffset++] = SDO->CANrxData[i+1];
                }
            }

            /* send response if: wrong segment or last segment or end of transfer or timeout */
            if(err || (SDO->sequence >= SDO->blksize) || (SDO->CANrxData[0] & 0x80) || timeoutSubblockDownolad)
            {
                /* prepare response */
                SDO->CANtxBuff->data[0] = 0xA2;
                SDO->CANtxBuff->data[1] = SDO->sequence;
                SDO->sequence = 0;

                /* empty buffer in domain data type if not last segment */
                if((SDO->ODF_arg.ODdataStorage == 0) && (SDO->bufferOffset != 0) && ((SDO->CANrxData[0] & 0x80)) != 0)
                {
                    /* calculate CRC on next bytes, if enabled */
                    if(SDO->crcEnabled)
                    {
                        SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->bufferOffset, SDO->crc);
                    }

                    /* write data to the Object dictionary */
                    SDO->ODF_arg.lastSegment = 0;
                    abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                    if(abortCode != 0U)
                    {
                        CO_SDO_abort(SDO, abortCode);
                        return -1;
                    }

                    SDO->ODF_arg.dataLength = CO_SDO_BUFFER_SIZE;
                    SDO->bufferOffset = 0;
                }

                /* blksize */
                len = CO_SDO_BUFFER_SIZE - SDO->bufferOffset;
                SDO->blksize = (len > (7*127)) ? 127 : (len / 7);
                SDO->CANtxBuff->data[2] = SDO->blksize;

                /* last segment */
                if(((SDO->CANrxData[0] & 0x80) != 0) && !err)
                {
                    SDO->state = STATE_DOWNLOAD_BLOCK_END;
                }

                /* send response */
                CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            }

            /* don't clear SDO->CANrxNew flag on timeout */
            if(timeoutSubblockDownolad) return 1;

            break;
        }

        case STATE_DOWNLOAD_BLOCK_END:
        {
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE1U) != 0xC1U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* number of bytes in the last segment of the last block that do not contain data. */
            len = (SDO->CANrxData[0]>>2U) & 0x07U;
            SDO->bufferOffset -= len;

            /* calculate and verify CRC, if enabled */
            if(SDO->crcEnabled)
            {
                uint16_t crc;
                SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->bufferOffset, SDO->crc);

                CO_memcpySwap2((uint8_t*)&crc, &SDO->CANrxData[1]);

                if(SDO->crc != crc)
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_CRC);   /* CRC error (block mode only). */
                    return -1;
                }
            }

            /* write data to the Object dictionary */
            SDO->ODF_arg.lastSegment = 1;
            abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
            if(abortCode != 0U)
            {
                CO_SDO_abort(SDO, abortCode);
                return -1;
            }

            /* send response */
            SDO->CANtxBuff->data[0] = 0xA1;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            SDO->state = STATE_IDLE;
            break;
        }

        case STATE_UPLOAD_INITIATE:
        {
            /* default response */
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* Expedited transfer */
            if(SDO->ODF_arg.dataLength <= 4U)
            {
                for(i=0U; i<SDO->ODF_arg.dataLength; i++)
                    SDO->CANtxBuff->data[4U+i] = SDO->ODF_arg.data[i];

                SDO->CANtxBuff->data[0] = 0x43U | ((4U-SDO->ODF_arg.dataLength) << 2U);
                CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);

                /* finish */
                SDO->state = STATE_IDLE;
            }

            /* Segmented transfer */
            else
            {
                SDO->bufferOffset = 0U;
                SDO->sequence = 0U;
                SDO->state = STATE_UPLOAD_SEGMENTED;

                /* indicate data size, if known */
                if(SDO->ODF_arg.dataLengthTotal != 0U)
                {
                    uint32_t len = SDO->ODF_arg.dataLengthTotal;
                    CO_memcpySwap4(&SDO->CANtxBuff->data[4], (uint8_t*)&len);
                    SDO->CANtxBuff->data[0] = 0x41U;
                }
                else
                {
                    SDO->CANtxBuff->data[0] = 0x40U;
                }

                /* send response */
                CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            }
            break;
        }

        case STATE_UPLOAD_SEGMENTED:
        {
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE0U) != 0x60U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify toggle bit */
            i = ((SDO->CANrxData[0]&0x10U) != 0) ? 1U : 0U;
            if(i != SDO->sequence)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_TOGGLE_BIT);/* toggle bit not alternated */
                return -1;
            }

            /* calculate length to be sent */
            len = SDO->ODF_arg.dataLength - SDO->bufferOffset;
            if(len > 7U) len = 7U;

            /* If data type is domain, re-fill the data buffer if neccessary and indicated so. */
            if((SDO->ODF_arg.ODdataStorage == 0) && (len < 7U) && (!SDO->ODF_arg.lastSegment))
            {
                /* copy previous data to the beginning */
                for(i=0U; i<len; i++)
                {
                    SDO->ODF_arg.data[i] = SDO->ODF_arg.data[SDO->bufferOffset+i];
                }

                /* move the beginning of the data buffer */
                SDO->ODF_arg.data += len;
                SDO->ODF_arg.dataLength = CO_OD_getLength(SDO->entryItem, SDO->ODF_arg.subIndex) - len;

                /* read next data from Object dictionary function */
                abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                if(abortCode != 0U)
                {
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* return to the original data buffer */
                SDO->ODF_arg.data -= len;
                SDO->ODF_arg.dataLength +=  len;
                SDO->bufferOffset = 0;

                /* re-calculate the length */
                len = SDO->ODF_arg.dataLength;
                if(len > 7U) len = 7U;
            }

            /* fill response data bytes */
            for(i=0U; i<len; i++)
                SDO->CANtxBuff->data[i+1] = SDO->ODF_arg.data[SDO->bufferOffset++];

            /* first response byte */
            SDO->CANtxBuff->data[0] = 0x00 | (SDO->sequence ? 0x10 : 0x00) | ((7-len)<<1);
            SDO->sequence = (SDO->sequence) ? 0 : 1;

            /* verify end of transfer */
            if((SDO->bufferOffset == SDO->ODF_arg.dataLength) && (SDO->ODF_arg.lastSegment))
            {
                SDO->CANtxBuff->data[0] |= 0x01;
                SDO->state = STATE_IDLE;
            }

            /* send response */
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case STATE_UPLOAD_BLOCK_INITIATE:
        {
            /* default response */
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* calculate CRC, if enabled */
            if((SDO->CANrxData[0] & 0x04U) != 0U)
            {
                SDO->crcEnabled = 1;
                SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->ODF_arg.dataLength, 0);
            }
            else
            {
                SDO->crcEnabled = 0;
                SDO->crc = 0;
            }

            /* Number of segments per block */
            SDO->blksize = SDO->CANrxData[4];

            /* verify client subcommand and blksize */
            if(((SDO->CANrxData[0]&0x03U) != 0x00U) || (SDO->blksize < 1U) || (SDO->blksize > 127U))
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify if SDO data buffer is large enough */
            if(((SDO->blksize*7U) > SDO->ODF_arg.dataLength) && (!SDO->ODF_arg.lastSegment))
            {
                CO_SDO_abort(SDO, CO_SDO_AB_BLOCK_SIZE); /* Invalid block size (block mode only). */
                return -1;
            }

            /* indicate data size, if known */
            if(SDO->ODF_arg.dataLengthTotal != 0U)
            {
                uint32_t len = SDO->ODF_arg.dataLengthTotal;
                CO_memcpySwap4(&SDO->CANtxBuff->data[4], (uint8_t*)&len);
                SDO->CANtxBuff->data[0] = 0xC6U;
            }
            else
            {
                SDO->CANtxBuff->data[0] = 0xC4U;
            }

            /* send response */
            SDO->state = STATE_UPLOAD_BLOCK_INITIATE_2;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case STATE_UPLOAD_BLOCK_INITIATE_2:
        {
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE3U) != 0xA3U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            SDO->bufferOffset = 0;
            SDO->sequence = 0;
            SDO->endOfTransfer = 0;
            SDO->CANrxNew = 0;
            SDO->state = STATE_UPLOAD_BLOCK_SUBBLOCK;
            /* continue in next case */
        }

        case STATE_UPLOAD_BLOCK_SUBBLOCK:
        {
            /* is block confirmation received */
            if(SDO->CANrxNew)
            {
                uint8_t ackseq;
                uint16_t j;

                /* verify client command specifier and subcommand */
                if((SDO->CANrxData[0]&0xE3U) != 0xA2U)
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                    return -1;
                }

                ackseq = SDO->CANrxData[1];   /* sequence number of the last segment, that was received correctly. */

                /* verify if response is too early */
                if(ackseq > SDO->sequence)
                {
                    CO_SDO_abort(SDO, CO_SDO_AB_BLOCK_SIZE); /* Invalid block size (block mode only). */
                    return -1;
                }

                /* end of transfer */
                if((SDO->endOfTransfer) && (ackseq == SDO->blksize))
                {
                    /* first response byte */
                    SDO->CANtxBuff->data[0] = 0xC1 | ((7 - SDO->lastLen) << 2);

                    /* CRC */
                    if(SDO->crcEnabled)
                        CO_memcpySwap2(&SDO->CANtxBuff->data[1], (uint8_t*)&SDO->crc);

                    SDO->state = STATE_UPLOAD_BLOCK_END;

                    /* send response */
                    CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
                    break;
                }

                /* move remaining data to the beginning */
                for(i=ackseq*7, j=0; i<SDO->ODF_arg.dataLength; i++, j++)
                    SDO->ODF_arg.data[j] = SDO->ODF_arg.data[i];

                /* set remaining data length in buffer */
                SDO->ODF_arg.dataLength -= ackseq * 7U;

                /* new block size */
                SDO->blksize = SDO->CANrxData[2];

                /* If data type is domain, re-fill the data buffer if neccessary and indicated so. */
                if((SDO->ODF_arg.ODdataStorage == 0) && (SDO->ODF_arg.dataLength < (SDO->blksize*7U)) && (!SDO->ODF_arg.lastSegment))
                {
                    /* move the beginning of the data buffer */
                    len = SDO->ODF_arg.dataLength; /* length of valid data in buffer */
                    SDO->ODF_arg.data += len;
                    SDO->ODF_arg.dataLength = CO_OD_getLength(SDO->entryItem, SDO->ODF_arg.subIndex) - len;

                    /* read next data from Object dictionary function */
                    abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                    if(abortCode != 0U)
                    {
                        CO_SDO_abort(SDO, abortCode);
                        return -1;
                    }

                    /* calculate CRC on next bytes, if enabled */
                    if(SDO->crcEnabled)
                    {
                        SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->ODF_arg.dataLength, SDO->crc);
                    }

                  /* return to the original data buffer */
                    SDO->ODF_arg.data -= len;
                    SDO->ODF_arg.dataLength +=  len;
                }

                /* verify if SDO data buffer is large enough */
                if(((SDO->blksize*7U) > SDO->ODF_arg.dataLength) && (!SDO->ODF_arg.lastSegment)){
                    CO_SDO_abort(SDO, CO_SDO_AB_BLOCK_SIZE); /* Invalid block size (block mode only). */
                    return -1;
                }

                SDO->bufferOffset = 0U;
                SDO->sequence = 0U;
                SDO->endOfTransfer = 0;

                /* clear flag here */
                SDO->CANrxNew = 0;
            }

            /* return, if all segments was allready transfered or on end of transfer */
            if((SDO->sequence == SDO->blksize) || (SDO->endOfTransfer))
            {
                return 1;/* don't clear the SDO->CANrxNew flag, so return directly */
            }

            /* reset timeout */
            SDO->timeoutTimer = 0;

            /* calculate length to be sent */
            len = SDO->ODF_arg.dataLength - SDO->bufferOffset;
            if(len > 7U)
            {
                len = 7U;
            }

            /* fill response data bytes */
            for(i=0U; i<len; i++){
                SDO->CANtxBuff->data[i+1] = SDO->ODF_arg.data[SDO->bufferOffset++];
            }

            /* first response byte */
            SDO->CANtxBuff->data[0] = ++SDO->sequence;

            /* verify end of transfer */
            if((SDO->bufferOffset == SDO->ODF_arg.dataLength) && (SDO->ODF_arg.lastSegment))
            {
                SDO->CANtxBuff->data[0] |= 0x80;
                SDO->lastLen = len;
                SDO->blksize = SDO->sequence;
                SDO->endOfTransfer = 1;
            }

            /* send response */
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);

            /* don't clear the SDO->CANrxNew flag, so return directly */
            return 1;
        }

        case STATE_UPLOAD_BLOCK_END:
        {
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE1U) != 0xA1U)
            {
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            SDO->state = STATE_IDLE;
            break;
        }

        default:{
            CO_SDO_abort(SDO, CO_SDO_AB_DEVICE_INCOMPAT);/* general internal incompatibility in the device */
            return -1;
        }
    }

    /* free buffer */
    SDO->CANrxNew = 0;

    if(SDO->state != STATE_IDLE)
    {
        return 1;
    }

    return 0;
}
