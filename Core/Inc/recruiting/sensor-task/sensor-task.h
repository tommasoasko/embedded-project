#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "eagletrt-fsm-api.h"
#include "Core/Inc/recruiting/filters/filters.h"

/**
 * @brief Enumerates the possible states of the sensor task
 */
enum SensorState
{
    SENSOR_STATE_POST = 0,
    SENSOR_STATE_PAUSED,
    SENSOR_STATE_READING,
    SENSOR_STATE_FATAL
};

/**
 * @brief Structure containing the context data for the sensor task
 */
struct SensorContext
{
    volatile uint16_t adc_data[2];
    MovingAverageFilterPot_s filter_pot;
    MovingAverageFilterNTC_s filter_ntc;
    uint8_t state_just_changed;
    uint32_t state_entry_time;
};

void sensor_task_init(void);
void sensor_task_routine(uint8_t task_id);

#endif