/*
 * CANopen Process Data Object.
 *
 * @file        CO_PDO.c
 * @ingroup     CO_PDO
 * @version     SVN: \$Id: CO_PDO.c 46 2013-08-24 09:18:16Z jani22 $
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
#include <CANopen.h>
#include <CO_Common.h>
#include <CO_Driver.h>
#include "CO_OD.h"
#include "CO_NMT_Heartbeat.h"
#include "CO_Emergency.h"
#include "CO_SYNC.h"
#include "CO_PDO.h"
#include "ONE_Time.h"

/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private variable ----------------------------------------------------------*/
/* Private function ----------------------------------------------------------*/
static int16_t CO_TPDOsend(CO_TPDO_t *TPDO);
static void CO_RPDOconfigCom(CO_RPDO_t* RPDO, uint32_t COB_IDUsedByRPDO);
static void CO_TPDOconfigCom(CO_TPDO_t* TPDO, uint32_t COB_IDUsedByTPDO, uint8_t syncFlag);
static uint32_t CO_RPDOconfigMap(CO_RPDO_t* RPDO, uint8_t noOfMappedObjects);
static uint32_t CO_TPDOconfigMap(CO_TPDO_t* TPDO, uint8_t noOfMappedObjects);

static uint32_t CO_PDOfindMap(
        CO_SDO_t               *SDO,
        uint32_t                map,
        uint8_t                 R_T,
        uint8_t               **ppData,
        uint8_t                *pLength,
        uint8_t                *pSendIfCOSFlags);
/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_PDO_receive(void *object, uint8_t dlc, uint8_t *pData)
{
    CO_RPDO_t *RPDO;

    RPDO = (CO_RPDO_t*)object;   /* this is the correct pointer type of the first argument */

    if( (RPDO->valid) &&
        (*RPDO->operatingState == CO_NMT_OPERATIONAL) &&
        (dlc >= RPDO->dataLength) &&
        (dlc <= 8U) &&
        (!RPDO->CANrxNew))
    {
        /* copy data and set 'new message' flag */
    	memcpy(&RPDO->CANrxData, pData, dlc);

        RPDO->CANrxNew = 1;
    }
}

/*
 * Configure RPDO Communication parameter.
 *
 * Function is called from commuincation reset or when parameter changes.
 *
 * Function configures following variable from CO_RPDO_t: _valid_. It also
 * configures CAN rx buffer. If configuration fails, emergency message is send
 * and device is not able to enter NMT operational.
 *
 * @param RPDO RPDO object.
 * @param COB_IDUsedByRPDO _RPDO communication parameter_, _COB-ID for PDO_ variable
 * from Object dictionary (index 0x1400+, subindex 1).
 */
static void CO_RPDOconfigCom(CO_RPDO_t* RPDO, uint32_t COB_IDUsedByRPDO)
{
    uint16_t ID;
    CO_ReturnError_t r;

    ID = (uint16_t)COB_IDUsedByRPDO;

    /* is RPDO used? */
    if((COB_IDUsedByRPDO & 0xBFFFF800L) == 0 && RPDO->dataLength && ID)
    {
        /* is used default COB-ID? */
        if(ID == RPDO->defaultCOB_ID) ID += RPDO->nodeId;
        RPDO->valid = 1;
    }
    else{
        ID = 0;
        RPDO->valid = 0;
        RPDO->CANrxNew = 0;
    }
    r = CO_CANrxBufferInit(
            RPDO->CANdevRx,         /* CAN device */
            ID,                     /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)RPDO,            /* object passed to receive function */
            CO_PDO_receive);        /* this function will process received message */
    if(r != CO_ERROR_NO)
    {
        RPDO->valid = 0;
        RPDO->CANrxNew = 0;
    }
}

/*
 * Configure TPDO Communication parameter.
 *
 * Function is called from commuincation reset or when parameter changes.
 *
 * Function configures following variable from CO_TPDO_t: _valid_. It also
 * configures CAN tx buffer. If configuration fails, emergency message is send
 * and device is not able to enter NMT operational.
 *
 * @param TPDO TPDO object.
 * @param COB_IDUsedByTPDO _TPDO communication parameter_, _COB-ID for PDO_ variable
 * from Object dictionary (index 0x1400+, subindex 1).
 * @param syncFlag Indicate, if TPDO is synchronous.
 */
