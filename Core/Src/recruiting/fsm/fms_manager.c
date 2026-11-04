#include "Core/Inc/recruiting/fsm/fsm_manager.h"
#include "adc.h"

struct FSMHandler system_fsm;

/**
 * @brief Routine function for the POST state, responsible for performing the Power-On Self-Test (POST),
 * going to the WAITING state if successful, or transitioning to the FATAL state if a hardware fault is detected.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_post(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- POST STATE---\r\n"
                        "Performing Power-On Self-Test (POST)...\r\n";
        bsp_uart_print(msg_cli);

        bsp_adc_disable_dma_irq();
        if (bsp_adc_start_dma((uint32_t *)ctx->adc_data, 2) != 0)
        {
            ctx->fatal_error_msg = "ERROR: HAL_ADC_Start_DMA failed (HAL_ERROR/BUSY)\r\n";
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_FATAL);
            return;
        }
        

        ctx->state_entry_time = current_time;
        return;
    }

    // wait 50ms before checking the ADC values to ensure they are stable
    if (current_time - ctx->state_entry_time < 50)
    {
        return;
    }

    // diagnostic bounds check
    uint16_t pot_raw = ctx->adc_data[0];
    uint16_t ntc_raw = ctx->adc_data[1];

    // TODO check if the bounds are useful
    uint8_t pot_fault = (pot_raw <= 10 || pot_raw >= 4085);
    uint8_t ntc_fault = (ntc_raw <= 10 || ntc_raw >= 4085);

    if (pot_fault && ntc_fault)
    {
        ctx->fatal_error_msg = "ERROR: potentiometer and NTC thermistor out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    }
    else if (pot_fault)
    {
        ctx->fatal_error_msg = "ERROR: potentiometer out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    }
    else if (ntc_fault)
    {
        ctx->fatal_error_msg = "ERROR: NTC thermistor out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    }
    else
    {
        // sensors are within bounds, transition to WAITING state
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_WAITING);
    }
}

/**
 * @brief Routine function for the WAITING state, responsible for handling user input and transitioning to the LISTENING state.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_waiting(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    // set board LED PWM, reset timers
    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- WAITING STATE: CLI MODE ON ---\r\n"
                        "Type 'run' to start listening.\r\n> ";
        bsp_uart_print(msg_cli);

        // configure board LED (500ms, 15% duty cycle)
        bsp_board_led_set_pwm(WAITING_TIM1_AUTORELOAD, WAITING_TIM1_COMPARE);

        // configure external LED (breathing 2 sec)
        bsp_ext_led_set_pwm(WAITING_TIM3_AUTORELOAD, WAITING_TIM3_COMPARE);

        ctx->breathing_duty = 0;
        ctx->breathing_direction = 1;
        ctx->last_breathing_update = current_time;
    }

    // update duty cycle for breathing effect every 10ms
    if (current_time - ctx->last_breathing_update >= 10)
    {
        ctx->last_breathing_update = current_time;

        ctx->breathing_duty += ctx->breathing_direction;

        if (ctx->breathing_duty >= 100)
        {
            ctx->breathing_duty = 100;
            ctx->breathing_direction = -1; // start decreasing
        }
        else if (ctx->breathing_duty <= 0)
        {
            ctx->breathing_duty = 0;
            ctx->breathing_direction = 1; // start increasing
        }

        // apply the new duty cycle to the external LED PWM
        bsp_ext_led_set_duty(ctx->breathing_duty * 10); // scale to [0, 1000] for PWM
    }

    if (ctx->button_pressed || ctx->cli_command_run)
    {
        ctx->button_pressed = 0;
        ctx->cli_command_run = 0;

        ctx->state_just_changed = 1;

        fsm_api_transition_to(&system_fsm, STATE_LISTENING);
    }
}

/**
 * @brief Routine function for the LISTENING state, responsible for streaming sensor data via UART.
 * Transitions to the PAUSE state upon button press or to the WARNING state if a warning condition is met.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_listening(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    // set board LED PWM, reset timers
    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- LISTENING STATE ---\r\n"
                        "Streaming sensor data...\r\n";
        bsp_uart_print(msg_cli);

        // configure board LED: PWM 200ms, duty 80%
        bsp_board_led_set_pwm(LISTENING_TIM1_AUTORELOAD, LISTENING_TIM1_COMPARE);

        // reset timers for UART transmission and warning condition
        ctx->last_uart_tx_time = current_time;
        ctx->warning_condition_start = current_time;
    }

    // every 100ms, send POT and NTC values via UART
    if (current_time - ctx->last_uart_tx_time >= 100)
    {
        ctx->last_uart_tx_time = current_time;

        // send filtered values via UART
        char buffer[100];
        int len = 0;
        switch (ctx->current_filter)
        {
        case MOVING_AVERAGE_FILTER:
            strcpy(buffer, "POT moving average: ");
            len = 20;
            len += uint_to_string(ctx->filtered_pot, buffer + len);

            strcpy(buffer + len, ", NTC moving average: ");
            len += 22;
            len += uint_to_string(ctx->filtered_ntc, buffer + len);
            break;

        case RANDOM_NOISE_FILTER:
            strcpy(buffer, "POT noise: ");
            len = 11;
            len += uint_to_string(Random_Noise_Filter_Apply(ctx->adc_data[0], 20), buffer + len); // TODO idk if 20 is a good level

            strcpy(buffer + len, ", NTC noise: ");
            len += 13;
            len += uint_to_string(Random_Noise_Filter_Apply(ctx->filtered_pot, 20), buffer + len); // TODO idk if 20 is a good level
            break;
        case NO_FILTER:
        default:
            strcpy(buffer, "POT raw: ");
            len = 9;
            len += uint_to_string(ctx->adc_data[0], buffer + len);

            strcpy(buffer + len, ", NTC raw: ");
            len += 11;
            len += uint_to_string(ctx->adc_data[1], buffer + len);
            break;
        }

        strcpy(buffer + len, "\r\n");
        len += 2;
        bsp_uart_print(buffer);
    }

    // set the external LED PWM based on the difference between filtered NTC and POT values
    if (ctx->filtered_ntc > ctx->filtered_pot)
    {
        uint16_t diff = ctx->filtered_ntc - ctx->filtered_pot;
        uint16_t duty_cycle_value = (diff > THRESHOLD) ? 100 : diff;
        duty_cycle_value = (duty_cycle_value * 100) / THRESHOLD;             // scale to [0, 100]
        bsp_ext_led_set_duty(duty_cycle_value * 10); // scale to [0, 1000] for PWM
    }
    else
    {
        // apply 0% PWM
        bsp_ext_led_set_duty(0);
    }

    // check for button press to transition to PAUSE state
    if (ctx->button_pressed)
    {
        ctx->button_pressed = 0;
        ctx->state_just_changed = 1; // set flag to indicate state change
        fsm_api_transition_to(&system_fsm, STATE_PAUSE);
        return;
    }

    // warning condition (5 seconds of filtered NTC > filtered POT + THRESHOLD)
    if ((ctx->filtered_ntc - ctx->filtered_pot) > THRESHOLD)
    {
        if (current_time - ctx->warning_condition_start >= 5000)
        {
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_WARNING);
        }
    }
    else
    {
        // reset the warning condition timer if the condition is not met
        ctx->warning_condition_start = current_time;
    }
}

/** @brief Routine function for the PAUSE state, responsible for handling the pause functionality and transitioning to the LISTENING state.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_pause(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- PAUSE STATE ---\r\n"
                        "Streaming paused. Press button or type 'run' to resume.\r\n";
        bsp_uart_print(msg_cli);

        // configure board LED: PWM 2000ms, duty 50%
        bsp_board_led_set_pwm(PAUSE_TIM1_AUTORELOAD, PAUSE_TIM1_COMPARE);

        // configure external LED (breathing 2 sec)
        bsp_ext_led_set_pwm(PAUSE_TIM3_AUTORELOAD, PAUSE_TIM3_COMPARE);

        ctx->breathing_duty = 0;
        ctx->breathing_direction = 1;
        ctx->last_breathing_update = current_time;
    }

    // update duty cycle for breathing effect every 10ms
    if (current_time - ctx->last_breathing_update >= 10)
    {
        ctx->last_breathing_update = current_time;

        ctx->breathing_duty += ctx->breathing_direction;

        if (ctx->breathing_duty >= 100)
        {
            ctx->breathing_duty = 100;
            ctx->breathing_direction = -1; // start decreasing
        }
        else if (ctx->breathing_duty <= 0)
        {
            ctx->breathing_duty = 0;
            ctx->breathing_direction = 1; // start increasing
        }

        // apply the new duty cycle to the external LED PWM
        bsp_ext_led_set_duty(ctx->breathing_duty * 10); // scale to [0, 1000] for PWM
    }

    if (ctx->button_pressed || ctx->cli_command_run)
    {
        ctx->button_pressed = 0;
        ctx->cli_command_run = 0;

        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_LISTENING);
    }
}

/** @brief Routine function for the WARNING state, responsible for handling warning conditions and transitioning to the FATAL state if necessary.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_warning(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- WARNING STATE ---\r\n"
                        "Warning: NTC value has been higher than POT value for 5 seconds.\r\n";
        bsp_uart_print(msg_cli);

        // reset timers for UART transmission
        ctx->state_entry_time = current_time;
        ctx->last_uart_tx_time = ctx->state_entry_time;
        ctx->btn_hold_time = 0; // reset button hold time

        bsp_board_led_set_duty(WARNING_TIM1_COMPARE);
        bsp_ext_led_set_duty(WARNING_TIM3_COMPARE);

        ctx->streaming_mode = 1; // set streaming mode flag
    }

    if (current_time - ctx->last_uart_tx_time >= 200)
    {
        ctx->last_uart_tx_time = current_time;

        // send warning message via UART
        char *warning_msg = "WARNING\r\n";
        bsp_uart_print(warning_msg);
    }

    if (bsp_is_button_pressed())
    {
        if (ctx->btn_hold_time == 0)
        {
            ctx->btn_hold_time = current_time; // start timing the button hold
        }
        else if (current_time - ctx->btn_hold_time >= 2000)
        { // 2 seconds hold
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_WAITING);
            return;
        }
    }
    else
    {
        ctx->btn_hold_time = 0; // reset button hold time if button is released
    }

    if (current_time - ctx->state_entry_time >= 10000)
    {
        ctx->state_just_changed = 1;
        ctx->fatal_error_msg = "ERROR: Timeout in Warning state (10 seconds elapsed)\r\n";
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
        return;
    }
}

/**
 * @brief Routine function for the FATAL state, responsible for handling fatal errors and system shutdown.
 *
 * @param data Pointer to the context structure containing system state and data
 */
