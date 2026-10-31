#include "Core/Inc/recruiting/fsm/fsm_manager.h"

#define THRESHOLD 1000 // define a threshold value for the warning condition

struct FSMHandler system_fsm;

void routine_post(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- POST STATE---\r\n"
                        "Performing Power-On Self-Test (POST)...\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);

        if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ctx->adc_data, 2) != HAL_OK) {
            ctx->fatal_error_msg = "ERROR: HAL_ADC_Start_DMA failed (HAL_ERROR/BUSY)\r\n";
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_FATAL);
            return;
        } else {
            HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
        }

        ctx->state_entry_time = current_time;
        return;
    }

    // wait 50ms before checking the ADC values to ensure they are stable 
    if (current_time - ctx->state_entry_time < 50) {
        return;
    }

    // diagnostic bounds check
    uint16_t pot_raw = ctx->adc_data[0];
    uint16_t ntc_raw = ctx->adc_data[1];

    // TODO check if the bounds are correct for your specific hardware, adjust as necessary
    uint8_t pot_fault = (pot_raw <= 10 || pot_raw >= 4085);
    uint8_t ntc_fault = (ntc_raw <= 10 || ntc_raw >= 4085);

    if (pot_fault && ntc_fault) {
        ctx->fatal_error_msg = "ERROR: potentiometer and NTC thermistor out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    } 
    else if (pot_fault) {
        ctx->fatal_error_msg = "ERROR: potentiometer out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    } 
    else if (ntc_fault) {
        ctx->fatal_error_msg = "ERROR: NTC thermistor out of bounds (HW fault)\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
    } 
    else {
        // sensors are within bounds, transition to WAITING state
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_WAITING);
    }
}

void routine_waiting(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    // set board LED PWM, reset timers
    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;
        
        char *msg_cli = "\r\n--- WAITING STATE: CLI MODE ON ---\r\n"
                        "Type 'run' to start listening.\r\n> ";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);

        // Disabilita i sensori (o li ignoreremo semplicemente)
        // HAL_ADC_Stop_DMA(&hadc1); 

        // configure board LED (500ms, 15% duty cycle)
        __HAL_TIM_SET_AUTORELOAD(&htim1, 500);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 75); // 15% of 500 = 75 ,TODO substitute with defines
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        htim1.Instance->EGR = TIM_EGR_UG;

        // configure external LED (breathing 2 sec)
        __HAL_TIM_SET_AUTORELOAD(&htim3, 1000);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COUNTER(&htim3, 0);
        htim3.Instance->EGR = TIM_EGR_UG;

        // // TIM1 (Board LED)
        // __HAL_TIM_SET_AUTORELOAD(&htim1, 3000); 
        // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500); // 50% di 3000 = 1.5s on, 1.5s off
        // __HAL_TIM_SET_COUNTER(&htim1, 0);

        // // TIM3 (External LED)
        // __HAL_TIM_SET_AUTORELOAD(&htim3, 1000);
        // __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        // __HAL_TIM_SET_COUNTER(&htim3, 0);
        
        ctx->breathing_duty = 0;
        ctx->breathing_direction = 1;
        ctx->last_breathing_update = current_time;
    }

    // update duty cycle for breathing effect every 10ms
    if (current_time - ctx->last_breathing_update >= 10) {
        ctx->last_breathing_update = current_time;
        
        ctx->breathing_duty += ctx->breathing_direction;
        
        if (ctx->breathing_duty >= 100) {
            ctx->breathing_duty = 100;
            ctx->breathing_direction = -1; // start decreasing
        } else if (ctx->breathing_duty <= 0) {
            ctx->breathing_duty = 0;
            ctx->breathing_direction = 1;  // start increasing
        }
        
        // apply the new duty cycle to the external LED PWM
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ctx->breathing_duty * 10); // scale to [0, 1000] for PWM
    }

    if (ctx->button_pressed || ctx->cli_command_run) {
        ctx->button_pressed = 0;
        ctx->cli_command_run = 0;
        
        ctx->state_just_changed = 1;
        
        fsm_api_transition_to(&system_fsm, STATE_LISTENING);
    }
}

