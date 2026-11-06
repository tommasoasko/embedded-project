#ifndef BSP_INTERFACE_H
#define BSP_INTERFACE_H

#include <stdint.h>

uint32_t bsp_get_time_ms(void);

void bsp_uart_print(const char *msg);

int bsp_adc_start_dma(uint32_t *buffer, uint32_t length);
void bsp_adc_disable_dma_irq(void);

uint8_t bsp_is_button_pressed(void);

void bsp_board_led_set_pwm(uint16_t period_ms, uint16_t high_time_ms);
void bsp_board_led_set_duty(uint16_t duty_value);
void bsp_board_led_stop(void);

void bsp_ext_led_set_pwm(uint16_t period_ms, uint16_t high_time_ms);
void bsp_ext_led_set_duty(uint16_t duty_value);
void bsp_ext_led_stop(void);

void bsp_system_reset(void);

void bsp_uart_listen_start(void);
uint8_t bsp_uart_msg_ready(void);
void bsp_uart_get_msg(char *out_buffer);

#endif // BSP_INTERFACE_H