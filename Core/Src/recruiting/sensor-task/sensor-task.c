#include "Core/Inc/recruiting/sensor-task/sensor-task.h"
#include "Core/Inc/recruiting/shared-data/shared-data.h"
#include "Core/Inc/recruiting/bsp/bsp-interface.h"
#include <string.h>

static struct FSMHandler sensor_fsm;
static struct SensorContext sensor_ctx;

/**
 * @brief Transition function from POST to PAUSED state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void trans_sens_post_to_paused(void *data) {}

/**
 * @brief Transition function from POST to FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void trans_sens_post_to_fatal(void *data) {}

/**
 * @brief Transition function from PAUSED to READING state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void trans_sens_paused_to_reading(void *data) {}

/**
 * @brief Transition function from READING to PAUSED state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void trans_sens_reading_to_paused(void *data) {}

/**
 * @brief Transition function from READING to FATAL state
 *
 * @param data Pointer to the context structure containing system state and data
 */
void trans_sens_reading_to_fatal(void *data) {}

/**
 * @brief Array of transitions for the POST state
 */
struct Transition sens_post_transitions[] = {
    {SENSOR_STATE_PAUSED, trans_sens_post_to_paused},
    {SENSOR_STATE_FATAL, trans_sens_post_to_fatal}};

/**
 * @brief Array of transitions for the PAUSED state
 */
struct Transition sens_paused_transitions[] = {
    {SENSOR_STATE_READING, trans_sens_paused_to_reading}};

/**
 * @brief Array of transitions for the READING state
 */
struct Transition sens_reading_transitions[] = {
    {SENSOR_STATE_PAUSED, trans_sens_reading_to_paused},
    {SENSOR_STATE_FATAL, trans_sens_reading_to_fatal}};

/**
 * @brief Array of transitions for the FATAL state
 */
struct Transition sens_fatal_transitions[] = {};

static void routine_sensor_post(void *data)
{
    struct SensorContext *ctx = (struct SensorContext *)data;
    uint32_t current_time = bsp_get_time_ms();

    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        bsp_uart_print("\r\n--- SENSOR POST ---\r\n");

        bsp_adc_disable_dma_irq();
        if (bsp_adc_start_dma((uint32_t *)ctx->adc_data, 2) != 0)
        {
            g_shared_data.fatal_error_msg = "ERROR: ADC DMA failed\r\n";
            g_shared_data.hardware_fault = 1;
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_FATAL);
            return;
        }
        ctx->state_entry_time = current_time;
        return;
    }

    if (current_time - ctx->state_entry_time < 50)
        return;

    if (ctx->adc_data[0] <= 10 || ctx->adc_data[0] >= 4085)
    {
        g_shared_data.fatal_error_msg = "ERROR: Sensor out of bounds\r\n";
        g_shared_data.hardware_fault = 1;
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_FATAL);
    }
    else
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_PAUSED);
    }
}

/**
 * @brief Routine for handling the PAUSED state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_sensor_paused(void *data)
{
    struct SensorContext *ctx = (struct SensorContext *)data;
    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        g_shared_data.is_reading_active = 0;
        bsp_uart_print("\r\n--- SENSOR PAUSED ---\r\nType 'run' or press button to read.\r\n> "
                       "Type 'raw', 'moving average', or 'random' to select the filter type.\r\n> ");
    }

    if (bsp_uart_msg_ready())
    {
        char local_cmd[64];
        bsp_uart_get_msg(local_cmd);
        if (strcmp(local_cmd, "run") == 0)
        {
            g_shared_data.button_pressed = 1;
        }
        else if (strcmp(local_cmd, "raw") == 0)
        {
            g_shared_data.current_filter = NO_FILTER;
            bsp_uart_print("Filter: RAW\r\n> ");
        }
        else if (strcmp(local_cmd, "moving average") == 0)
        {
            g_shared_data.current_filter = MOVING_AVERAGE_FILTER;
            bsp_uart_print("Filter: MOVING AVERAGE\r\n> ");
        }
        else if (strcmp(local_cmd, "random") == 0)
        {
            g_shared_data.current_filter = RANDOM_NOISE_FILTER;
            bsp_uart_print("Filter: RANDOM\r\n> ");
        }
    }

    if (g_shared_data.button_pressed)
    {
        g_shared_data.button_pressed = 0;
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_READING);
    }
}

/**
 * @brief Routine for handling the READING state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_sensor_reading(void *data)
{
    struct SensorContext *ctx = (struct SensorContext *)data;
    if (ctx->state_just_changed)
    {
        ctx->state_just_changed = 0;
        g_shared_data.is_reading_active = 1;
        bsp_uart_print("\r\n--- SENSOR READING ---\r\n");
    }

    if (g_shared_data.hardware_fault)
    {
        ctx->state_just_changed = 1;
        fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_FATAL);
        return;
    }

    // update shared data for the Control FSM and serial task
    g_shared_data.raw_pot = ctx->adc_data[0];
    g_shared_data.raw_ntc = ctx->adc_data[1];
    g_shared_data.filtered_pot = ctx->adc_data[0];
    g_shared_data.filtered_ntc = ctx->adc_data[1];
    if (g_shared_data.current_filter == MOVING_AVERAGE_FILTER)
    {
        g_shared_data.filtered_pot = moving_average_filter_pot_apply(&sensor_ctx.filter_pot, ctx->adc_data[0]);
        g_shared_data.filtered_ntc = moving_average_filter_ntc_apply(&sensor_ctx.filter_ntc, ctx->adc_data[1]);
    }

    if (bsp_uart_msg_ready())
    {
        char local_cmd[64];
        bsp_uart_get_msg(local_cmd);
        if (strcmp(local_cmd, "pause") == 0)
            g_shared_data.button_pressed = 1;
    }

    if (g_shared_data.button_pressed)
    {
        g_shared_data.button_pressed = 0;

        if (!g_shared_data.is_warning_active)
        {
            ctx->state_just_changed = 1;
            fsm_api_transition_to(&sensor_fsm, SENSOR_STATE_PAUSED);
        }
    }
}

/**
 * @brief Routine for handling the FATAL state
 * @param data Pointer to the context structure containing system state and data
 */
static void routine_sensor_fatal(void *data)
{
    // Minimal logic, relies on Control FSM to halt actuators
}

/**
 * @brief Array of states for the sensor FSM
 */
struct State sensor_states_arr[] = {
    {SENSOR_STATE_POST, routine_sensor_post, SENSOR_STATE_POST, sens_post_transitions, 2},
    {SENSOR_STATE_PAUSED, routine_sensor_paused, SENSOR_STATE_PAUSED, sens_paused_transitions, 1},
    {SENSOR_STATE_READING, routine_sensor_reading, SENSOR_STATE_READING, sens_reading_transitions, 2},
    {SENSOR_STATE_FATAL, routine_sensor_fatal, SENSOR_STATE_FATAL, sens_fatal_transitions, 0}};

/**
 * @brief Initialize the sensor task
 */
void sensor_task_init(void)
{
    sensor_ctx.state_just_changed = 1;
    g_shared_data.current_filter = NO_FILTER;
    fsm_api_init(&sensor_fsm, sensor_states_arr, 4, SENSOR_STATE_POST);
}

/**
 * @brief Routine for running the sensor task
 * @param task_id ID of the task to run
 */
void sensor_task_routine(uint8_t task_id)
{
    fsm_api_run_state(&sensor_fsm, &sensor_ctx);
}