static void CO_TPDOconfigCom(CO_TPDO_t* TPDO, uint32_t COB_IDUsedByTPDO, uint8_t syncFlag)
{
    uint16_t ID;

    ID = (uint16_t)COB_IDUsedByTPDO;

    /* is TPDO used? */
    if((COB_IDUsedByTPDO & 0xBFFFF800L) == 0 && TPDO->dataLength && ID)
    {
        /* is used default COB-ID? */
        if(ID == TPDO->defaultCOB_ID) ID += TPDO->nodeId;
        TPDO->valid = 1;
    }
    else
    {
        ID = 0;
        TPDO->valid = 0;
    }

    TPDO->CANtxBuff = CO_CANtxBufferInit(
            TPDO->CANdevTx,            /* CAN device */
            ID,                        /* CAN identifier */
            0,                         /* rtr */
            TPDO->dataLength,          /* number of data bytes */
            syncFlag);                 /* synchronous message flag bit */

    if(TPDO->CANtxBuff == 0)
    {
        TPDO->valid = 0;
    }
}

/*
 * Find mapped variable in Object Dictionary.
 *
 * Function is called from CO_R(T)PDOconfigMap or when mapping parameter changes.
 *
 * @param SDO SDO object.
 * @param map PDO mapping parameter.
 * @param R_T 0 for RPDO map, 1 for TPDO map.
 * @param ppData Pointer to returning parameter: pointer to data of mapped variable.
 * @param pLength Pointer to returning parameter: *add* length of mapped variable.
 * @param pSendIfCOSFlags Pointer to returning parameter: sendIfCOSFlags variable.
 * @param pIsMultibyteVar Pointer to returning parameter: 1 for multibyte variable.
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_PDOfindMap(
        CO_SDO_t               *SDO,
        uint32_t                map,
        uint8_t                 R_T,
        uint8_t               **ppData,
        uint8_t                *pLength,
        uint8_t                *pSendIfCOSFlags)
{
	const CO_OD_EntryItem_t *object;
    uint16_t index;
    uint8_t subIndex;
    uint8_t dataLen;
    uint8_t objectLen;
    uint8_t attr;

    index = (uint16_t)(map>>16);
    subIndex = (uint8_t)(map>>8);
    dataLen = (uint8_t) map;   /* data length in bits */

    /* data length must be byte aligned */
    if(dataLen&0x07) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    dataLen >>= 3;    /* new data length is in bytes */
    *pLength += dataLen;

    /* total PDO length can not be more than 8 bytes */
    if(*pLength > 8) return CO_SDO_AB_MAP_LEN;  /* The number and length of the objects to be mapped would exceed PDO length. */

    /* is there a reference to dummy entries */
    if(index <=7 && subIndex == 0)
    {
        static uint32_t dummyTX = 0;
        static uint32_t dummyRX = 0;

        /* is size of variable big enough for map */
        if(dataLen > 4) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

        /* Data and ODE pointer */
        if(R_T == 0) *ppData = (uint8_t*) &dummyRX;
        else         *ppData = (uint8_t*) &dummyTX;

        return 0;
    }

    /* find object in Object Dictionary */
    object = CO_OD_find(SDO->ODEntryList, index);

    /* Does object exist in OD? */
    if(object == NULL || subIndex > object->maxSubIndex)
        return CO_SDO_AB_NOT_EXIST;   /* Object does not exist in the object dictionary. */

    attr = CO_OD_getAttribute(object, subIndex);
    if(R_T==0 && !(attr&(CO_ODA_RPDO_MAPABLE|CO_ODA_RPDO_MAPABLE) && attr&CO_ODA_WRITEABLE)) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    /* Is object Mappable for RPDO? */
    //if(R_T==0 && !(attr&(CO_ODA_RPDO_MAPABLE|CO_ODA_RPDO_MAPABLE) && attr&CO_ODA_WRITEABLE)) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */
    /* Is object Mappable for TPDO? */
    //if(R_T!=0 && !(attr&CO_ODA_TPDO_MAPABLE && attr&CO_ODA_READABLE)) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    /* is size of variable big enough for map */
    objectLen = CO_OD_getLength(object, subIndex);
    if(objectLen < dataLen) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    /* pointer to data */
    *ppData = (uint8_t*) CO_OD_getDataPointer(object, subIndex);

    /* setup change of state flags */
    if(attr&CO_ODA_TPDO_DETECT_COS)
    {
        int16_t i;
        for(i=*pLength-dataLen; i<*pLength; i++)
        {
            *pSendIfCOSFlags |= 1<<i;
        }
    }

    return 0;
}

