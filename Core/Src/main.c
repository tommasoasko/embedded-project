/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// #include <stdio.h>
#include <string.h>
#include "Core/Inc/recruiting/filters/filters.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
	NO_FILTER,
	MOVING_AVERAGE_FILTER,
	RANDOM_NOISE_FILTER,
} filter_type_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint16_t adc_data[2] = {0, 0}; // holds ADC values for potentiometer and NTC

uint8_t rx_char;				// last received character from UART
char cli_buffer[64];			// buffer for command line input
volatile uint8_t cli_idx = 0;	// cli buffer index
volatile uint8_t cli_ready = 0; // flag indicating when a command is ready to be processed

volatile uint8_t is_streaming = 1;				   // flag for streaming/cli mode
volatile uint32_t last_button_press_timestamp = 0; // timestamp of the last button press

volatile filter_type_t current_filter = NO_FILTER; // current filter type
MovingAverageFilter_t moving_average_filter_pot; // moving average filter instance for potentiometer
MovingAverageFilter_t moving_average_filter_ntc; // moving average filter instance for NTC

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
int uint_to_string(uint16_t, char *);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_data, 2);

	// TODO try to  remove this when the DMA buffer is longer
	HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

	// Start UART reception in interrupt mode for one character
	HAL_UART_Receive_IT(&huart2, &rx_char, 1);

	// setup the moving average filter
	Moving_Average_Filter_Init(&moving_average_filter_pot);
	Moving_Average_Filter_Init(&moving_average_filter_ntc);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	uint8_t previous_mode = is_streaming;
	while (1)
	{
		// check if the mode has changed
		if (is_streaming != previous_mode) {
            previous_mode = is_streaming;
            if (is_streaming) {
                char *msg_stream = "\r\n--- STREAMING MODE ON ---\r\n";
                HAL_UART_Transmit(&huart2, (uint8_t *)msg_stream, strlen(msg_stream), 100);
            } else {
                char *msg_cli = "\r\n--- CLI MODE ON ---\r\n"
                                "Available commands:\r\n"
                                "  raw            - Set NO FILTER\r\n"
                                "  moving average - Set MOVING AVERAGE filter\r\n"
                                "  random         - Set RANDOM NOISE filter\r\n"
                                "> ";
                HAL_UART_Transmit(&huart2, (uint8_t *)msg_cli, strlen(msg_cli), 100);
            }
        }

		// apply the filter just to fill the moving average buffer, but don't use the result
		uint16_t filtered_pot = Moving_Average_Filter_Apply(&moving_average_filter_pot, adc_data[0]);
		uint16_t filtered_ntc = Moving_Average_Filter_Apply(&moving_average_filter_ntc, adc_data[1]);

		if (is_streaming)
        {
            cli_ready = 0;
            cli_idx = 0;
            
            char buffer[128];
            int len = 0;

            switch (current_filter) {
                case MOVING_AVERAGE_FILTER:
                    strcpy(buffer, "POT raw: ");
                    len = 9;
                    len += uint_to_string(adc_data[0], buffer + len);
                    
                    strcpy(buffer + len, ", POT filtered:");
                    len += 15;
                    len += uint_to_string(filtered_pot, buffer + len);
                    
                    strcpy(buffer + len, ", NTC raw: ");
                    len += 11;
                    len += uint_to_string(adc_data[1], buffer + len);
                    
                    strcpy(buffer + len, ", NTC filtered:");
                    len += 15;
                    len += uint_to_string(filtered_ntc, buffer + len);
                    break;

                case RANDOM_NOISE_FILTER:
                    strcpy(buffer, "POT raw: ");
                    len = 9;
                    len += uint_to_string(adc_data[0], buffer + len);
                    
                    strcpy(buffer + len, ", POT noise: ");
                    len += 13;
                    len += uint_to_string(filtered_pot, buffer + len);
                    
                    strcpy(buffer + len, ", NTC raw: ");
                    len += 11;
                    len += uint_to_string(adc_data[1], buffer + len);
                    
                    strcpy(buffer + len, ", NTC noise: ");
                    len += 13;
                    len += uint_to_string(filtered_ntc, buffer + len);
                    break;

                case NO_FILTER:
                default:
                    strcpy(buffer, "POT raw: ");
                    len = 9;
                    len += uint_to_string(adc_data[0], buffer + len);
                    
                    strcpy(buffer + len, ", NTC raw: ");
                    len += 11;
                    len += uint_to_string(adc_data[1], buffer + len);
                    break;
            }

            strcpy(buffer + len, "\r\n");
            len += 2;

            HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, 100);
            HAL_Delay(100);
        }
		else
		{
			if (cli_ready)
			{
				// set the filter based on the command received
                char *reply;
                if (strcmp(cli_buffer, "raw") == 0)
                {
                    current_filter = NO_FILTER;
                    reply = "Filter set to RAW\r\n"
							"> ";
                }
                else if (strcmp(cli_buffer, "moving average") == 0)
                {
                    current_filter = MOVING_AVERAGE_FILTER;
                    reply = "Filter set to MOVING AVERAGE\r\n"
							"> ";
                }
                else if (strcmp(cli_buffer, "random") == 0)
                {
                    current_filter = RANDOM_NOISE_FILTER;
                    reply = "Filter set to RANDOM NOISE\r\n"
							"> ";
				}
                else
                {
                    reply = "Unknown command\r\n"
							"> ";
                }
                
                HAL_UART_Transmit(&huart2, (uint8_t *)reply, strlen(reply), 100);

                cli_ready = 0; // reset the flag
                cli_idx = 0;   // reset the index for the next command
			}
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_13)
	{
		uint32_t current_time = HAL_GetTick(); // get current time

		// debounce check
		if (current_time - last_button_press_timestamp > 200)
		{												// TODO check if it's a good debounce time and remove magic number
			is_streaming = !is_streaming;				// toggle mode
			last_button_press_timestamp = current_time; // update timestamp
		}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2)
	{
		if (!is_streaming)
		{
			HAL_UART_Transmit(&huart2, &rx_char, 1, 10); // local ECHO
	
			// if the received character is a \n or \r, we consider the command complete and set the flag
			if (rx_char == '\r' || rx_char == '\n')
			{
				if (cli_idx > 0)
				{
					cli_buffer[cli_idx] = '\0';
					cli_ready = 1;
				}
			}
			// otherwise save the character in the buffer if there's space
			else if (cli_idx < sizeof(cli_buffer) - 1)
			{
				cli_buffer[cli_idx++] = rx_char;
			}
		}

		// restart UART reception for the next character
		HAL_UART_Receive_IT(&huart2, &rx_char, 1);
	}
}


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

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */