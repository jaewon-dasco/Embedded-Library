/**
 * @file    ONE_Task.h
 * @brief   [?뚯씪??紐⑹쟻 諛?二쇱슂 湲곕뒫??媛꾨왂???ㅻ챸?⑸땲??]
 *
 * @author  JONE
 * @date    2024-06-10
 *
 **/

#ifndef __ONE_TASK_H__
#define __ONE_TASK_H__

#define ONE_TASK_VERSION     0.1

#include "ONE_Common.h"

#define TASTKENTRY_MAXINDEX  20  // Maximum number of task entries

typedef struct {
    void (*Function)(void);   // Pointer to the task function
    uint32_t Period;          // Task period in milliseconds
    uint32_t ProcessingTime;  // Time taken to process the task
    uint32_t Timestamp;       // Last execution timestamp
    uint8_t IsActive;         // Task active status
} oTaskScheduleItem_t;

typedef struct {
    oTaskScheduleItem_t ScheduleEntry[TASTKENTRY_MAXINDEX];  // Array of task entries
    uint8_t CountOfEntry;                                    // Number of active task entries
    uint8_t IndexOfCurrent;                                  // Index of the current task being executed
} oTaskManager_t;

extern oTaskManager_t oTASK_MainTask;

extern oResult_t oTASK_SetSchedule(oTaskManager_t *TaskManager, void (*TaskFunction)(void), uint32_t Period);
extern void oTASK_Process(oTaskManager_t *TaskManager);
extern void oTASK_Run();

#endif /* __ONE_TASK_H__ */

/* History

2026-06-26 | v0.1
	- baseline (ONE_Task.h)
*/
