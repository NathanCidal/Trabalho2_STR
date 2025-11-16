/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EN_0  GPIOC->BRR = (1 << 7)
#define EN_1  GPIOC->BSRR = (1 << 7)
#define RS_0  GPIOA->BRR = (1 << 9)
#define RS_1  GPIOA->BSRR = (1 << 9)
#define D7_0  GPIOA->BRR = (1 << 8)
#define D7_1  GPIOA->BSRR = (1 << 8)
#define D6_0  GPIOB->BRR = (1 << 14)
#define D6_1  GPIOB->BSRR = (1 << 14)
#define D5_0  GPIOB->BRR = (1 << 4)
#define D5_1  GPIOB->BSRR = (1 << 4)
#define D4_0  GPIOB->BRR = (1 << 5)
#define D4_1  GPIOB->BSRR = (1 << 5)
#define BL_0  GPIOB->BRR = 1
#define BL_1  GPIOB->BSRR = 1

#define LCD_Cursor_Off    0x0C
#define LCD_Cursor_Blink  0x0E
#define LCD_Cursor_Always 0x0F

#define PRINT_DISPLAY  0x01
#define PRINT_TERMINAL 0x02
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* Definitions for taskSemaforo */
osThreadId_t taskSemaforoHandle;
const osThreadAttr_t taskSemaforo_attributes = {
  .name = "taskSemaforo",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for taskServoGate */
osThreadId_t taskServoGateHandle;
const osThreadAttr_t taskServoGate_attributes = {
  .name = "taskServoGate",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for taskSensor */
osThreadId_t taskSensorHandle;
const osThreadAttr_t taskSensor_attributes = {
  .name = "taskSensor",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for taskLCD */
osThreadId_t taskLCDHandle;
const osThreadAttr_t taskLCD_attributes = {
  .name = "taskLCD",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* USER CODE BEGIN PV */
uint8_t print_loc = PRINT_DISPLAY;

/* ADC DMA buffer */
uint32_t adcFSR = 0;

/* Processed FSR value (normalized 0.0–1.0) */
float fsrForce = 0;

/* Mutex optional (recommended if multiple tasks will use fsrForce) */
osMutexId_t fsrMutexHandle;

uint16_t global_Pulse = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
void StartTaskSemaforo(void *argument);
void StartTaskServoGate(void *argument);
void StartTaskSensor(void *argument);
void StartTaskLCD(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void us_delay(int time){
	for(int i = 0; i < time; i++){
		// 7 in the second loop is used in order to reach 1us Delay
		for(int j = 0; j < 7; j++){

		}
	}
}

void LCD_sendData(uint8_t data){
	if(data & 0x08) D7_1; else D7_0;
	if(data & 0x04) D6_1; else D6_0;
	if(data & 0x02) D5_1; else D5_0;
	if(data & 0x01) D4_1; else D4_0;
}

void LCD_wrcom4(uint8_t com4){
	LCD_sendData(com4);
	RS_0;
	EN_1;
	us_delay(5);
	EN_0;
	osDelay(20);
}

void LCD_wrcom(uint8_t com){
	LCD_sendData(com >> 4);
	RS_0;
	EN_1;
	us_delay(5);
	EN_0;
	LCD_sendData(com);
	us_delay(5);
	EN_1;
	us_delay(5);
	EN_0;
	osDelay(20);
}

void LCD_wrchar(uint8_t ch){
	LCD_sendData(ch >> 4);
	RS_1;
	EN_1;
	us_delay(5);
	EN_0;
	LCD_sendData(ch);
	us_delay(5);
	EN_1;
	us_delay(5);
	EN_0;
	osDelay(20);
}

void LCD_backlight(uint8_t light){
	if(light == 0) BL_0;
	else BL_1;
}

void LCD_clear(){
	LCD_wrcom(0x06);
}

void LCD_goto(uint8_t x, uint8_t y){
	uint8_t com = 0x80;
	//16x2 Display
	if(y == 0) com = 0x80;
	if(y == 1) com = 0xC0;
	if(x >= 0 && x < 16) com += x;
	LCD_wrcom(com);
}

void LCD_Init(uint8_t cursor){
	LCD_wrcom4(0x03);
	LCD_wrcom4(0x03);
	LCD_wrcom4(0x03);
	LCD_wrcom4(0x02);
	LCD_wrcom(0x28);
	LCD_wrcom(cursor);
	LCD_wrcom(0x06);
	LCD_wrcom(0x01);
	LCD_backlight(1);
}


//Extra for printf on Display
int __io_putchar(int ch){
	if(print_loc == PRINT_DISPLAY){
		if(ch != '\n'){
			LCD_wrchar((uint8_t)ch);
		}
	}

	if(print_loc == PRINT_TERMINAL){
		if(ch != 0){
			HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, 2);
		}
	}

	return ch;
}
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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  //HAL_ADC_Start_DMA(&hadc1, &adcFSR, 1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);


  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of taskSemaforo */
  taskSemaforoHandle = osThreadNew(StartTaskSemaforo, NULL, &taskSemaforo_attributes);

  /* creation of taskServoGate */
  taskServoGateHandle = osThreadNew(StartTaskServoGate, NULL, &taskServoGate_attributes);

  /* creation of taskSensor */
  //taskSensorHandle = osThreadNew(StartTaskSensor, NULL, &taskSensor_attributes);

  /* creation of taskLCD */
  taskLCDHandle = osThreadNew(StartTaskLCD, NULL, &taskLCD_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 8000-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 400-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 400-1-20;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_GREEN_Pin|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_14|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB14 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_14|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartTaskSemaforo */
/**
  * @brief  Function implementing the taskSemaforo thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTaskSemaforo */
void StartTaskSemaforo(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
	  fsrForce = 0.7;
	  osDelay(5000);
	  fsrForce = 0;
	  osDelay(1000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTaskServoGate */
/**
* @brief Function implementing the taskServoGate thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskServoGate */
	void StartTaskServoGate(void *argument)
	{
	  /* USER CODE BEGIN StartTaskServoGate */
		/* Pulse widths in timer ticks (with 1 MHz timer clock): */
		    const uint16_t SERVO_MIN = 1000;   // 1.0 ms
		    const uint16_t SERVO_MID = 1500;   // 1.5 ms
		    const uint16_t SERVO_MAX = 2000;   // 2.0 ms

		    uint16_t pulse = SERVO_MID;
		    uint32_t maxHoldEndTime = 0;       // end time for 6s max pulse
		    uint8_t gateActive = 0;            // state flag

		    for (;;)
		    {
		        float force = fsrForce; // from sensor task (0–1.0)

		        /* Check for new trigger */
		        if (!gateActive && force >= 0.5f)
		        {
		            gateActive = 1;
		            pulse = SERVO_MAX;
		            maxHoldEndTime = osKernelGetTickCount() + 6000; // 6 seconds
		        }

		        /* If gate is active, check if 6s have passed */
		        if (gateActive)
		        {
		            if (osKernelGetTickCount() >= maxHoldEndTime)
		            {
		                gateActive = 0;
		                pulse = SERVO_MIN; // back to resting MIN
		            }
		        }

		        /* Update PWM every 20ms (50Hz) */
		        global_Pulse = pulse;
		        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);

		        osDelay(200); // 50 Hz update rate
		    }
	  /* USER CODE END StartTaskServoGate */
	}

	/* USER CODE BEGIN Header_StartTaskSensor */
	/**
	* @brief Function implementing the taskSensor thread.
	* @param argument: Not used
	* @retval None
	*/
	/* USER CODE END Header_StartTaskSensor */
	void StartTaskSensor(void *argument)
	{
	  /* USER CODE BEGIN StartTaskSensor */

		//const float VREF = 3.3f;
		//float voltage = 0;
		//float filtered = 0;
		for(;;)
		{
			/*
			voltage = (VREF * adcFSR) / 4095.0f;
			float force = voltage / VREF;  // 0–1.0
			filtered = 0.90f * filtered + 0.10f * force;
			fsrForce = filtered;
			osDelay(10);  // 100 Hz sampling
			 */
			osDelay(1000);
		}
	  /* USER CODE END StartTaskSensor */
	}

/* USER CODE BEGIN Header_StartTaskLCD */
/**
* @brief Function implementing the taskLCD thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskLCD */
void StartTaskLCD(void *argument)
{
  /* USER CODE BEGIN StartTaskLCD */
	 LCD_Init(LCD_Cursor_Off);
	 print_loc = PRINT_DISPLAY;
	 float force;
  /* Infinite loop */
  for(;;)
  {
	 force = 0.7;
	 LCD_goto(0, 0);
	 printf("P=%04d\n", global_Pulse);
	 LCD_goto(0, 1);
	 printf("F=%0.1f\n", force);
	 osDelay(400);
  }
  /* USER CODE END StartTaskLCD */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
