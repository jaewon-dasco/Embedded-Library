/*
 * EEPROM_24LC16B.h
 *
 *  Created on: Dec 24, 2024
 *      Author: JONE
 */

#ifndef INC_EEPROM_24LC_H_
#define INC_EEPROM_24LC_H_

#include "ONE_Common.h"
#include "Mi_Main.h"
#include "main.h"

#define EEPROM_24LC32A

#if defined(EEPROM_24LC32A)
#define EEPROM_24LC_PAGE_BYTESIZE	32
#define EEPROM_24LC_MAX_BYTESIZE	(EEPROM_24LC_PAGE_BYTESIZE*4095)
#elif defined(EEPROM_24LC16B)
#define EEPROM_24LC_PAGE_BYTESIZE	16
#define EEPROM_24LC_MAX_BYTESIZE	(EEPROM_24LC_PAGE_BYTESIZE*255)
#endif

#if defined(EEPROM_24LC32A)
#define EEPROM_24LC_MEMADD_SIZE		I2C_MEMADD_SIZE_16BIT
#else
#define EEPROM_24LC_MEMADD_SIZE		I2C_MEMADD_SIZE_8BIT
#endif

extern oResult_t EEPROM_24LC_Read(I2C_HandleTypeDef *pHandle, uint16_t Index, uint8_t *pData, uint32_t Size);
extern oResult_t EEPROM_24LC_Write(I2C_HandleTypeDef *pHandle, uint16_t Index, uint8_t *pData, uint32_t Size);

#endif /* INC_EEPROM_24LC_H_ */
