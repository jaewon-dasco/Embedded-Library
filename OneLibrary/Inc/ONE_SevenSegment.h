/*
 * ONE_IoCtrl.h
 *
 *  Created on: Dec 5, 2024
 *      Author: JONE
 */

#ifndef INC_ONE_SEVENSEGMENT_H_
#define INC_ONE_SEVENSEGMENT_H_

#include "ONE_Common.h"
#include "ONE_Signal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//7Segment
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SEVENSEGMENT_CHANNEL_MAXINDEX			4

#ifdef GPIO_PIN_MASK
#define SEVENSEGMENT_IO_CTRL(io,state)			((io.Port != NULL) ? HAL_GPIO_WritePin(io.Port,io.Pin, (state? io.ActiveLevel:!io.ActiveLevel)) : (io.Pin = 0))
#else
#define SEVENSEGMENT_IO_CTRL(io,state)
#endif

										  	  // A     B     C     D     E     F     G     H    I      J   (K)   L    (M)   N     O     P     Q     R     S     T     U     V   (W)  (X)    Y    (Z)
static const char oSevenSegment_Alpha[]		= {0xEE, 0x3E, 0x9C, 0x7A, 0x9E, 0x8E, 0xBC, 0x2E, 0x20, 0x70,  0,  0x1C,  0,  0x2A, 0x3A, 0xCE, 0x76, 0x0a, 0xB6, 0x1E, 0x38, 0x7C, 0,   0,   0x76,  0};
static const char oSevenSegment_Number[]	= {0xFC, 0x60, 0xDA, 0xF2, 0x66, 0xB6, 0xBE, 0xE4, 0xFE, 0xF6 };
static const char oSevenSegment_Minus		= 0x02;
static const char oSevenSegment_Plus 		= 0x62;

typedef struct{
	oIO_t Channel[SEVENSEGMENT_CHANNEL_MAXINDEX];
	oIO_t SegmentA;
	oIO_t SegmentB;
	oIO_t SegmentC;
	oIO_t SegmentD;
	oIO_t SegmentE;
	oIO_t SegmentF;
	oIO_t SegmentG;
	oIO_t SegmentDot;

	uint8_t StringIndex;
	uint8_t SegmentIndex;
	uint32_t UpdateTimer;
	uint8_t SequenceStep;
}oSevenSegment_t;

extern void oSevenSegment(oSevenSegment_t *pSegment, char* pStr, uint16_t Interval, uint8_t Alignement);

#endif
