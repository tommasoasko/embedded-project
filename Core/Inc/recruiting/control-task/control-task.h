#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "eagletrt-fsm-api.h"

/** @brief Enumeration of control states */
enum ControlState
{
    CONTROL_STATE_POST = 0,
    CONTROL_STATE_OK,
    CONTROL_STATE_WARNING,
    CONTROL_STATE_FATAL
};

/** @brief Structure for control context */
struct ControlContext
{
    uint8_t state_just_changed;
    uint32_t warning_start_time;
    int16_t breathing_duty;
    int8_t breathing_dir;
    uint32_t state_entry_time;
    uint32_t btn_hold_time;
    uint32_t last_breathing_update;
    uint8_t was_reading_active;
};

void control_task_init(void);
void control_task_routine(uint8_t task_id);

#endif