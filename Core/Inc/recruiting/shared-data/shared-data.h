#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>
#include "Core/Inc/recruiting/filters/filters.h"

/**
 * @brief Structure containing the shared data for the sensor and control task
 */
struct SharedData
{
    uint16_t raw_pot;
    uint16_t raw_ntc;
    uint16_t filtered_pot;
    uint16_t filtered_ntc;
    enum FilterType current_filter;

    uint8_t is_reading_active; // enables serial output
    uint8_t button_pressed;    // set by EXTI callback
    uint8_t hardware_fault;    // set if ADC DMA fails
    const char *fatal_error_msg;
    uint8_t is_warning_active; // set if warning condition is met
};

extern struct SharedData g_shared_data;

#endif /* SHARED_DATA_H */