void routine_listening(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    // set board LED PWM, reset timers
    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- LISTENING STATE ---\r\n"
                        "Streaming sensor data...\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);
        
        // configure board LED: PWM 200ms, duty 80%
        __HAL_TIM_SET_AUTORELOAD(&htim1, 200);
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 160); // 80% of 200 = 160
        
        // reset timers for UART transmission and warning condition
        ctx->last_uart_tx_time = current_time;
        ctx->warning_condition_start = current_time;
    }

    // every 100ms, send POT and NTC values via UART
    if (current_time - ctx->last_uart_tx_time >= 100) {
        ctx->last_uart_tx_time = current_time;
        
        // send filtered values via UART
        char buffer[100];
        int len = 0;
        switch (ctx->current_filter) {
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
        HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, 100);
    }

    // TODO: Implement logic to set the external LED PWM based on the difference between filtered NTC and POT values
    if (ctx->filtered_ntc > ctx->filtered_pot) {
        uint16_t diff = ctx->filtered_ntc - ctx->filtered_pot;
        uint16_t duty_cycle_value = (diff > THRESHOLD) ? 100 : diff;
        duty_cycle_value = (duty_cycle_value * 100) / THRESHOLD; // scale to [0, 100]
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty_cycle_value * 10); // scale to [0, 1000] for PWM
    } else {
        // TODO: apply 0% PWM
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    }

    // check for button press to transition to PAUSE state
    if (ctx->button_pressed) {
        ctx->button_pressed = 0;
        ctx->state_just_changed = 1; // set flag to indicate state change
        fsm_api_transition_to(&system_fsm, STATE_PAUSE);
        return;
    }

    // warning condition (5 seconds of filtered NTC > filtered POT + THRESHOLD)
    if ((ctx->filtered_ntc - ctx->filtered_pot) > THRESHOLD) {
        if (current_time - ctx->warning_condition_start >= 5000) {
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_WARNING);
        }
    } else {
        // reset the warning condition timer if the condition is not met
        ctx->warning_condition_start = current_time; 
    }
}

void routine_pause(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- PAUSE STATE ---\r\n"
                        "Streaming paused. Press button or type 'run' to resume.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);

        // configure board LED: PWM 2000ms, duty 50%
        __HAL_TIM_SET_AUTORELOAD(&htim1, 2000);
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1000); // 50% of 2000 = 1000

                // configure external LED (breathing 2 sec)
        __HAL_TIM_SET_AUTORELOAD(&htim3, 1000);
        __HAL_TIM_SET_COUNTER(&htim3, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
        
        ctx->breathing_duty = 0;
        ctx->breathing_direction = 1;
        ctx->last_breathing_update = current_time;
    }

    // update duty cycle for breathing effect every 10ms
    if (current_time - ctx->last_breathing_update >= 10) {
        ctx->last_breathing_update = current_time;
        
        ctx->breathing_duty += ctx->breathing_direction;
        
        if (ctx->breathing_duty >= 100) {
            ctx->breathing_duty = 100;
            ctx->breathing_direction = -1; // start decreasing
        } else if (ctx->breathing_duty <= 0) {
            ctx->breathing_duty = 0;
            ctx->breathing_direction = 1;  // start increasing
        }
        
        // apply the new duty cycle to the external LED PWM
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ctx->breathing_duty * 10); // scale to [0, 1000] for PWM
    }

    if (ctx->button_pressed || ctx->cli_command_run) {
        ctx->button_pressed = 0;
        ctx->cli_command_run = 0;
        
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&system_fsm, STATE_LISTENING);
    }
}

void routine_warning(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- WARNING STATE ---\r\n"
                        "Warning: NTC value has been higher than POT value for 5 seconds.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);

        // reset timers for UART transmission
        ctx->state_entry_time = current_time;
        ctx->last_uart_tx_time = ctx->state_entry_time;
        ctx->btn_hold_time = 0; // reset button hold time

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);

        ctx->streaming_mode = 1; // set streaming mode flag
    }

    if (current_time - ctx->last_uart_tx_time >= 200) {
        ctx->last_uart_tx_time = current_time;

        // send warning message via UART
        char *warning_msg = "WARNING\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)warning_msg, strlen(warning_msg), 100);
    }

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
        if (ctx->btn_hold_time == 0) {
            ctx->btn_hold_time = current_time; // start timing the button hold
        } else if (current_time - ctx->btn_hold_time >= 2000) { // 2 seconds hold
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&system_fsm, STATE_WAITING);
            return;
        }
    } else {
        ctx->btn_hold_time = 0; // reset button hold time if button is released
    }

    if (current_time - ctx->state_entry_time >= 10000) {
        ctx->state_just_changed = 1;
        ctx->fatal_error_msg = "ERROR: Timeout in Warning state (10 seconds elapsed)\r\n";
        fsm_api_transition_to(&system_fsm, STATE_FATAL);
        return;
    }


}

