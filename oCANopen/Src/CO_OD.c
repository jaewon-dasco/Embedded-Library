/*******************************************************************************

   File - CO_OD.c
   CANopen Object Dictionary.

   Copyright (C) 2004-2008 Janez Paternoster

   License: GNU Lesser General Public License (LGPL).

   <http://canopennode.sourceforge.net>

   (For more information see <CO_SDO.h>.)
*/
/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.



   Author: Janez Paternoster


   This file was automatically generated with CANopenNode Object
   Dictionary Editor. DON'T EDIT THIS FILE MANUALLY !!!!

*******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#include <CANopen.h>
#include "CO_OD.h"
#include "ONE_Common.h"

/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private variable ----------------------------------------------------------*/
const CO_OD_Record_t OD_recodeItem1018[] = {{0x87, 4},{0x87, 4},{0x87, 4},{0x87, 4}};
const CO_OD_Record_t OD_recodeItem1200[] = {{0x85, 4},{0x85, 4},{0x05, 1}};
const CO_OD_Record_t OD_recodeItem1400[] = {{0x8D, 4},{0x0D, 4}};
const CO_OD_Record_t OD_recodeItem1600[] = {{0x8D, 4},{0x0D, 4},{0x8D, 4},{0x0D, 4},{0x8D, 4},{0x0D, 4},{0x8D, 4},{0x0D, 4}};
const CO_OD_Record_t OD_recodeItem1800[] = {{0x85, 4},{0x07, 1},{0x8F, 2},{0x0F, 1},{0x8F, 2},{0x07, 1}};
const CO_OD_Record_t OD_recodeItem1A00[] = {{0x85, 4},{0x05, 4},{0x85, 4},{0x05, 4},{0x85, 4},{0x05, 4},{0x85, 4},{0x05, 4}};


/* Private function ----------------------------------------------------------*/

/**
 * Configure additional functionality to one @ref CO_SDO_objectDictionary entry.
 *
 * Additional functionality include: @ref CO_SDO_OD_function and
 * #CO_SDO_OD_flags_t. It is optional feature and can be used on any object in
 * Object dictionary. If OD entry does not exist, function returns silently.
 *
 * @param SDO This object.
 * @param index Index of object in the Object dictionary.
 * @param pODFunc Pointer to @ref CO_SDO_OD_function, specified by application.
 * If NULL, @ref CO_SDO_OD_function will not be used on this object.
 * @param object Pointer to object, which will be passed to @ref CO_SDO_OD_function.
 * @param flags Pointer to array of #CO_SDO_OD_flags_t defined externally. If
 * zero, #CO_SDO_OD_flags_t will not be used on this OD entry.
 * @param flagsSize Size of the above array. It must be equal to number
 * of sub-objects in object dictionary entry. Otherwise #CO_SDO_OD_flags_t will
 * not be used on this OD entry.
 */
void CO_OD_configure(CO_OD_t *ODEntryList, uint16_t index, CO_OD_Callback_t pODFunc, void *object, uint8_t *flags, uint8_t flagsSize)
{
	CO_OD_EntryItem_t *oditem;

    if((oditem = CO_OD_find(ODEntryList, index)) != NULL)
    {
        uint8_t maxSubIndex = oditem->maxSubIndex;

		oditem->extention.pODFunc = pODFunc;
        oditem->extention.object = object;
        if((flags != NULL) && (flagsSize != 0U) && (flagsSize == maxSubIndex))
        {
            uint16_t i;
            oditem->extention.flags = flags;
            for(i=0U; i<maxSubIndex; i++)
            {
            	oditem->extention.flags[i] = 0U;
            }
        }
        else
        {
        	oditem->extention.flags = NULL;
        }
    }
}

