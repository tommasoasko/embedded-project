#include "Core/Inc/recruiting/shared-data/shared-data.h"
#include "Core/Inc/recruiting/bsp/bsp-interface.h"
#include "Core/Inc/recruiting/serial-task/serial-task.h"

/**
 * @brief Convert a uint16_t number to a string
 * @param num The number to convert
 * @param str The string to store the result
 * @return The length of the resulting string
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
            temp[temp_idx++] = (char)((num % 10) + '0');
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

/**
 * @brief Initialize the serial task
 */
void serial_task_init(void) {}

/**
 * @brief Routine for handling the SERIAL task, responsible for sending data over UART when readings are active and no hardware fault is present.
 *
 * @param task_id ID of the task to run
 */
void serial_task_routine(uint8_t task_id)
{
    (void)task_id;

    if (!g_shared_data.is_reading_active || g_shared_data.hardware_fault)
    {
        return;
    }

    char num_buf[10];

    switch (g_shared_data.current_filter)
    {
    case MOVING_AVERAGE_FILTER:
        bsp_uart_print("POT moving average: ");
        uint_to_string(g_shared_data.filtered_pot, num_buf);
        bsp_uart_print(num_buf);
        bsp_uart_print(", NTC moving average: ");
        uint_to_string(g_shared_data.filtered_ntc, num_buf);
        bsp_uart_print(num_buf);
        break;

    case RANDOM_NOISE_FILTER:
        bsp_uart_print("POT noise: ");
        uint_to_string(random_noise_filter_apply(g_shared_data.raw_pot, 20), num_buf);
        bsp_uart_print(num_buf);
        bsp_uart_print(", NTC noise: ");
        uint_to_string(random_noise_filter_apply(g_shared_data.filtered_pot, 20), num_buf);
        bsp_uart_print(num_buf);
        break;

    case NO_FILTER:
    default:
        bsp_uart_print("POT raw: ");
        uint_to_string(g_shared_data.raw_pot, num_buf);
        bsp_uart_print(num_buf);
        bsp_uart_print(", NTC raw: ");
        uint_to_string(g_shared_data.raw_ntc, num_buf);
        bsp_uart_print(num_buf);
        break;
    }

    bsp_uart_print("\r\n");
}