void routine_fatal(void *data)
{
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- FATAL STATE ---\r\n"
                        "A fatal error has occurred. System halted.\r\n";
        bsp_uart_print(msg_cli);

        if (ctx->fatal_error_msg != NULL)
        {
            bsp_uart_print(ctx->fatal_error_msg);
        }

        bsp_board_led_set_pwm(FATAL_TIM1_AUTORELOAD, FATAL_TIM1_COMPARE);

        bsp_ext_led_set_duty(FATAL_TIM3_COMPARE);

        ctx->state_entry_time = current_time;
        ctx->last_uart_tx_time = ctx->state_entry_time;
        ctx->streaming_mode = 0; // clear streaming mode flag
    }

    if (current_time - ctx->last_uart_tx_time >= 100)
    {
        ctx->last_uart_tx_time = current_time;

        // send fatal error message via UART
        char *fatal_msg = "ERROR: ";
        bsp_uart_print(fatal_msg);
        bsp_uart_print(ctx->fatal_error_msg);
    }

    if (ctx->button_pressed)
    {
        ctx->button_pressed = 0;

        // software reset the system
        bsp_system_reset();
    }
}

/**
 * @brief Transition function for moving from the POST state to the WAITING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_post_to_waiting(void *data) {}

/**
 * @brief Transition function for moving from the POST state to the FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_post_to_fatal(void *data) {}

/**
 * @brief Transition function for moving from the WAITING state to the LISTENING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_waiting_to_listening(void *data) {}

/**
 * @brief Transition function for moving from the WAITING state to the FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_waiting_to_fatal(void *data) {}

/**
 * @brief Transition function for moving from the LISTENING state to the PAUSE state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_listening_to_pause(void *data) {}

/**
 * @brief Transition function for moving from the LISTENING state to the WARNING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_listening_to_warning(void *data) {}

/**
 * @brief Transition function for moving from the LISTENING state to the FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_listening_to_fatal(void *data) {}

/**
 * @brief Transition function for moving from the WARNING state to the FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_warning_to_fatal(void *data) {}

/**
 * @brief Transition function for moving from the WARNING state to the WAITING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_warning_to_waiting(void *data) {}

/**
 * @brief Transition function for moving from the PAUSE state to the LISTENING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_pause_to_listening(void *data) {}

/**
 * @brief Transition function for moving from the PAUSE state to the FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void transition_pause_to_fatal(void *data) {}

/**
 * @brief Array of transitions for the POST state
 *
 * The POST state has two possible transitions:
 * 1. Transition to the WAITING state if the post condition is resolved.
 * 2. Transition to the FATAL state if a fatal error occurs.
 */