/*
 * Configure RPDO Mapping parameter.
 *
 * Function is called from communication reset or when parameter changes.
 *
 * Function configures following variables from CO_RPDO_t: _dataLength_ and
 * _mapPointer_.
 *
 * @param RPDO RPDO object.
 * @param noOfMappedObjects Number of mapped object (from OD).
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_RPDOconfigMap(CO_RPDO_t* RPDO, uint8_t noOfMappedObjects)
{
    int16_t i;
    uint8_t length = 0;
    uint32_t ret = 0;
    const uint32_t* pMap = &RPDO->RPDOMapPar->mappedObject1;

    if(!RPDO->valid){
    	return ret;
    }

    for(i=noOfMappedObjects; i>0; i--)
    {
        int16_t j;
        uint8_t* pData;
        uint8_t dummy = 0;
        uint8_t prevLength = length;
        uint32_t map = *(pMap++);

        /* function do much checking of errors in map */
        ret = CO_PDOfindMap(RPDO->SDO, map, 1, &pData, &length, &dummy);

        if(ret)
        {
            length = 0;
            CO_errorReport(RPDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /* write PDO data pointers */
        for(j=prevLength; j<length; j++)
		{
			RPDO->mapPointer[j] = pData++;
		}

    }

    RPDO->dataLength = length;

    return ret;
}

/*
 * Configure TPDO Mapping parameter.
 *
 * Function is called from communication reset or when parameter changes.
 *
 * Function configures following variables from CO_TPDO_t: _dataLength_,
 * _mapPointer_ and _sendIfCOSFlags_.
 *
 * @param TPDO TPDO object.
 * @param noOfMappedObjects Number of mapped object (from OD).
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_TPDOconfigMap(CO_TPDO_t* TPDO, uint8_t noOfMappedObjects)
{
    int16_t i;
    uint8_t length = 0;
    uint32_t ret = 0;
    const uint32_t* pMap = &TPDO->TPDOMapPar->mappedObject1;

    TPDO->sendIfCOSFlags = 0;

    for(i=noOfMappedObjects; i>0; i--)
    {
        int16_t j;
        uint8_t* pData;
        uint8_t prevLength = length;
        uint32_t map = *(pMap++);

        /* function do much checking of errors in map */
        ret = CO_PDOfindMap(
                TPDO->SDO,
                map,
                1,
                &pData,
                &length,
                &TPDO->sendIfCOSFlags);
        if(ret)
        {
            length = 0;
            CO_errorReport(TPDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /* write PDO data pointers */
        for(j=prevLength; j<length; j++)
		{
			TPDO->mapPointer[j] = pData++;
		}

    }

    TPDO->dataLength = length;

    return ret;
}

/*
 * Function for accessing _RPDO communication parameter_ (index 0x1400+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static int CO_ODF_RPDOcom(void *arg)
{
    CO_RPDO_t *RPDO;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t*)arg;
    RPDO = (CO_RPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading)
    {
        if(ODF_arg->subIndex == 1)
        {
            uint32_t *value = (uint32_t*) ODF_arg->data;

            /* if default COB ID is used, write default value here */
            if(((*value)&0xFFFF) == RPDO->defaultCOB_ID && RPDO->defaultCOB_ID)
                *value += RPDO->nodeId;

            /* If PDO is not valid, set bit 31 */
            if(!RPDO->valid) *value |= 0x80000000L;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(RPDO->restrictionFlags & 0x04)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
	
    if(*RPDO->operatingState == CO_NMT_OPERATIONAL && (RPDO->restrictionFlags & 0x01))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */

    if(ODF_arg->subIndex == 1){   /* COB_ID */
        uint32_t *value = (uint32_t*) ODF_arg->data;

        /* bits 11...29 must be zero */
        if(*value & 0x3FFF8000L)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if(((*value)&0xFFFF) == (RPDO->defaultCOB_ID + RPDO->nodeId)){
            *value &= 0xC0000000L;
            *value += RPDO->defaultCOB_ID;
        }

        /* if PDO is valid, bits 0..29 can not be changed */
        if(RPDO->valid && ((*value ^ RPDO->RPDOCommPar->COB_IDUsedByRPDO) & 0x3FFFFFFFL))
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* configure RPDO */
        CO_RPDOconfigCom(RPDO, *value);
    }
    else if(ODF_arg->subIndex == 2){   /* Transmission_type */
        uint8_t *value = (uint8_t*) ODF_arg->data;

        /* values from 241...253 are not valid */
        if(*value >= 241 && *value <= 253)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
    }

    return CO_SDO_AB_NONE;
}

