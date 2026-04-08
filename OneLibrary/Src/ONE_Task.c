#include "ONE_Common.h"
#include "ONE_Time.h"
#include "ONE_Task.h"

oTaskManager_t oTASK_MainTask;

oResult_t oTASK_SetSchedule(oTaskManager_t *TaskManager, void (*TaskFunction)(void), uint32_t Period)
{
    uint8_t Index;
    uint8_t IsExisting = 0;
    oTaskScheduleItem_t *newEntry;

    if(TaskManager == NULL || TaskFunction == NULL) {
        return RESULT_NULL;
    }

    if (TaskManager->CountOfEntry < TASTKENTRY_MAXINDEX) {
        for(Index = 0; Index < TaskManager->CountOfEntry; Index++) {
            if (TaskManager->ScheduleEntry[Index].Function == TaskFunction) {
                IsExisting = 1;
                break;
            }
        }

        if(!IsExisting){
            Index = TaskManager->CountOfEntry;
            TaskManager->CountOfEntry++;
        }

        newEntry = &TaskManager->ScheduleEntry[Index];
        newEntry->Function = TaskFunction;
        newEntry->Period = Period;
        newEntry->Timestamp = 0;
        newEntry->IsActive = 1;

        return RESULT_OK;
    }

    return RESULT_ERROR;
}

void oTASK_Process(oTaskManager_t *TaskManager)
{
    if(TaskManager == NULL || TaskManager->CountOfEntry == 0) {
        return;
    }
    
    while(TaskManager->IndexOfCurrent < TaskManager->CountOfEntry){
        oTaskScheduleItem_t *task = &TaskManager->ScheduleEntry[TaskManager->IndexOfCurrent];
        TaskManager->IndexOfCurrent++;

        if(task->Function && task->IsActive && (oTMR_Interval(oTMR_GetTick(TICKBASE_SYSTICK), task->Timestamp) >= task->Period)) {
            uint32_t time = oTMR_GetTick(TICKBASE_SYSTICK);
            task->Timestamp = time;
            task->Function();
            task->ProcessingTime = oTMR_Interval(oTMR_GetTick(TICKBASE_SYSTICK), time);
            break; // Exit after processing the first active task
        }
    }

    TaskManager->IndexOfCurrent %= TaskManager->CountOfEntry;
}

void oTASK_Run()
{
    oTASK_Process(&oTASK_MainTask);
}
