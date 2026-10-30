#ifndef FSM_MANAGER_H
#define FSM_MANAGER_H

#include <stdint.h>
#include "usart.h"
#include "tim.h"
#include "eagletrt-fsm-api.h"
#include "Core/Inc/recruiting/filters/filters.h"

typedef enum {
    STATE_POST = 0,
    STATE_WAITING,
    STATE_LISTENING,
    STATE_PAUSE,
    STATE_WARNING,
    STATE_FATAL
} SystemState_t;

typedef struct {
    uint16_t *adc_data;           // DMA buffer for ADC values
    FilterType_t current_filter;  // current filter type
    uint16_t filtered_pot;        // filtered potentiometer value
    uint16_t filtered_ntc;        // filtered NTC value
    
    uint8_t button_pressed;       // flag (0/1) set by the button interrupt
    uint8_t cli_command_run;      // flag (0/1) set when a CLI command is received
    uint8_t streaming_mode;        // flag (0/1) set when in streaming mode
    uint8_t state_just_changed;   // flag (0/1) set when the state has just changed

    uint32_t state_entry_time;     // timestamp when the current state was entered
    uint32_t last_uart_tx_time;   // timestamp of the last UART transmission
    uint32_t warning_condition_start; // timestamp when the warning condition started

    uint32_t last_breathing_update; // timestamp of the last breathing effect update
    int8_t breathing_direction;     // 1 increasing brightness, -1 decreasing brightness
    int16_t breathing_duty;         // [0, 100] current duty cycle for breathing effect

    const char *fatal_error_msg; // pointer to a string describing the fatal error, if any
} Context_t;

void FSM_Init(Context_t *context);
void FSM_Run(Context_t *context);
void FSM_TriggerButtonPress(Context_t *context);

#endif