/*
 * Function for accessing _TPDO communication parameter_ (index 0x1800+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static int CO_ODF_TPDOcom(void *arg)
{
    CO_TPDO_t *TPDO;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t*)arg;
    TPDO = (CO_TPDO_t*) ODF_arg->object;

    if(ODF_arg->subIndex == 4) return CO_SDO_AB_SUB_UNKNOWN;  /* Sub-index does not exist. */

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading)
    {
        if(ODF_arg->subIndex == 1)
        {   /* COB_ID */
            uint32_t *value = (uint32_t*) ODF_arg->data;

            /* if default COB ID is used, write default value here */
            if(((*value)&0xFFFF) == TPDO->defaultCOB_ID && TPDO->defaultCOB_ID)
                *value += TPDO->nodeId;

            /* If PDO is not valid, set bit 31 */
            if(!TPDO->valid) *value |= 0x80000000L;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(TPDO->restrictionFlags & 0x04)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
	
    if(*TPDO->operatingState == CO_NMT_OPERATIONAL && (TPDO->restrictionFlags & 0x01))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */

    if(ODF_arg->subIndex == 1)
    {   /* COB_ID */
        uint32_t *value = (uint32_t*) ODF_arg->data;

        /* bits 11...29 must be zero */
        if(*value & 0x3FFF8000L)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if(((*value)&0xFFFF) == (TPDO->defaultCOB_ID + TPDO->nodeId))
        {
            *value &= 0xC0000000L;
            *value += TPDO->defaultCOB_ID;
        }

        /* if PDO is valid, bits 0..29 can not be changed */
        if(TPDO->valid && ((*value ^ TPDO->TPDOCommPar->COB_IDUsedByTPDO) & 0x3FFFFFFFL))
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* configure TPDO */
        CO_TPDOconfigCom(TPDO, *value, TPDO->CANtxBuff->syncFlag);
        TPDO->syncCounter = 255;
    }
    else if(ODF_arg->subIndex == 2)
    {   /* Transmission_type */
        uint8_t *value = (uint8_t*) ODF_arg->data;

        /* values from 241...253 are not valid */
        if(*value >= 241 && *value <= 253 && *value != 0)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
        
        TPDO->CANtxBuff->syncFlag = (*value <= 240) ? 1 : 0;
        TPDO->syncCounter = 255;
    }
    else if(ODF_arg->subIndex == 3)
    {   /* Inhibit_Time */  //edit by JONE
        uint16_t *value = (uint16_t*) ODF_arg->data;        
        TPDO->inhibitTimer = *value * 100;
    }
    else if(ODF_arg->subIndex == 5)
    {   /* Event_Timer */
        uint16_t *value = (uint16_t*) ODF_arg->data;
        TPDO->eventTimer = *value;
    }
    else if(ODF_arg->subIndex == 6)
    {   /* SYNC start value */
        uint8_t *value = (uint8_t*) ODF_arg->data;

        /* if PDO is valid, value can not be changed */
        if(TPDO->valid)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* values from 240...255 are not valid */
        if(*value > 240)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
    }

    return CO_SDO_AB_NONE;
}

/*
 * Function for accessing _RPDO mapping parameter_ (index 0x1600+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static int CO_ODF_RPDOmap(void *arg)
{
    CO_RPDO_t *RPDO;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;

    RPDO = (CO_RPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading)
    {
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(ODF_arg->subIndex == 0)
        {
            /* If there is error in mapping, dataLength is 0, so numberOfMappedObjects is 0. */
            if(!RPDO->dataLength) *value = 0;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(RPDO->restrictionFlags & 0x08)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    if(*RPDO->operatingState == CO_NMT_OPERATIONAL && (RPDO->restrictionFlags & 0x02))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    if(RPDO->valid)
        return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

    /* numberOfMappedObjects */
    if(ODF_arg->subIndex == 0)
    {
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(*value > 8)
            return CO_SDO_AB_VALUE_HIGH;  /* Value of parameter written too high. */

        /* configure mapping */
        return(CO_SDO_abortCode_t)CO_RPDOconfigMap(RPDO, *value);    // VJ to remove warnings // modify 20151006 jack (CO_SDO_abortCode_t)
    }

    /* mappedObject */
    else
    {
        uint32_t *value = (uint32_t*) ODF_arg->data;
        uint8_t* pData;
        uint8_t length = 0;
        uint8_t dummy = 0;

        if(RPDO->dataLength)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* verify if mapping is correct */
        return (int)CO_PDOfindMap(RPDO->SDO, *value, 0, &pData, &length, &dummy);
    }

   return CO_SDO_AB_NONE; // modify 20151006 jack (CO_SDO_abortCode_t)
}

/*
 * Function for accessing _TPDO mapping parameter_ (index 0x1A00+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static int CO_ODF_TPDOmap(void *arg)
{
    CO_TPDO_t *TPDO;
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;

    TPDO = (CO_TPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading)
    {
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(ODF_arg->subIndex == 0)
        {
            /* If there is error in mapping, dataLength is 0, so numberOfMappedObjects is 0. */
            if(!TPDO->dataLength) *value = 0;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(TPDO->restrictionFlags & 0x08) {
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    }
    if(*TPDO->operatingState == CO_NMT_OPERATIONAL && (TPDO->restrictionFlags & 0x02)){
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    }
    if(TPDO->valid){
        return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
    }
    /* numberOfMappedObjects */
    if(ODF_arg->subIndex == 0)
    {
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(*value > 8)
            return CO_SDO_AB_VALUE_HIGH;  /* Value of parameter written too high. */

        /* configure mapping */
        return (CO_SDO_abortCode_t)CO_TPDOconfigMap(TPDO, *value);    // VJ to remove warnings  // modify 20151006 jack
    }

    /* mappedObject */
    else
    {
        uint32_t *value = (uint32_t*) ODF_arg->data;
        uint8_t* pData;
        uint8_t length = 0;
        uint8_t dummy = 0;

        if(TPDO->dataLength) {
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
        }
        /* verify if mapping is correct */
        return (int)CO_PDOfindMap(TPDO->SDO, *value, 1, &pData, &length, &dummy);
    }

    // return CO_SDO_AB_NONE;  // modify 20151006 jack
}

