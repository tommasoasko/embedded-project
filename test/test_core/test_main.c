#include "unity.h"
#include "test/include/fff.h"
#include "eagletrt-fsm-api.h"

#include "Core/Inc/recruiting/filters/filters.h"
#include "Core/Inc/recruiting/fsm/fsm_manager.h"
#include "Core/Inc/recruiting/bsp/bsp_interface.h"

// initialize the FFF framework
DEFINE_FFF_GLOBALS;

// mocks functions from bsp_interface.h
FAKE_VOID_FUNC(bsp_uart_print, const char *);
FAKE_VOID_FUNC(bsp_adc_disable_dma_irq);
FAKE_VOID_FUNC(bsp_uart_get_msg, char *);
FAKE_VALUE_FUNC(uint32_t, bsp_get_time_ms);
FAKE_VALUE_FUNC(uint8_t, bsp_uart_msg_ready);
FAKE_VALUE_FUNC(int, bsp_adc_start_dma, uint32_t *, uint32_t);
FAKE_VOID_FUNC(bsp_board_led_set_pwm, uint16_t, uint16_t);
FAKE_VOID_FUNC(bsp_ext_led_set_pwm, uint16_t, uint16_t);
FAKE_VOID_FUNC(bsp_board_led_set_duty, uint16_t);
FAKE_VOID_FUNC(bsp_ext_led_set_duty, uint16_t);
FAKE_VOID_FUNC(bsp_system_reset);
FAKE_VALUE_FUNC(uint8_t, bsp_is_button_pressed);

extern struct FSMHandler system_fsm;
Context_t ctx;
static uint16_t dummy_adc_buffer[2];

void setUp(void)
{
    FFF_RESET_HISTORY();

    RESET_FAKE(bsp_uart_print);
    RESET_FAKE(bsp_get_time_ms);
    RESET_FAKE(bsp_uart_msg_ready);
    RESET_FAKE(bsp_adc_start_dma);
    RESET_FAKE(bsp_uart_get_msg);
    RESET_FAKE(bsp_adc_disable_dma_irq);
    RESET_FAKE(bsp_board_led_set_pwm);
    RESET_FAKE(bsp_ext_led_set_pwm);
    RESET_FAKE(bsp_board_led_set_duty);
    RESET_FAKE(bsp_ext_led_set_duty);
    RESET_FAKE(bsp_system_reset);
    RESET_FAKE(bsp_is_button_pressed);

    memset(&ctx, 0, sizeof(Context_t));
    memset(&system_fsm, 0, sizeof(struct FSMHandler));

    ctx.adc_data = dummy_adc_buffer;
}

void tearDown(void)
{
    // the setup does all the necessary cleanup, so no additional teardown is needed
}

// UTILS

void custom_bsp_uart_get_msg_run(char *out_buffer) { strcpy(out_buffer, "run"); }
void custom_bsp_uart_get_msg_random(char *out_buffer) { strcpy(out_buffer, "random"); }

void helper_reach_waiting_state(void)
{
    FSM_Init(&ctx);
    ctx.state_just_changed = 1;
    FSM_Run(&ctx); // setup POST state

    bsp_get_time_ms_fake.return_val = 60;
    ctx.adc_data[0] = 2000;
    ctx.adc_data[1] = 2000;
    FSM_Run(&ctx); // go to WAITING state
    FSM_Run(&ctx); // setup WAITING state
}

// TESTS

// test 1: moving average filter math
void test_moving_average_math(void)
{
    // setup the moving average filter
    Moving_Average_Filter_Pot_Init(&ctx.moving_average_filter_pot);

    // apply three samples to the filter
    Moving_Average_Filter_Pot_Apply(&ctx.moving_average_filter_pot, 1000);
    Moving_Average_Filter_Pot_Apply(&ctx.moving_average_filter_pot, 2000);
    uint16_t current_val = Moving_Average_Filter_Pot_Apply(&ctx.moving_average_filter_pot, 3000);

    // assert (1000 + 2000 + 3000) / 3 = 2000
    TEST_ASSERT_EQUAL_UINT16(2000, current_val);
}

// test 2: button press triggers transition from WAITING to LISTENING
void test_fsm_button_triggers_listening(void)
{
    FSM_Init(&ctx);

    // setup POST state
    FSM_Run(&ctx);

    // setup fake time and ADC values to transition to WAITING state
    bsp_get_time_ms_fake.return_val = 60;
    ctx.adc_data[0] = 2000;
    ctx.adc_data[1] = 2000;

    // run FSM to transition to WAITING state
    FSM_Run(&ctx);

    // assert that the FSM is now in the WAITING state
    TEST_ASSERT_EQUAL(STATE_WAITING, fsm_api_get_state(&system_fsm));

    // setup the FSM for the WAITING state
    FSM_Run(&ctx);

    // simulate button press to transition to LISTENING state
    ctx.button_pressed = 1;
    FSM_Run(&ctx);

    // assert that the button_pressed flag has been reset
    TEST_ASSERT_EQUAL_UINT8(0, ctx.button_pressed);

    // assert that the FSM is now in the LISTENING state
    TEST_ASSERT_EQUAL(STATE_LISTENING, fsm_api_get_state(&system_fsm));
}