struct Transition post_transitions[] = {
    {STATE_WAITING, transition_post_to_waiting},
    {STATE_FATAL, transition_post_to_fatal}};

/**
 * @brief Array of transitions for the WAITING state
 *
 * The WAITING state has two possible transitions:
 * 1. Transition to the LISTENING state if the waiting condition is resolved.
 * 2. Transition to the FATAL state if a fatal error occurs.
 */
struct Transition waiting_transitions[] = {
    {STATE_LISTENING, transition_waiting_to_listening},
    {STATE_FATAL, transition_waiting_to_fatal}};

/**
 * @brief Array of transitions for the LISTENING state
 *
 * The LISTENING state has three possible transitions:
 * 1. Transition to the PAUSE state if the listening condition is interrupted.
 * 2. Transition to the WARNING state if a warning condition is detected.
 * 3. Transition to the FATAL state if a fatal error occurs.
 */
struct Transition listening_transitions[] = {
    {STATE_PAUSE, transition_listening_to_pause},
    {STATE_WARNING, transition_listening_to_warning},
    {STATE_FATAL, transition_listening_to_fatal}};

/**
 * @brief Array of transitions for the PAUSE state
 *
 * The PAUSE state has two possible transitions:
 * 1. Transition to the LISTENING state if the pause condition is resolved.
 * 2. Transition to the FATAL state if a fatal error occurs.
 */