/**
 * Initialize RPDO object.
 *
 * Function must be called in the communication reset section.
 *
 * @param RPDO This object will be initialized.
 * @param em Emergency object.
 * @param SDO SDO server object.
 * @param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * @param nodeId CANopen Node ID of this device. If default COB_ID is used, value will be added.
 * @param defaultCOB_ID Default COB ID for this PDO (without NodeId).
 * See #CO_Default_CAN_ID_t
 * @param restrictionFlags Flag bits indicates, how PDO communication
 * and mapping parameters are handled:
 *  - Bit1: If 1, communication parameters are writeable only in pre-operational NMT state.
 *  - Bit2: If 1, mapping parameters are writeable only in pre-operational NMT state.
 *  - Bit3: If 1, communication parameters are read-only.
 *  - Bit4: If 1, mapping parameters are read-only.
 * @param RPDOCommPar Pointer to _RPDO communication parameter_ record from Object
 * dictionary (index 0x1400+).
 * @param RPDOMapPar Pointer to _RPDO mapping parameter_ record from Object
 * dictionary (index 0x1600+).
 * @param idx_RPDOCommPar Index in Object Dictionary.
 * @param idx_RPDOMapPar Index in Object Dictionary.
 * @param CANdevRx CAN device for PDO reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */

CO_ReturnError_t CO_RPDO_init(void *pCANopen, uint32_t COBID, uint16_t EventTime, uint32_t *pMapping, uint8_t SizeOfMapping)
{
	uint16_t i = 0;
	uint32_t IDs[4] = {0,};
	uint8_t IndexOfRPDO = 0;
	CO_RPDO_t *RPDO = NULL;
	CO_OD_EntryItem_t *object = NULL;
	CO_t *pCO = (CO_t *)pCANopen;

	while(IndexOfRPDO < CO_NO_RPDO && RPDO == NULL){
		if(pCO->RPDO[IndexOfRPDO].RPDOCommPar == NULL){
			RPDO = &pCO->RPDO[IndexOfRPDO];
		}
		else{
			IndexOfRPDO++;
		}
	}

	if(IndexOfRPDO >= CO_NO_RPDO){
		return CO_ERROR_PARAMETERS;
	}

	if((object=CO_OD_find(&pCO->OD, OD_H1400_RXPDO_1_PARAM+IndexOfRPDO)) != NULL){
		RPDO->RPDOCommPar = CO_OD_getDataPointer(object, 1);
	}
	else{
		return CO_ERROR_PARAMETERS;
	}

	if((object=CO_OD_find(&pCO->OD, OD_H1600_RXPDO_1_MAPPING+IndexOfRPDO)) != NULL){
		RPDO->RPDOMapPar = CO_OD_getDataPointer(object, 1);

		memcpy(RPDO->RPDOMapPar, pMapping, SizeOfMapping*4);
	}
	else{
		return CO_ERROR_PARAMETERS;
	}

	if(COBID != 0){
		RPDO->RPDOCommPar->COB_IDUsedByRPDO = COBID;
	}
	else{
		RPDO->RPDOCommPar->COB_IDUsedByRPDO = CO_CAN_ID_RPDO(IndexOfRPDO) | pCO->CommunicationObejct.NodeID;
	}

    /* Configure object variables */
    RPDO->em = &pCO->em;
    RPDO->SDO = &pCO->SDO;
    RPDO->operatingState = &pCO->NMT.operatingState;
    RPDO->nodeId = pCO->CommunicationObejct.NodeID;
    RPDO->defaultCOB_ID = CO_CAN_ID_RPDO(IndexOfRPDO);
    RPDO->restrictionFlags = 0;
    RPDO->EventTime = EventTime;
    RPDO->valid = 1;

    /* Configure Object dictionary entry at index 0x1400+ and 0x1600+ */
    CO_OD_configure(&pCO->OD, OD_H1400_RXPDO_1_PARAM+IndexOfRPDO, CO_ODF_RPDOcom, (void*)RPDO, 0, 0);
    CO_OD_configure(&pCO->OD, OD_H1600_RXPDO_1_MAPPING+IndexOfRPDO, CO_ODF_RPDOmap, (void*)RPDO, 0, 0);

    /* configure communication and mapping */
    RPDO->CANrxNew = 0;
    RPDO->CANdevRx = &pCO->CANmodule;

    CO_RPDOconfigMap(RPDO, SizeOfMapping);
    CO_RPDOconfigCom(RPDO, RPDO->RPDOCommPar->COB_IDUsedByRPDO);

    //RPDOs filter (1번필터 부터 시작 / 0번필터는 예약되어있음)
    for(uint32_t index=0x1400; index<0x1500; index++){
    	if((object=CO_OD_find(&pCO->OD, index)) != NULL){
			UNSIGNED32* pID = (UNSIGNED32 *)CO_OD_getDataPointer(object, 1);

			if(pID != NULL){
				IDs[i] = *pID;
				i++;

				if(RPDO->RPDOCommPar->COB_IDUsedByRPDO == *pID){
					oCAN_SetIdFilter(pCO->CANmodule.pVxd,i/4+1, IDs, i, 0);
					break;
				}
				else if(i >= 4){
					oCAN_SetIdFilter(pCO->CANmodule.pVxd,i/4+1, IDs, i, 0);
					memset(&IDs,0,sizeof(IDs));
					i=0;
				}
			}
			else{
				IDs[i] = 0;
			}
    	}
    	else{
            if(i > 0){
                oCAN_SetIdFilter(pCO->CANmodule.pVxd, i/4+1, IDs, i, 0);
            }
    		break;
    	}
    }

    return CO_ERROR_NO;
}