void CO_OD_sort(CO_OD_EntryItem_t* items, uint16_t size) {
    if (items == NULL || size < 2) return;

    for (uint16_t i = 0; i < size - 1; ++i) {
        for (uint16_t j = 0; j < size - i - 1; ++j) {
            uint8_t swap = 0;
            if (items[j].index > items[j + 1].index) {
                swap = 1;
            }

            if (swap) {
                CO_OD_EntryItem_t temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
}


/**
 * Find object with specific index in Object dictionary.
 *
 * @param SDO This object.
 * @param index Index of the object in Object dictionary.
 *
 * @return Sequence number of the @ref CO_SDO_objectDictionary entry, 0xFFFF if not found.
 */
CO_OD_EntryItem_t* CO_OD_findEndtry(CO_OD_Entry_t *Entry, uint16_t index)
{
    /* Fast search in ordered Object Dictionary. If indexes are mixed, this won't work. */
    /* If Object Dictionary has up to 2^N entries, then N is max number of loop passes. */
    uint16_t cur, min, max;
    CO_OD_EntryItem_t* object;

    if(Entry == NULL || index == 0 || Entry->Count <= 0){
    	return NULL;
    }

    min = 0U;
    max = Entry->Count - 1U;
    while(min < max)
    {
        cur = (min + max) / 2;
        object = &Entry->Items[cur];
        /* Is object matched */
        if(index == object->index)
        {
            return object;
        }
        if(index < object->index)
        {
            max = cur;
            if(max) max--;
        }
        else
            min = cur + 1U;
    }

    if(min == max)
    {
        object = &Entry->Items[min];
        /* Is object matched */
        if(index == object->index)
        {
            return object;
        }
    }

    return NULL;  /* object does not exist in OD */
}

CO_OD_EntryItem_t* CO_OD_find(CO_OD_t *List, uint16_t index)
{
	CO_OD_EntryItem_t *entry = NULL;

	if(List->ManufacturerEntry.Items && (entry = CO_OD_findEndtry(&List->ManufacturerEntry, index)) != NULL){
		return entry;
	}

	if(List->CommunicationEntry.Items && (entry = CO_OD_findEndtry(&List->CommunicationEntry, index)) != NULL){
		return entry;
	}

	return NULL;
}

CO_OD_EntryItem_t* CO_OD_addVariable(CO_OD_t *List, uint16_t index, uint8_t maxSubIndex, uint16_t attribute, uint16_t length, void *pData)
{
	CO_OD_EntryItem_t *item = CO_OD_find(List, index);
	CO_OD_Entry_t *entry = NULL;

	if(item != NULL){
		return item;
	}

	if((index >= 0x1000 && index <= 0x1A7F) || index <= 0x1F80 || (index >= 0x2101 && index <= 0x2102)){
		entry = (CO_OD_Entry_t *)&List->CommunicationEntry;
	}
	else if(List->ManufacturerEntry.Items){
		entry = &List->ManufacturerEntry;
	}

	if(entry == NULL){
		return NULL;
	}

	if(entry->Count < entry->Length){
		entry->Items[entry->Count].index = index;
		entry->Items[entry->Count].maxSubIndex = maxSubIndex;
		entry->Items[entry->Count].attribute = attribute;
		entry->Items[entry->Count].length = length;
		entry->Items[entry->Count].pData = pData;
		entry->Items[entry->Count].pArgument = NULL;

		item = &entry->Items[entry->Count];
		entry->Count++;

		CO_OD_sort(entry->Items, entry->Count);
	}

	return item;
}

CO_OD_EntryItem_t* CO_OD_addRecode(CO_OD_t *List, uint16_t index, uint8_t maxSubIndex, CO_OD_Record_t *record, void *pData)
{
	CO_OD_EntryItem_t *item = CO_OD_find(List, index);
	CO_OD_Entry_t *entry = NULL;

	if(item != NULL){
		return item;
	}

	if((index >= 0x1000 && index <= 0x1A7F) || index <= 0x1F80 || (index >= 0x2101 && index <= 0x2102)){
		entry = (CO_OD_Entry_t *)&List->CommunicationEntry;
	}
	else if(List->ManufacturerEntry.Items){
		entry = &List->ManufacturerEntry;
	}

	if(entry == NULL){
		return NULL;
	}

	if(entry->Count < entry->Length){
		entry->Items[entry->Count].index = index;
		entry->Items[entry->Count].maxSubIndex = maxSubIndex;
		entry->Items[entry->Count].pData = pData;
		entry->Items[entry->Count].pArgument = record;

		item = &entry->Items[entry->Count];
		entry->Count++;

		CO_OD_sort(entry->Items, entry->Count);
	}

	return item;
}

/**
 * Get length of the given object with specific subIndex.
 *
 * @param SDO This object.
 * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
 * @param subIndex Sub-index of the object in Object dictionary.
 *
 * @return Data length of the variable.
 */
uint16_t CO_OD_getLength(const CO_OD_EntryItem_t *object, uint8_t subIndex)
{
    if(object->maxSubIndex == 0U)
    {    /* Object type is Var */
        if(object->pData == 0)
        { /* data type is domain */
            return 32;
        }
        else
        {
            return object->length;
        }
    }
    else if(subIndex == 0){
    	return 1;
    }
    else if(object->attribute != 0U)
    { /* Object type is Array */
        if(subIndex == 0U)
        {
            return 1U;
        }
        else if(object->pData == 0)
        {
            /* data type is domain */
            return 32;
        }
        else
        {
            return object->length;
        }
    }
    else if(object->pArgument != 0)
    {
    	/* Object type is Record */
    	return ((const CO_OD_Record_t*)(object->pArgument))[subIndex-1].length;
    }
    else{
    	/* data type is domain */
		return 32;
    }
}

/**
 * Get attribute of the given object with specific subIndex. See #CO_SDO_OD_attributes_t.
 *
 * If Object Type is array and subIndex is zero, functon always returns
 * 'read-only' attribute.
 *
 * @param SDO This object.
 * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
 * @param subIndex Sub-index of the object in Object dictionary.
 *
 * @return Attribute of the variable.
 */
uint16_t CO_OD_getAttribute(const CO_OD_EntryItem_t* object, uint8_t subIndex)
{
	uint16_t attr;

    if(object->maxSubIndex == 0U)
    {   /* Object type is Var */
        return object->attribute;
    }
    else if(object->attribute != 0U)
    {/* Object type is Array */
        attr = object->attribute;
        if(subIndex == 0U)
        {
            /* First subIndex is readonly */
            attr &= ~(CO_ODA_WRITEABLE | CO_ODA_RPDO_MAPABLE);
            attr |= CO_ODA_READABLE;
        }
        return attr;
    }
    else if(object->pArgument)
    {
    	/* Object type is Record */

    	if(subIndex == 0U)
		{
    		/* First subIndex is readonly */
    		attr = ((const CO_OD_Record_t*)(object->pArgument))[0].attribute;

			attr &= ~(CO_ODA_WRITEABLE | CO_ODA_RPDO_MAPABLE);
			attr |= CO_ODA_READABLE;

			return attr;
		}
    	else{
    		return ((const CO_OD_Record_t*)(object->pArgument))[subIndex-1].attribute;
    	}
    }
    else{
    	return 0;
    }
}

/**
 * Get pointer to data of the given object with specific subIndex.
 *
 * If Object Type is array and subIndex is zero, functon returns pointer to
 * object->maxSubIndex variable.
 *
 * @param SDO This object.
 * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
 * @param subIndex Sub-index of the object in Object dictionary.
 *
 * @return Pointer to the variable in @ref CO_SDO_objectDictionary.
 */
void* CO_OD_getDataPointer(const CO_OD_EntryItem_t* object, uint8_t subIndex)
{
    if(object->maxSubIndex == 0U)
    {   /* Object type is Var */
        return object->pData;
    }
    else if(subIndex==0)
	{
		/* this is the data, for the subIndex 0 in the array */
		return (void*) &object->maxSubIndex;
	}
    else if(object->pData == 0)
    {
        /* data type is domain */
        return 0;
    }
    else if(object->attribute != 0U)
    {
    	/* Object type is Array */
		return (void*)(((int8_t*)object->pData) + ((subIndex-1) * object->length));
    }
    else
    {
    	/* Object Type is Record */
    	void *pData = object->pData;

    	for(uint8_t i=1; i<subIndex; i++){
    		pData += (uint32_t)(((const CO_OD_Record_t*)(object->pArgument))[i-1].length);
    	}

        return pData;
    }
}

/**
 * Get pointer to the #CO_SDO_OD_flags_t byte of the given object with
 * specific subIndex.
 *
 * @param SDO This object.
 * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
 * @param subIndex Sub-index of the object in Object dictionary.
 *
 * @return Pointer to the #CO_SDO_OD_flags_t of the variable.
 */
uint8_t* CO_OD_getFlagsPointer(CO_OD_EntryItem_t *object, uint8_t subIndex)
{
	if(object == NULL || object->extention.flags == NULL){
		return NULL;
	}

	if(object->maxSubIndex > 0){
		if(subIndex == 0){
			return NULL;
		}

		subIndex -= 1;
	}

    return &object->extention.flags[subIndex];
}

void* CO_OD_getData(CO_OD_t *List, uint16_t index, uint8_t subIndex)
{
	CO_OD_EntryItem_t *item = CO_OD_find(List, index);

	if(item == NULL) return NULL;

	return CO_OD_getDataPointer(item, subIndex);
}
