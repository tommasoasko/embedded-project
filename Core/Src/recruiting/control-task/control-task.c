#include "Core/Inc/recruiting/control-task/control-task.h"
#include "Core/Inc/recruiting/shared-data/shared-data.h"
#include "Core/Inc/recruiting/bsp/bsp-interface.h"

#define THRESHOLD 500

#define WAITING_TIM1_AUTORELOAD 500
#define WAITING_TIM1_COMPARE 75
#define WAITING_TIM3_AUTORELOAD 1000
#define WAITING_TIM3_COMPARE 0
#define LISTENING_TIM1_AUTORELOAD 200
#define LISTENING_TIM1_COMPARE 160
#define WARNING_TIM1_COMPARE 0
#define WARNING_TIM3_COMPARE 0
#define FATAL_TIM1_AUTORELOAD 40
#define FATAL_TIM1_COMPARE 20
#define FATAL_TIM3_COMPARE 0

static struct FSMHandler control_fsm;
static struct ControlContext control_ctx;

void trans_ctrl_post_to_ok(void *data) {}
void trans_ctrl_post_to_fatal(void *data) {}
void trans_ctrl_ok_to_warning(void *data) {}
void trans_ctrl_ok_to_fatal(void *data) {}
void trans_ctrl_warning_to_ok(void *data) {}
void trans_ctrl_warning_to_fatal(void *data) {}

/**
 * @brief Transition array for the POST state
 *
 */
struct Transition ctrl_post_transitions[] = {
    {CONTROL_STATE_OK, trans_ctrl_post_to_ok},
    {CONTROL_STATE_FATAL, trans_ctrl_post_to_fatal}};

/**
 * @brief Transition array for the OK state
 *
 */
struct Transition ctrl_ok_transitions[] = {
    {CONTROL_STATE_WARNING, trans_ctrl_ok_to_warning},
    {CONTROL_STATE_FATAL, trans_ctrl_ok_to_fatal}};

/**
 * @brief Transition array for the WARNING state
 *
 */
struct Transition ctrl_warning_transitions[] = {
    {CONTROL_STATE_OK, trans_ctrl_warning_to_ok},
    {CONTROL_STATE_FATAL, trans_ctrl_warning_to_fatal}};

/**
 * @brief Transition array for the FATAL state
 *
 */
struct Transition ctrl_fatal_transitions[] = {};

