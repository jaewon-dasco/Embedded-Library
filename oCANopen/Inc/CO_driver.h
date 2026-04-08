
/*
 * CAN module object for Microchip STM32F103 microcontroller.
 *
 * @file        CO_driver.h
 * @version     SVN: \$Id: CO_driver.h 42 2013-07-09 11:14:12Z jani22 $
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

#ifndef CO_DRIVER_H
#define CO_DRIVER_H

/* Includes ------------------------------------------------------------------*/
#include "ONE_Math.h"
#include <CO_Common.h>
#include "CO_OD.h"

#ifdef _CMSIS_OS_H
	#define CO_TASK_SUSPEND vTaskSuspendAll()
	#define CO_TASK_RESUME xTaskResumeAll()
#else
	#define CO_TASK_SUSPEND
	#define CO_TASK_RESUME
#endif

/* Exported define -----------------------------------------------------------*/
#define PACKED_STRUCT               __attribute__((packed))
#define ALIGN_STRUCT_DWORD          __attribute__((aligned(4)))

/* Disabling interrupts */
#define CO_DISABLE_INTERRUPTS();
#define CO_ENABLE_INTERRUPTS();

#define CO_CAN_ID_TPDO(n)	(n < 4 ? (0x180+(0x100*n)) : (0x1A0+(0x100*MATH_LIMIT(n-4,0,3))))
#define CO_CAN_ID_RPDO(n)	(0x200+(0x100*MATH_LIMIT(n, 0, 3)))

/* Exported define -----------------------------------------------------------*/
/**
 * Default CANopen identifiers.
 *
 * Default CANopen identifiers for CANopen communication objects. Same as
 * 11-bit addresses of CAN messages. These are default identifiers and
 * can be changed in CANopen. Especially PDO identifiers are confgured
 * in PDO linking phase of the CANopen network configuration.
 */
typedef enum{
     CO_CAN_ID_NMT_SERVICE       = 0x000,   /**< 0x000, Network management */
     CO_CAN_ID_SYNC              = 0x080,   /**< 0x080, Synchronous message */
     CO_CAN_ID_EMERGENCY         = 0x080,   /**< 0x080, Emergency messages (+nodeID) */
     CO_CAN_ID_TIME_STAMP        = 0x100,   /**< 0x100, Time stamp message */
     CO_CAN_ID_TPDO_1            = 0x180,   /**< 0x180, Default TPDO1 (+nodeID) */
     CO_CAN_ID_RPDO_1            = 0x200,   /**< 0x200, Default RPDO1 (+nodeID) */
     CO_CAN_ID_TPDO_2            = 0x280,   /**< 0x280, Default TPDO2 (+nodeID) */
     CO_CAN_ID_RPDO_2            = 0x300,   /**< 0x300, Default RPDO2 (+nodeID) */
     CO_CAN_ID_TPDO_3            = 0x380,   /**< 0x380, Default TPDO3 (+nodeID) */
     CO_CAN_ID_RPDO_3            = 0x400,   /**< 0x400, Default RPDO3 (+nodeID) */
     CO_CAN_ID_TPDO_4            = 0x480,   /**< 0x480, Default TPDO4 (+nodeID) */
     CO_CAN_ID_RPDO_4            = 0x500,   /**< 0x500, Default RPDO5 (+nodeID) */
     CO_CAN_ID_TSDO              = 0x580,   /**< 0x580, SDO response from server (+nodeID) */
     CO_CAN_ID_RSDO              = 0x600,   /**< 0x600, SDO request from client (+nodeID) */
     CO_CAN_ID_HEARTBEAT         = 0x700    /**< 0x700, Heartbeat message */
}CO_Default_CAN_ID_t;

/* Received message object */
typedef struct{
    uint16_t            ident;
    uint16_t            mask;
    void               *object;
    void              (*pFunct)(void *object, uint8_t dlc, uint8_t *pData);
}CO_CANrx_t;

/* Transmit message object. */
typedef struct{
    uint32_t            ident;
    uint8_t             DLC;
    uint8_t             data[8];
    volatile uint8_t    bufferFull;
    volatile uint8_t    syncFlag;
}CO_CANtx_t;/* ALIGN_STRUCT_DWORD; */

/* CAN module object. */
typedef struct{
    oCANVxD_t          *pVxd;

    CO_CANrx_t         rxArray[10];
    uint16_t            rxArraySize;
    CO_CANtx_t         txArray[10];
    uint16_t            txArraySize;
    volatile uint8_t    useCANrxFilters;
    volatile uint8_t    bufferInhibitFlag;
    volatile uint8_t    firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    void               *em;

    uint8_t             IsReceived;
    uint8_t             IsTransmitted;
    uint8_t             IsOpen;
    uint8_t             IsBusOff;
    uint8_t             IsBusPassive;
}CO_CANmodule_t;

/* Init CAN Led Interface */
typedef enum {
    eCoLed_None = 0,
    eCoLed_Green = 1,
    eCoLed_Red = 2,
} eCoLeds;

/* Exported variables -----------------------------------------------------------*/

/* Exported functions -----------------------------------------------------------*/



/* Request CAN configuration or normal mode */
//void CO_CANsetConfigurationMode(CAN_TypeDef *CANbaseAddress);
//void CO_CANsetNormalMode(CAN_TypeDef *CANbaseAddress);

/* Initialize CAN module object. */
CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t  *CANmodule, CO_OD_t *ODEntryList, CAN_HandleTypeDef *CANHandle);


/* Switch off CANmodule. */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule);


/* Read CAN identifier */
//uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);


/* Configure CAN message receive buffer. */
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        uint16_t                mask,
        int8_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, uint8_t dlc, uint8_t *pData));


/* Configure CAN message transmit buffer. */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        int8_t                  rtr,
        uint8_t                 noOfBytes,
        int8_t                  syncFlag);

/* Send CAN message. */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/* Clear all synchronous TPDOs from CAN module transmit buffers. */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/* Verify all errors of CAN module. */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule);


/* CAN interrupts receives and transmits CAN messages. */
void CO_CANinterrupt_Tx(CO_CANmodule_t *CANmodule);
void CO_CANinterrupt_Status(CO_CANmodule_t *CANmodule);

#endif    /* CO_DRIVER_H */

