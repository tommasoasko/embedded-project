#ifndef FSM_MANAGER_H
#define FSM_MANAGER_H

#include <stdint.h>
#include "eagletrt-fsm-api.h"
#include "Core/Inc/recruiting/filters/filters.h"
#include "Core/Inc/recruiting/bsp/bsp_interface.h"

#define THRESHOLD 500 // define a threshold value for the warning condition
#define WAITING_TIM1_AUTORELOAD 500
#define WAITING_TIM1_COMPARE 75
#define WAITING_TIM3_AUTORELOAD 1000
#define WAITING_TIM3_COMPARE 0
#define LISTENING_TIM1_AUTORELOAD 200
#define LISTENING_TIM1_COMPARE 160
#define PAUSE_TIM1_AUTORELOAD 2000
#define PAUSE_TIM1_COMPARE 1000
#define PAUSE_TIM3_AUTORELOAD 1000
#define PAUSE_TIM3_COMPARE 0
#define WARNING_TIM1_COMPARE 0
#define WARNING_TIM3_COMPARE 0
#define FATAL_TIM1_AUTORELOAD 40
#define FATAL_TIM1_COMPARE 20
#define FATAL_TIM3_COMPARE 0

/** @brief Enumerated type for the system states */
typedef enum
{
    STATE_POST = 0,
    STATE_WAITING,
    STATE_LISTENING,
    STATE_PAUSE,
    STATE_WARNING,
    STATE_FATAL
} SystemState_t;

/** @brief Structure containing the context for the finite state machine */
typedef struct
{
    volatile uint16_t *adc_data; // DMA buffer for ADC values
    FilterType_t current_filter; // current filter type
    uint16_t filtered_pot;       // filtered potentiometer value
    uint16_t filtered_ntc;       // filtered NTC value

    MovingAverageFilterPot_t moving_average_filter_pot; // moving average filter for potentiometer
    MovingAverageFilterNTC_t moving_average_filter_ntc; // moving average filter for NTC

    uint8_t button_pressed;     // flag (0/1) set by the button interrupt
    uint8_t cli_command_run;    // flag (0/1) set when a CLI command is received
    uint8_t cli_enabled;        // flag (0/1) set when CLI is enabled
    uint8_t state_just_changed; // flag (0/1) set when the state has just changed

    uint32_t state_entry_time;        // timestamp when the current state was entered
    uint32_t last_uart_tx_time;       // timestamp of the last UART transmission
    uint32_t warning_condition_start; // timestamp when the warning condition started
    uint32_t btn_hold_time;           // timestamp of the last button press, for debouncing

    uint32_t last_breathing_update; // timestamp of the last breathing effect update
    int8_t breathing_direction;     // 1 increasing brightness, -1 decreasing brightness
    int16_t breathing_duty;         // [0, 100] current duty cycle for breathing effect

    const char *fatal_error_msg; // pointer to a string describing the fatal error, if any

} Context_t;

void FSM_Init(Context_t *context);
void FSM_Run(Context_t *context);
void FSM_TriggerButtonPress(Context_t *context);

int uint_to_string(uint16_t num, char *str);

#endif