/**
 * @brief Routine for handling the POST state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_control_post(void *data)
{
    struct ControlContext *ctx = (struct ControlContext *)data;

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
    }

    if (g_shared_data.hardware_fault)
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&control_fsm, CONTROL_STATE_FATAL);
    }
    else
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&control_fsm, CONTROL_STATE_OK);
    }
}

/**
 * @brief Routine for handling the OK state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_control_ok(void *data)
{
    struct ControlContext *ctx = (struct ControlContext *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        ctx->breathing_duty = 0;
        ctx->breathing_dir = 1;
        ctx->warning_start_time = current_time;
        ctx->last_breathing_update = current_time;
        ctx->was_reading_active = g_shared_data.is_reading_active;

        // set base PWM for OK state (same as WAITING)
        bsp_board_led_set_pwm(WAITING_TIM1_AUTORELOAD, WAITING_TIM1_COMPARE);
        bsp_ext_led_set_pwm(WAITING_TIM3_AUTORELOAD, WAITING_TIM3_COMPARE);
    }

    if (g_shared_data.hardware_fault)
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&control_fsm, CONTROL_STATE_FATAL);
        return;
    }

    if (ctx->was_reading_active != g_shared_data.is_reading_active)
    {
        ctx->was_reading_active = g_shared_data.is_reading_active;

        if (ctx->was_reading_active)
        {
            bsp_board_led_set_pwm(LISTENING_TIM1_AUTORELOAD, LISTENING_TIM1_COMPARE);
            ctx->warning_start_time = current_time;
        }
        else
        {
            bsp_board_led_set_pwm(WAITING_TIM1_AUTORELOAD, WAITING_TIM1_COMPARE);
            bsp_ext_led_set_pwm(WAITING_TIM3_AUTORELOAD, WAITING_TIM3_COMPARE);

            ctx->breathing_duty = 0;
            ctx->breathing_dir = 1;
            ctx->last_breathing_update = current_time;
        }
    }

    // breathing effect for the external LED when readings are not active
    if (!g_shared_data.is_reading_active)
    {
        if (current_time - ctx->last_breathing_update >= 10)
        {
            ctx->last_breathing_update = current_time;

            ctx->breathing_duty += (int16_t)ctx->breathing_dir;

            if (ctx->breathing_duty >= 100)
            {
                ctx->breathing_duty = 100;
                ctx->breathing_dir = -1; // start decreasing
            }
            else if (ctx->breathing_duty <= 0)
            {
                ctx->breathing_duty = 0;
                ctx->breathing_dir = 1; // start increasing
            }

            // apply the new duty cycle to the external LED PWM
            bsp_ext_led_set_duty(ctx->breathing_duty * 10); // scale to [0, 1000] for PWM
        }

        // reset timer warning
        ctx->warning_start_time = current_time;
    }
    else
    {
        // same behavior as in LISTENING state
        if (g_shared_data.filtered_ntc > g_shared_data.filtered_pot)
        {
            if (g_shared_data.filtered_ntc > g_shared_data.filtered_pot)
            {
                uint16_t diff = g_shared_data.filtered_ntc - g_shared_data.filtered_pot;
                uint16_t duty = (diff > THRESHOLD) ? 100 : diff;
                duty = (duty * 100) / THRESHOLD;
                bsp_ext_led_set_duty(duty * 10);
            }

            // check if the warning condition has been met for 5 seconds
            if (current_time - ctx->warning_start_time >= 5000)
            {
                ctx->state_just_changed = 1;
                fsm_api_transition_to(&control_fsm, CONTROL_STATE_WARNING);
                return;
            }
        }
        else
        {
            bsp_ext_led_set_duty(0);
            ctx->warning_start_time = current_time; // reset warning timer if condition is not met
        }
    }
}

/**
 * @brief Routine for handling the WARNING state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_control_warning(void *data)
{
    struct ControlContext *ctx = (struct ControlContext *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        ctx->state_entry_time = current_time;
        ctx->btn_hold_time = 0;
        g_shared_data.is_warning_active = 1;

        bsp_board_led_set_duty(WARNING_TIM1_COMPARE);
        bsp_ext_led_set_duty(WARNING_TIM3_COMPARE);
    }

    if (g_shared_data.hardware_fault)
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&control_fsm, CONTROL_STATE_FATAL);
        return;
    }

    // check if the warning condition is no longer met
    if (bsp_is_button_pressed())
    {
        if (ctx->btn_hold_time == 0)
        {
            ctx->btn_hold_time = current_time;
        }
        else if (current_time - ctx->btn_hold_time >= 2000)
        {
            // if button held for 2 seconds, transition back to OK state
            g_shared_data.button_pressed = 1;
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&control_fsm, CONTROL_STATE_OK);
            return;
        }
    }
    else
    {
        ctx->btn_hold_time = 0;
    }

    // check if 10 seconds have passed in the WARNING state, transition to FATAL if so
    if (current_time - ctx->state_entry_time >= 10000)
    {
        g_shared_data.fatal_error_msg = "ERROR: Timeout in Warning state\r\n";
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&control_fsm, CONTROL_STATE_FATAL);
    }
}

/**
 * @brief Routine for handling the FATAL state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_control_fatal(void *data)
{
    struct ControlContext *ctx = (struct ControlContext *)data;

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        bsp_board_led_set_pwm(FATAL_TIM1_AUTORELOAD, FATAL_TIM1_COMPARE);
        bsp_ext_led_set_duty(FATAL_TIM3_COMPARE);
    }

    // in the FATAL state, the system is halted and requires a reset to recover (software reset via button press)
    if (g_shared_data.button_pressed)
    {
        g_shared_data.button_pressed = 0;
        bsp_system_reset();
    }
}

/**
 * @brief Array of control states
 */
struct State control_states_arr[] = {
    {CONTROL_STATE_POST, routine_control_post, CONTROL_STATE_POST, ctrl_post_transitions, 2},
    {CONTROL_STATE_OK, routine_control_ok, CONTROL_STATE_OK, ctrl_ok_transitions, 2},
    {CONTROL_STATE_WARNING, routine_control_warning, CONTROL_STATE_WARNING, ctrl_warning_transitions, 2},
    {CONTROL_STATE_FATAL, routine_control_fatal, CONTROL_STATE_FATAL, ctrl_fatal_transitions, 0}};

/**
 * @brief Initialize the control task
 */
void control_task_init(void)
{
    control_ctx.state_just_changed = 1;
    fsm_api_init(&control_fsm, control_states_arr, 4, CONTROL_STATE_POST);
}

/**
 * @brief Run the control task routine
 * @param task_id ID of the task to run
 */
void control_task_routine(uint8_t task_id)
{
    fsm_api_run_state(&control_fsm, &control_ctx);
}