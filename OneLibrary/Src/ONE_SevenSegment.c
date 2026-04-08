/*
 * ONE_IoControl.c
 *
 *  Created on: Dec 5, 2024
 *      Author: JONE
 */
#include "ONE_Time.h"
#include "ONE_Memory.h"
#include "ONE_SevenSegment.h"

void oSevenSegment(oSevenSegment_t *pSegment, char* pStr, uint16_t Interval, uint8_t Alignement)
{
    if(pSegment == NULL || pStr == NULL){
        return;
    }

    uint32_t i = 0;
    uint32_t len = 0;
    uint32_t displayIdxStr = 0;
    uint32_t displayIdxFND = 0;
    uint8_t hasDot = 0;
    char FndBit = 0;

    if (!Alignement) {
        displayIdxFND = pSegment->SegmentIndex;
    } else {
        displayIdxFND = SEVENSEGMENT_CHANNEL_MAXINDEX - pSegment->SegmentIndex - 1;
    }

    switch(pSegment->SequenceStep)
    {
		default:
			pSegment->SequenceStep = 0;
		case 0:
			if (oTMR_Elapsed(&pSegment->UpdateTimer, Interval, TICKBASE_SYSTICK)) {
				pSegment->SequenceStep++;
			}
			break;
		case 1:
		    // FND Select 1 to 4
		    for (i=0; i < SEVENSEGMENT_CHANNEL_MAXINDEX; i++) {
		    	if(pSegment->Channel[i].Port){
		    		SEVENSEGMENT_IO_CTRL(pSegment->Channel[i], 0);
		    	}
		    }

		    //clear all IO
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentA, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentB, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentC, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentD, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentE, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentF, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentG, 0);
			SEVENSEGMENT_IO_CTRL(pSegment->SegmentDot, 0);
			pSegment->SequenceStep++;
			pSegment->UpdateTimer = oTMR_GetTick(TICKBASE_SYSTICK);
        case 2:
            if (oTMR_Elapsed(&pSegment->UpdateTimer, 1, TICKBASE_SYSTICK)) {
                pSegment->SequenceStep++;
            }
            break;
		case 3:
		    len = strlen(pStr);

		    if (len > 0 && pSegment->StringIndex < len) {

		        if (!Alignement) {
		            displayIdxStr = pSegment->StringIndex;
		        } else {
		            displayIdxStr = len - pSegment->StringIndex - 1;
		        }

		        if (pStr[displayIdxStr] >= '0' && pStr[displayIdxStr] <= '9') {
		            FndBit = oSevenSegment_Number[pStr[displayIdxStr] - '0'];
		        } else if (pStr[displayIdxStr] >= 'A' && pStr[displayIdxStr] <= 'Z') {
		            FndBit = oSevenSegment_Alpha[pStr[displayIdxStr] - 'A'];
		        } else if (pStr[displayIdxStr] == '-') {
		            FndBit = oSevenSegment_Minus;
		        } else if (pStr[displayIdxStr] == '+') {
		            FndBit = oSevenSegment_Plus;
		        } else if (pStr[displayIdxStr] == '.') {
		        	pSegment->StringIndex++;
		        	return;
				}

		        if (displayIdxStr + 1 < len && pStr[displayIdxStr + 1] == '.') {
		            hasDot = 1;
		        }
		    }

		    // FND Select 1 to 4
		    for (i=0; i < SEVENSEGMENT_CHANNEL_MAXINDEX; i++) {
		    	if(pSegment->Channel[i].Port){
		    		SEVENSEGMENT_IO_CTRL(pSegment->Channel[i], displayIdxFND == i);
		    	}
		    }

		    // FND LED A-G and dot
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentA, oMEM_GetBit(&FndBit, 7));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentB, oMEM_GetBit(&FndBit, 6));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentC, oMEM_GetBit(&FndBit, 5));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentD, oMEM_GetBit(&FndBit, 4));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentE, oMEM_GetBit(&FndBit, 3));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentF, oMEM_GetBit(&FndBit, 2));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentG, oMEM_GetBit(&FndBit, 1));
		    SEVENSEGMENT_IO_CTRL(pSegment->SegmentDot, hasDot);

		    // 다음 세그먼트로 이동
		    pSegment->SegmentIndex = (pSegment->SegmentIndex + 1) % SEVENSEGMENT_CHANNEL_MAXINDEX;

		    // 유효한 세그먼트가 없으면 초기화
		    if (pSegment->Channel[pSegment->SegmentIndex].Port == NULL) {
		        pSegment->SegmentIndex = 0;
		    }

		    // 문자열 인덱스 갱신
            if (pSegment->SegmentIndex == 0) {
                pSegment->StringIndex = 0;
            } else {
                pSegment->StringIndex++;
            }

            pSegment->UpdateTimer = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pSegment->UpdateTimer); → 대입으로 변경
            pSegment->SequenceStep++;
            break;
    }
}