/**
 * Initialize TPDO object.
 *
 * Function must be called in the communication reset section.
 *
 * pCANopen : Pointer is CO_t
 * IndexOfTPDO : TPDO index
 * Transmission : Transmission type
 * InhibitTime : Inhibit time(ms)
 * EventTime : Event time(ms)
 * pMapping : Od mapping parameter
 * SizeOfMapping : Size of mapping
 *
 */
CO_ReturnError_t CO_TPDO_init(void *pCANopen, uint32_t COBID, uint8_t Transmission, uint16_t InhibitTime, uint16_t EventTime, uint32_t *pMapping, uint8_t SizeOfMapping)
{
	uint8_t IndexOfTPDO = 0;
	CO_TPDO_t *TPDO = NULL;
	CO_OD_EntryItem_t *object = NULL;
	CO_t *pCO = (CO_t *)pCANopen;

	while(IndexOfTPDO < CO_NO_TPDO && TPDO == NULL){
		if(pCO->TPDO[IndexOfTPDO].TPDOCommPar == NULL){
			TPDO = &pCO->TPDO[IndexOfTPDO];
		}
		else{
			IndexOfTPDO++;
		}
	}

	if(IndexOfTPDO >= CO_NO_TPDO){
		return CO_ERROR_PARAMETERS;
	}

	if((object=CO_OD_find(&pCO->OD, OD_H1800_TXPDO_1_PARAM+IndexOfTPDO)) != NULL){
		TPDO->TPDOCommPar = CO_OD_getDataPointer(object, 1);
	}
	else{
		return CO_ERROR_PARAMETERS;
	}

	if((object=CO_OD_find(&pCO->OD, OD_H1A00_TXPDO_1_MAPPING+IndexOfTPDO)) != NULL){
		TPDO->TPDOMapPar = CO_OD_getDataPointer(object, 1);

		memcpy(TPDO->TPDOMapPar, pMapping, SizeOfMapping*4);
	}
	else{
		return CO_ERROR_PARAMETERS;
	}

	if(Transmission > 0 && Transmission <= CO_PDO_TRANSMITION_ASYNC ){
		TPDO->TPDOCommPar->transmissionType = Transmission;
	}
	else{
		TPDO->TPDOCommPar->transmissionType = CO_PDO_TRANSMITION_ASYNC;
	}

	TPDO->TPDOCommPar->inhibitTime = InhibitTime*10;
	TPDO->TPDOCommPar->eventTimer = EventTime;

    /* Configure object variables */
    TPDO->em = &pCO->em;
    TPDO->SDO = &pCO->SDO;
    TPDO->operatingState = &pCO->NMT.operatingState;
    TPDO->nodeId = pCO->CommunicationObejct.NodeID;
    TPDO->restrictionFlags = 0;
    TPDO->defaultCOB_ID = CO_CAN_ID_TPDO(IndexOfTPDO);

	if(COBID != 0){
		TPDO->TPDOCommPar->COB_IDUsedByTPDO = COBID;
	}
	else{
		TPDO->TPDOCommPar->COB_IDUsedByTPDO = TPDO->defaultCOB_ID | pCO->CommunicationObejct.NodeID;
	}

    /* Configure Object dictionary entry at index 0x1800+ and 0x1A00+ */
	CO_OD_configure(&pCO->OD, OD_H1800_TXPDO_1_PARAM+IndexOfTPDO, CO_ODF_TPDOcom, (void*)TPDO, 0, 0);
	CO_OD_configure(&pCO->OD, OD_H1A00_TXPDO_1_MAPPING+IndexOfTPDO, CO_ODF_TPDOmap, (void*)TPDO, 0, 0);

	/* configure communication and mapping */
	TPDO->CANdevTx = &pCO->CANmodule;
	TPDO->syncCounter = 255;
	TPDO->inhibitTimer = TPDO->TPDOCommPar->inhibitTime;
	TPDO->eventTimer = TPDO->TPDOCommPar->eventTimer;
	TPDO->SYNCtimerPrevious = 0;

	if(TPDO->TPDOCommPar->transmissionType>=254) TPDO->sendRequest = 1;

	CO_TPDOconfigMap(TPDO, SizeOfMapping);
	CO_TPDOconfigCom(TPDO, TPDO->TPDOCommPar->COB_IDUsedByTPDO, ((TPDO->TPDOCommPar->transmissionType<=240) ? 1 : 0));

	if((TPDO->TPDOCommPar->transmissionType>240 && TPDO->TPDOCommPar->transmissionType<254) || TPDO->TPDOCommPar->SYNCStartValue>240){
		TPDO->valid = 0;
	}

	if(TPDO->TPDOCommPar->transmissionType <= CO_PDO_TRANSMITION_SYNC){
		pCO->SYNC.enableMonitor = 1;
	}

	return CO_ERROR_NO;
}


/**
 * Verify Change of State of the PDO.
 *
 * Function verifies if variable mapped to TPDO has changed its value. Verified
 * are only variables, which has set attribute _CO_ODA_TPDO_DETECT_COS_ in
 * #CO_SDO_OD_attributes_t.
 *
 * Function may be called by application just before CO_TPDO_process() function,
 * for example: `TPDOx->sendRequest = CO_TPDOisCOS(TPDOx); CO_TPDO_process(TPDOx, ....`
 *
 * @param TPDO TPDO object.
 *
 * @return 1 if COS was detected.
 */
uint8_t CO_TPDOisCOS(CO_TPDO_t *TPDO)
{

    /* Prepare TPDO data automatically from Object Dictionary variables */
    uint8_t* pPDOdataByte;
    uint8_t** ppODdataByte;

    pPDOdataByte = &TPDO->CANtxBuff->data[TPDO->dataLength];
    ppODdataByte = &TPDO->mapPointer[TPDO->dataLength];

    switch(TPDO->dataLength)
    {
        case 8: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x80)) return 1;
        case 7: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x40)) return 1;
        case 6: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x20)) return 1;
        case 5: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x10)) return 1;
        case 4: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x08)) return 1;
        case 3: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x04)) return 1;
        case 2: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x02)) return 1;
        case 1: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x01)) return 1;
    }

    return 0;
}

/**
 * Send TPDO message.
 *
 * Function prepares TPDO data from Object Dictionary variables. It should not
 * be called by application, it is called from CO_TPDO_process().
 *
 *
 * @param TPDO TPDO object.
 *
 * @return Same as CO_CANsend().
 */
static int16_t CO_TPDOsend(CO_TPDO_t *TPDO)
{
    int16_t ret;

    uint8_t* pPDOdataByte = &TPDO->CANtxBuff->data[0];
    uint8_t** ppODdataByte = &TPDO->mapPointer[0];

    for(int16_t i=TPDO->dataLength; i>0; i--)
        *(pPDOdataByte++) = **(ppODdataByte++);

    TPDO->sendRequest = 0;

    ret = CO_CANsend(TPDO->CANdevTx, TPDO->CANtxBuff);
    
    if(ret == CO_ERROR_NO) // Send pdo data copy to buffer - add by JONE
    {
      CO_memcpy(TPDO->LastPDOdata, TPDO->CANtxBuff->data, TPDO->dataLength);

      TPDO->inhibitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
      TPDO->eventTimer = oTMR_GetTick(TICKBASE_SYSTICK);
    }

    return ret;
}

int8_t CO_TPDO_IsChanged(CO_TPDO_t *TPDO) //add by JONE
{
    uint8_t* pPDOdataByte = &TPDO->LastPDOdata[0];
    uint8_t** ppODdataByte = &TPDO->mapPointer[0];

    for(int16_t i=TPDO->dataLength; i>0; i--){
        if(*(pPDOdataByte++) != **(ppODdataByte++)){
        	return 1;
        }
    }

    return 0;
}


/**
 * Process received PDO messages.
 *
 * Function must be called cyclically in any NMT state. It copies data from RPDO
 * to Object Dictionary variables if: new PDO receives and PDO is valid and NMT
 * operating state is operational. It does not verify _transmission type_.
 *
 * @param RPDO This object.
 */
void CO_RPDO_process(CO_RPDO_t *RPDO)
{
	CO_TASK_SUSPEND;

    if(RPDO->CANrxNew && RPDO->valid && *RPDO->operatingState == CO_NMT_OPERATIONAL)
    {
        uint8_t* pPDOdataByte = &RPDO->CANrxData[0];
        uint8_t** ppODdataByte = &RPDO->mapPointer[0];

        for(int16_t i=RPDO->dataLength; i>0; i--)
            **(ppODdataByte++) = *(pPDOdataByte++);

        RPDO->Timestamp = oTMR_GetTick(TICKBASE_CORECYCLE_MS);
        RPDO->IsTimeout = 0;
        RPDO->IsNew = 1;
    }

    if(RPDO->EventTime && oTMR_Elapsed(&RPDO->Timestamp, RPDO->EventTime, TICKBASE_SYSTICK)){
    	RPDO->IsTimeout = 1;
    }

    RPDO->CANrxNew = 0;

    CO_TASK_RESUME;
}

/**
 * Process transmitting PDO messages.
 *
 * Function must be called cyclically in any NMT state. It prepares and sends
 * TPDO if necessary. If Change of State needs to be detected, function
 * CO_TPDOisCOS() must be called before.
 *
 * @param TPDO This object.
 * @param SYNC SYNC object. Ignored if NULL.
 * @param timeDifference_100us Time difference from previous function call in [100 * microseconds].
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 */

void CO_TPDO_process(CO_TPDO_t *TPDO, CO_SYNC_t *SYNC)
{    
    uint8_t PDO_Send = 0;

    if(TPDO->valid && *TPDO->operatingState == CO_NMT_OPERATIONAL)
    {
        /* Send PDO by application request or by Event timer */
        if(TPDO->TPDOCommPar->transmissionType == CO_PDO_TRANSMITION_ASYNC)
        {
        	//edit by JOEN
            PDO_Send = 0;
            if(TPDO->sendRequest){
            	PDO_Send = 1;
            }
            else if(oTMR_Elapsed(&TPDO->eventTimer, TPDO->TPDOCommPar->eventTimer, TICKBASE_SYSTICK)){//EventTime check
            	PDO_Send = 1;
            }
            else if(oTMR_Elapsed(&TPDO->inhibitTimer, TPDO->TPDOCommPar->inhibitTime/10, TICKBASE_SYSTICK)){ //InhibitTime check
            	PDO_Send = CO_TPDO_IsChanged(TPDO);
            }

            if(PDO_Send)
            {
            	CO_TPDOsend(TPDO);
            }		
        }
        else if(TPDO->TPDOCommPar->transmissionType == CO_PDO_TRANSMITION_MANUAL)
        {
        	if(TPDO->TransmissionTrigger)
            {
        		if(CO_TPDOsend(TPDO) == CO_ERROR_NO){
        			TPDO->TransmissionTrigger = 0;
        		}

            }		
        }
        /* Synchronous PDOs */
        else if(SYNC && SYNC->running && SYNC->curentSyncTimeIsInsideWindow)
        {
        	/* detect SYNC message */
            if(SYNC->timer < TPDO->SYNCtimerPrevious)
            {
                /* send synchronous acyclic PDO */
                if(TPDO->TPDOCommPar->transmissionType == 0)
                {

                	if(TPDO->sendRequest) CO_TPDOsend(TPDO);

                }
                /* send synchronous cyclic PDO */
                else
                {
                    /* is the start of synchronous TPDO transmission */
                    if(TPDO->syncCounter == 255)
                    {
                        if(SYNC->counterOverflowValue && TPDO->TPDOCommPar->SYNCStartValue)
                            TPDO->syncCounter = 254;   /* SYNCStartValue is in use */
                        else
                            TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                    }
                    /* if the SYNCStartValue is in use, start first TPDO after SYNC with matched SYNCStartValue. */
                    if(TPDO->syncCounter == 254)
                    {
                        if(SYNC->counter == TPDO->TPDOCommPar->SYNCStartValue)
                        {
                            TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                            CO_TPDOsend(TPDO);

                        }
                    }
                    /* Send PDO after every N-th Sync */
                    else if(--TPDO->syncCounter == 0)
                    {
                        TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                        CO_TPDOsend(TPDO);

                    }
                }
            }
            TPDO->SYNCtimerPrevious = SYNC->timer;
        }

    }
    else
    {

    	/* Not operational or valid. Force TPDO first send after operational or valid. */
        if(TPDO->TPDOCommPar->transmissionType>=254) TPDO->sendRequest = 1;
        else                                         TPDO->sendRequest = 0;
    }
}
