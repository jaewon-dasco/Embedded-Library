/**
 ******************************************************************************
 * @file    CO_common.h
 * @author
 * @version v1.0
 * @date
 * @brief   CAN Open common function file header
 ******************************************************************************
 */

#ifndef CO_COMMON_H
#define CO_COMMON_H

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "ONE_CAN.h"
#include "ONE_Time.h"

/* Exported define -----------------------------------------------------------*/
#define CO_NO_SYNC                     1   //Associated objects: 1005, 1006, 1007, 2103, 2104
#define CO_NO_EMERGENCY                1   //Associated objects: 1014, 1015
#define CO_NO_SDO_SERVER               1   //Associated objects: 1200
#define CO_NO_SDO_CLIENT               0
#define CO_NO_NMT_MASTER               0
#define CO_NO_RPDO                     8   //Associated objects: 1400, 1401, 1402, 1403, 1600, 1601, 1602, 1603
#define CO_NO_TPDO                     8   //Associated objects: 1800, 1801, 1802, 1803, 1A00, 1A01, 1A02, 1A03

/**
 * Helper union for manipulating data bytes.
 */
typedef union{
    uint8_t  u8[8];  /**< 8 bytes */
    uint16_t u16[4]; /**< 4 words */
    uint32_t u32[2]; /**< 2 double words */
}CO_bytes_t;

/* Return values */
typedef enum{
    CO_ERROR_NO                 = 0,
    CO_ERROR_ILLEGAL_ARGUMENT   = -1,
    CO_ERROR_OUT_OF_MEMORY      = -2,
    CO_ERROR_TIMEOUT            = -3,
    CO_ERROR_ILLEGAL_BAUDRATE   = -4,
    CO_ERROR_RX_OVERFLOW        = -5,
    CO_ERROR_RX_PDO_OVERFLOW    = -6,
    CO_ERROR_RX_MSG_LENGTH      = -7,
    CO_ERROR_RX_PDO_LENGTH      = -8,
    CO_ERROR_TX_OVERFLOW        = -9,
    CO_ERROR_TX_PDO_WINDOW      = -10,
    CO_ERROR_TX_UNCONFIGURED    = -11,
    CO_ERROR_PARAMETERS         = -12,
    CO_ERROR_DATA_CORRUPT       = -13,
    CO_ERROR_CRC                = -14
}CO_ReturnError_t;

/**
 * Helper function like memcpy.
 *
 * Function copies n data bytes from source to destination.
 *
 * @param dest Destination location.
 * @param src Source location.
 * @param size Number of data bytes to be copied (max 0xFFFF).
 */
void CO_memcpy(uint8_t dest[], const uint8_t src[], const uint16_t size);

/**
 * Helper function returns uint16 from byte array.
 *
 * @param data Location of source data.
 * @return Variable of type uint16_t.
 */
uint16_t CO_getUint16(const uint8_t data[]);

/**
 * Helper function returns uint32 from byte array.
 *
 * @param data Location of source data.
 * @return Variable of type uint32_t.
 */
uint32_t CO_getUint32(const uint8_t data[]);

/**
 * Helper function writes uint16 to byte array.
 *
 * @param data Location of destionation data.
 * @param value Variable of type uint16_t to be written into data.
 */
void CO_setUint16(uint8_t data[], const uint16_t value);

/**
 * Helper function writes uint32 to byte array.
 *
 * @param data Location of destionation data.
 * @param value Variable of type uint32_t to be written into data.
 */
void CO_setUint32(uint8_t data[], const uint32_t value);

void CO_memcpySwap2(uint8_t dest[], const uint8_t src[]);
void CO_memcpySwap4(uint8_t dest[], const uint8_t src[]);

/**
 * Calculate CRC sum on block of data.
 *
 * @param block Pointer to block of data.
 * @param blockLength Length of data in bytes;
 * @param crc Initial value (zero for xmodem). If block is split into
 * multiple segments, previous CRC is used as initial.
 *
 * @return Calculated CRC.
 */
unsigned short crc16_ccitt(
        const unsigned char     block[],
        unsigned int            blockLength,
        unsigned short          crc);


/** @} */

#endif    /* CO_COMMON_H */
