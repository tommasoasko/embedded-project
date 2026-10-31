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
#include "Core/Inc/recruiting/fsm/fsm_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
Context_t fsm_context;
volatile uint16_t adc_data[2] = {0, 0}; // holds ADC values for potentiometer and NTC

char cli_buffer[64];
volatile uint8_t cli_idx = 0;
static uint8_t rx_char = 0;

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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

	// HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_data, 2);

	// // TODO try to  remove this when the DMA buffer is longer
	// HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

	// Start UART reception in interrupt mode for one character
	HAL_UART_Receive_IT(&huart2, &rx_char, 1);

	// reset the context
	memset(&fsm_context, 0, sizeof(Context_t));
	fsm_context.adc_data = adc_data;
	fsm_context.current_filter = NO_FILTER;
	fsm_context.state_just_changed = 1;

	Moving_Average_Filter_Pot_Init(&fsm_context.moving_average_filter_pot);
	Moving_Average_Filter_NTC_Init(&fsm_context.moving_average_filter_ntc);

	FSM_Init(&fsm_context);

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		fsm_context.filtered_pot = Moving_Average_Filter_Pot_Apply(&fsm_context.moving_average_filter_pot, adc_data[0]);
		fsm_context.filtered_ntc = Moving_Average_Filter_NTC_Apply(&fsm_context.moving_average_filter_ntc, adc_data[1]);

		FSM_Run(&fsm_context);

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
		FSM_TriggerButtonPress(&fsm_context);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2)
	{
		HAL_UART_Transmit(&huart2, &rx_char, 1, 10);
        if (rx_char == '\r') HAL_UART_Transmit(&huart2, (uint8_t *)"\n", 1, 10);

        if (rx_char == '\r' || rx_char == '\n')
        {
            if (cli_idx > 0)
            {
                cli_buffer[cli_idx] = '\0';
                
                // cli command parsing
                if (strcmp(cli_buffer, "run") == 0) {
                    fsm_context.cli_command_run = 1; // Trigger per l'FSM
                } 
                else if (strcmp(cli_buffer, "raw") == 0) {
                    fsm_context.current_filter = NO_FILTER;
                    HAL_UART_Transmit(&huart2, (uint8_t *)"Filter: RAW\r\n> ", 15, 100);
                }
                else if (strcmp(cli_buffer, "moving average") == 0) {
                    fsm_context.current_filter = MOVING_AVERAGE_FILTER;
                    HAL_UART_Transmit(&huart2, (uint8_t *)"Filter: MOVING AVERAGE\r\n> ", 26, 100);
                }
				else if (strcmp(cli_buffer, "random") == 0) {
					fsm_context.current_filter = RANDOM_NOISE_FILTER;
					HAL_UART_Transmit(&huart2, (uint8_t *)"Filter: RANDOM NOISE\r\n> ", 24, 100);
				}
				else {
					HAL_UART_Transmit(&huart2, (uint8_t *)"Unknown command\r\n> ", 20, 100);
				}
                
                cli_idx = 0;
            }
        }
        else if (rx_char >= 32 && rx_char <= 126)
        {
            if (cli_idx < sizeof(cli_buffer) - 1)
            {
                cli_buffer[cli_idx++] = rx_char;
            }
        }

        HAL_UART_Receive_IT(&huart2, &rx_char, 1);
	}
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