struct Transition pause_transitions[] = {
    {STATE_LISTENING, transition_pause_to_listening},
    {STATE_FATAL, transition_pause_to_fatal}};

/**
 * @brief Array of transitions for the WARNING state
 *
 * The WARNING state has two possible transitions:
 * 1. Transition to the FATAL state if a fatal error occurs.
 * 2. Transition back to the WAITING state if the warning condition is resolved.
 */
struct Transition warning_transitions[] = {
    {STATE_FATAL, transition_warning_to_fatal},
    {STATE_WAITING, transition_warning_to_waiting}};

/**
 * @brief Array of transitions for the FATAL state
 *
 * The FATAL state has no transitions to other states, as it represents a critical error condition.
 */
struct Transition fatal_transitions[] = {
    // No transitions from FATAL state
};

/**
 * @brief Array of states for the FSM
 *
 * Each state is defined with its ID, function to execute, default next state,
 * transitions, and the number of transitions.
 */
struct State states[] = {
    {.id = STATE_POST,
     .function = routine_post,
     .next_default = STATE_POST,
     .transitions = post_transitions,
     .num_transitions = sizeof(post_transitions) / sizeof(post_transitions[0])},
    {.id = STATE_WAITING,
     .function = routine_waiting,
     .next_default = STATE_WAITING,
     .transitions = waiting_transitions,
     .num_transitions = sizeof(waiting_transitions) / sizeof(waiting_transitions[0])},
    {.id = STATE_LISTENING,
     .function = routine_listening,
     .next_default = STATE_LISTENING,
     .transitions = listening_transitions,
     .num_transitions = sizeof(listening_transitions) / sizeof(listening_transitions[0])},
    {.id = STATE_PAUSE,
     .function = routine_pause,
     .next_default = STATE_PAUSE,
     .transitions = pause_transitions,
     .num_transitions = sizeof(pause_transitions) / sizeof(pause_transitions[0])},
    {.id = STATE_WARNING,
     .function = routine_warning,
     .next_default = STATE_WARNING,
     .transitions = warning_transitions,
     .num_transitions = sizeof(warning_transitions) / sizeof(warning_transitions[0])},
    {.id = STATE_FATAL,
     .function = routine_fatal,
     .next_default = STATE_FATAL,
     .transitions = fatal_transitions,
     .num_transitions = sizeof(fatal_transitions) / sizeof(fatal_transitions[0])}};

/**
 * @brief Initialize the FSM
 *
 * @param context Pointer to the context structure containing system state and data
 */
void FSM_Init(Context_t *context)
{
    fsm_api_init(&system_fsm, states, sizeof(states) / sizeof(states[0]), STATE_POST);
}

/**
 * @brief Run the FSM
 *
 * @param context Pointer to the context structure containing system state and data
 */
void FSM_Run(Context_t *context)
{

    fsm_api_run_state(&system_fsm, context);
}

/**
 * @brief Trigger a button press event in the FSM
 *
 * @param context Pointer to the context structure containing system state and data
 */
void FSM_TriggerButtonPress(Context_t *context)
{
    if (context != NULL)
    {
        context->button_pressed = 1;
    }
}

// TODO move this function to a utility file
/**
 * @brief Convert an unsigned integer to a string representation
 *
 * @param num The unsigned integer to convert
 * @param str Pointer to the buffer where the string representation will be stored
 * @return int The length of the resulting string
 */
int uint_to_string(uint16_t num, char *str)
{
    int i = 0;
    if (num == 0)
    {
        str[i++] = '0';
    }
    else
    {
        char temp[10];
        int temp_idx = 0;
        while (num > 0)
        {
            temp[temp_idx++] = (num % 10) + '0';
            num /= 10;
        }
        while (temp_idx > 0)
        {
            str[i++] = temp[--temp_idx];
        }
    }
    str[i] = '\0';
    return i;
}