void routine_fatal(void *data) {
    Context_t *ctx = (Context_t *)data;
    uint32_t current_time = HAL_GetTick();

    if (ctx->state_just_changed) {
        ctx->state_just_changed = 0;

        char *msg_cli = "\r\n--- FATAL STATE ---\r\n"
                        "A fatal error has occurred. System halted.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);

        if (ctx->fatal_error_msg != NULL) {
            HAL_UART_Transmit(&huart2, (uint8_t *)ctx->fatal_error_msg, strlen(ctx->fatal_error_msg), 100);
        }

        __HAL_TIM_SET_AUTORELOAD(&htim1, 40);
        __HAL_TIM_SET_COUNTER(&htim1, 0);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 20);
        
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);

        ctx->state_entry_time = current_time;
        ctx->last_uart_tx_time = ctx->state_entry_time;
        ctx->streaming_mode = 0; // clear streaming mode flag
    }

    if (current_time - ctx->last_uart_tx_time >= 100) {
        ctx->last_uart_tx_time = current_time;

        // send fatal error message via UART
        char *fatal_msg = "ERROR: ";
        HAL_UART_Transmit(&huart2, (uint8_t *)fatal_msg, strlen(fatal_msg), 100);
        HAL_UART_Transmit(&huart2, (uint8_t *)ctx->fatal_error_msg, strlen(ctx->fatal_error_msg), 100);
    }

    if (ctx->button_pressed) {
        ctx->button_pressed = 0;

        // software reset the system
        NVIC_SystemReset();
    }
}

void transition_post_to_waiting(void *data) {}

void transition_post_to_fatal(void *data) {}

void transition_waiting_to_listening(void *data) {}

void transition_waiting_to_fatal(void *data) {}

void transition_listening_to_pause(void *data) {}

void transition_listening_to_warning(void *data) {}

void transition_listening_to_fatal(void *data) {}

void transition_warning_to_fatal(void *data) {}

void transition_warning_to_waiting(void *data) {}

void transition_pause_to_listening(void *data) {}

void transition_pause_to_fatal(void *data) {}

struct Transition post_transitions[] = {
    {STATE_WAITING, transition_post_to_waiting},
    {STATE_FATAL, transition_post_to_fatal}
};

struct Transition waiting_transitions[] = {
    {STATE_LISTENING, transition_waiting_to_listening},
    {STATE_FATAL, transition_waiting_to_fatal}
};

struct Transition listening_transitions[] = {
    {STATE_PAUSE, transition_listening_to_pause},
    {STATE_WARNING, transition_listening_to_warning},
    {STATE_FATAL, transition_listening_to_fatal}
};

struct Transition pause_transitions[] = {
    {STATE_LISTENING, transition_pause_to_listening},
    {STATE_FATAL, transition_pause_to_fatal}
};

struct Transition warning_transitions[] = {
    {STATE_FATAL, transition_warning_to_fatal},
    {STATE_WAITING, transition_warning_to_waiting}
};

struct Transition fatal_transitions[] = {
    // No transitions from FATAL state
};

struct State states[] = {
    {
        .id = STATE_POST,
        .function = routine_post,
        .next_default = STATE_WAITING,
        .transitions = post_transitions,
        .num_transitions = sizeof(post_transitions) / sizeof(post_transitions[0])
    },
    {
        .id = STATE_WAITING,
        .function = routine_waiting,
        .next_default = STATE_WAITING,
        .transitions = waiting_transitions,
        .num_transitions = sizeof(waiting_transitions) / sizeof(waiting_transitions[0])
    },
    {
        .id = STATE_LISTENING,
        .function = routine_listening,
        .next_default = STATE_LISTENING,
        .transitions = listening_transitions,
        .num_transitions = sizeof(listening_transitions) / sizeof(listening_transitions[0])
    },
    {
        .id = STATE_PAUSE,
        .function = routine_pause, // TODO
        .next_default = STATE_PAUSE,
        .transitions = pause_transitions,
        .num_transitions = sizeof(pause_transitions) / sizeof(pause_transitions[0])
    },
    {
        .id = STATE_WARNING,
        .function = routine_warning, // TODO
        .next_default = STATE_WARNING,
        .transitions = warning_transitions,
        .num_transitions = sizeof(warning_transitions) / sizeof(warning_transitions[0])
    },
    {
        .id = STATE_FATAL,
        .function = routine_fatal, // TODO
        .next_default = STATE_FATAL,
        .transitions = fatal_transitions,
        .num_transitions = sizeof(fatal_transitions) / sizeof(fatal_transitions[0])
    }
};


void FSM_Init(Context_t *context) {
    fsm_api_init(&system_fsm, states, sizeof(states) / sizeof(states[0]), STATE_POST);
}

void FSM_Run(Context_t *context) {
    fsm_api_run_state(&system_fsm, context);
}

void FSM_TriggerButtonPress(Context_t *context) {
    if (context != NULL) {
        context->button_pressed = 1;
    }
}

// TODO move this function to a utility file
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