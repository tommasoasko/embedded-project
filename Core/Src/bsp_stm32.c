#include "Core/Inc/recruiting/bsp/bsp_interface.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "adc.h"
#include <string.h>

static char bsp_cli_buffer[64];            // Buffer to store received UART characters
static volatile uint8_t bsp_cli_idx = 0;   // Index for the next character in the buffer
static uint8_t bsp_rx_char = 0;            // Variable to hold the received character
static volatile uint8_t bsp_msg_ready = 0; // Flag to indicate that a complete message has been received

/**
 * @brief Get the current time in milliseconds
 * @return Current time in milliseconds
 */
uint32_t bsp_get_time_ms(void)
{
    return HAL_GetTick();
}

/**
 * @brief Print a message via UART
 * @param msg Message to print
 */
void bsp_uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
}

/**
 * @brief Start ADC conversion using DMA
 * @param buffer Buffer to store ADC values
 * @param length Number of values to read
 * @return 0 on success, -1 on failure
 */
int bsp_adc_start_dma(uint32_t *buffer, uint32_t length)
{
    HAL_StatusTypeDef res = HAL_ADC_Start_DMA(&hadc1, buffer, length);
    if (res != HAL_OK)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Disable ADC DMA interrupt
 */
void bsp_adc_disable_dma_irq(void)
{
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
}

/**
 * @brief Check if the user button is pressed
 * @return 1 if pressed, 0 otherwise
 */
uint8_t bsp_is_button_pressed(void)
{
    // Sulla Nucleo il pulsante blu (PC13) è in configurazione pull-up (0 = premuto)
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief Set the PWM parameters for the board LED
 * @param period_ms PWM period in milliseconds
 * @param high_time_ms PWM high time in milliseconds
 */
void bsp_board_led_set_pwm(uint16_t period_ms, uint16_t high_time_ms)
{
    // HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_AUTORELOAD(&htim1, period_ms);
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, high_time_ms);

    // htim1.Instance->EGR = TIM_EGR_UG;
    // __HAL_TIM_MOE_ENABLE(&htim1);

    // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

/**
 * @brief Set the duty cycle for the board LED
 * @param duty_value Duty cycle value
 */
void bsp_board_led_set_duty(uint16_t duty_value)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_value);
}

/**
 * @brief Stop the board LED PWM
 */
void bsp_board_led_stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}

/**
 * @brief Set the PWM parameters for the external LED
 * @param period PWM period
 * @param high_time PWM high time
 */
void bsp_ext_led_set_pwm(uint16_t period_ms, uint16_t high_time_ms)
{
    // HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_AUTORELOAD(&htim3, period_ms);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, high_time_ms);
    // HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/**
 * @brief Set the duty cycle for the external LED
 * @param duty_value Duty cycle value
 */
void bsp_ext_led_set_duty(uint16_t duty_value)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty_value);
}

/**
 * @brief Stop the external LED PWM
 */
void bsp_ext_led_stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
}

/**
 * @brief Reset the system
 */
void bsp_system_reset(void)
{
    NVIC_SystemReset();
}

/**
 * @brief Start listening for UART messages
 */
void bsp_uart_listen_start(void)
{
    HAL_UART_Receive_IT(&huart2, &bsp_rx_char, 1);
}

/**
 * @brief Check if a complete UART message is ready
 * @return 1 if a message is ready, 0 otherwise
 */
uint8_t bsp_uart_msg_ready(void)
{
    return bsp_msg_ready;
}

/**
 * @brief Get the latest UART message
 * @param out_buffer Buffer to store the retrieved message
 */
void bsp_uart_get_msg(char *out_buffer)
{
    strcpy(out_buffer, bsp_cli_buffer);
    bsp_msg_ready = 0;
    bsp_cli_idx = 0;
}

/**
 * @brief UART receive complete callback
 * @param huart UART handle
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (bsp_rx_char == '\b' || bsp_rx_char == 127)
        {
            if (bsp_cli_idx > 0)
            {
                bsp_cli_idx--;
                
                uint8_t bs_seq[3] = {'\b', ' ', '\b'};
                HAL_UART_Transmit(&huart2, bs_seq, 3, 5); 
            }
        }
        else if (bsp_rx_char == '\r')
        {
            uint8_t crlf[2] = {'\r', '\n'};
            HAL_UART_Transmit(&huart2, crlf, 2, 5);

            bsp_cli_buffer[bsp_cli_idx] = '\0';
            bsp_msg_ready = 1;
        }
        else if (bsp_rx_char == '\n')
        {
        }
        else if (bsp_rx_char >= 32 && bsp_rx_char <= 126)
        {
            if (bsp_cli_idx < sizeof(bsp_cli_buffer) - 1)
            {
                bsp_cli_buffer[bsp_cli_idx++] = bsp_rx_char;
                
                HAL_UART_Transmit(&huart2, &bsp_rx_char, 1, 5);
            }
        }

        HAL_UART_Receive_IT(&huart2, &bsp_rx_char, 1);
    }
}