// test 3: CLI command "run" triggers transition from WAITING to LISTENING
void test_fsm_cli_run_command(void)
{
    helper_reach_waiting_state();

    // simulate receiving the "run" command via UART
    bsp_uart_msg_ready_fake.return_val = 1;
    bsp_uart_get_msg_fake.custom_fake = custom_bsp_uart_get_msg_run;

    FSM_Run(&ctx);

    TEST_ASSERT_EQUAL(STATE_LISTENING, fsm_api_get_state(&system_fsm));
}

// test 4: hardware fault during POST triggers transition to FATAL
void test_post_hardware_fault_triggers_fatal(void)
{
    FSM_Init(&ctx);
    ctx.state_just_changed = 1;
    FSM_Run(&ctx); // Setup POST

    bsp_get_time_ms_fake.return_val = 60;
    ctx.adc_data[0] = 5;    // POT value out of range (< 10)
    ctx.adc_data[1] = 2000; // normal NTC value

    FSM_Run(&ctx); // Run FSM, should detect hardware fault and transition to FATAL state

    // assert that the FSM is now in the FATAL state
    TEST_ASSERT_EQUAL(STATE_FATAL, fsm_api_get_state(&system_fsm));
}

// test 5: CLI command "random" changes filter type in WAITING state
void test_cli_changes_filter_type_in_waiting(void)
{
    helper_reach_waiting_state();

    // simulate receiving the "random" command via UART
    bsp_uart_msg_ready_fake.return_val = 1;
    bsp_uart_get_msg_fake.custom_fake = custom_bsp_uart_get_msg_random;
    FSM_Run(&ctx);

    // assert that the current filter type has been changed to RANDOM_NOISE_FILTER
    TEST_ASSERT_EQUAL(RANDOM_NOISE_FILTER, ctx.current_filter);

    // assert that the FSM is still in the WAITING state
    TEST_ASSERT_EQUAL(STATE_WAITING, fsm_api_get_state(&system_fsm));
}

// test 6: button press in LISTENING state triggers transition to PAUSE state
void test_listening_button_triggers_pause(void)
{
    helper_reach_waiting_state();

    // press button in WAITING state to transition to LISTENING state
    ctx.button_pressed = 1;
    FSM_Run(&ctx); // go to LISTENING state
    FSM_Run(&ctx); // setup LISTENING state

    // press button in LISTENING state
    ctx.button_pressed = 1;
    FSM_Run(&ctx); // process button press in LISTENING state

    // assert that the button_pressed flag has been reset
    TEST_ASSERT_EQUAL_UINT8(0, ctx.button_pressed);

    // assert that the FSM is now in the PAUSE state
    TEST_ASSERT_EQUAL(STATE_PAUSE, fsm_api_get_state(&system_fsm));
}

// test 7: NTC value higher than POT value for 5 seconds triggers transition to WARNING state
void test_listening_anomaly_triggers_warning(void)
{
    helper_reach_waiting_state();

    // simulate "run" command to transition to LISTENING state
    ctx.cli_command_run = 1;
    FSM_Run(&ctx); // go to LISTENING state

    // set the initial time to 100ms
    bsp_get_time_ms_fake.return_val = 100;
    FSM_Run(&ctx); // setup LISTENING state

    // set the time to 5101ms (sufficient time for the timeout)
    bsp_get_time_ms_fake.return_val = 5101;

    // force the filtered values so that NTC is greater than POT (above the threshold)
    ctx.filtered_pot = 1000;
    ctx.filtered_ntc = 4000;

    FSM_Run(&ctx); // check for warning condition and transition to WARNING state

    // assert that the FSM is now in the WARNING state
    TEST_ASSERT_EQUAL(STATE_WARNING, fsm_api_get_state(&system_fsm));
}

// MAIN

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_moving_average_math);
    RUN_TEST(test_fsm_button_triggers_listening);
    RUN_TEST(test_fsm_cli_run_command);
    RUN_TEST(test_listening_button_triggers_pause);
    RUN_TEST(test_cli_changes_filter_type_in_waiting);
    RUN_TEST(test_post_hardware_fault_triggers_fatal);
    RUN_TEST(test_listening_anomaly_triggers_warning);

    return UNITY_END();
}