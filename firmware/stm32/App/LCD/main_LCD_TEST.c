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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "esp8266_uart.h"
#include "sht3x.h"

#include "XPT2046.h"
#include "ILI9341.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENABLE_ESP8266_UART  0  /* Set 1 to enable ESP8266 forecast receive task */
#define ENABLE_SHT3X         0  /* Set 1 to enable SHT3X task                   */
#define ENABLE_LCD_SPI1      1  /* Set 1 to enable LCD task                      */

/* --- Display GPIO (all on GPIOB) ----------------------------------------- */
#define TFT_RST_PORT   GPIOB
#define TFT_RST_PIN    GPIO_PIN_5

#define TFT_CS_PORT    GPIOB
#define TFT_CS_PIN     GPIO_PIN_6

#define TFT_DC_PORT    GPIOB
#define TFT_DC_PIN     GPIO_PIN_7

/* --- Touchscreen GPIO ---------------------------------------------------- */
#define TS_CS_PORT     GPIOB
#define TS_CS_PIN      GPIO_PIN_4

#define TS_IRQ_PORT    GPIOB
#define TS_IRQ_PIN     GPIO_PIN_2   /* EXTI2, falling edge, pull-up */

/* --- Touch poll interval -------------------------------------------------- */
#define TOUCH_POLL_MS       20U   /* GPIO check interval                      */
#define TOUCH_DEBOUNCE_MS   50U   /* confirm delay after first detection       */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* --- RTOS thread handles -------------------------------------------------- */
osThreadId_t esp8266TaskHandle;
const osThreadAttr_t esp8266Task_attributes = {
  .name       = "Esp8266Task",
  .stack_size = 256 * 4,
  .priority   = (osPriority_t) osPriorityLow,
};

osThreadId_t sht3xTaskHandle;
const osThreadAttr_t sht3xTask_attributes = {
  .name       = "Sht3xTask",
  .stack_size = 256 * 4,
  .priority   = (osPriority_t) osPriorityBelowNormal,
};

osThreadId_t lcdTaskHandle;
const osThreadAttr_t lcdTask_attributes = {
  .name       = "LCDTask",
  .stack_size = 512 * 4,   /* increased: printf + display buffers */
  .priority   = (osPriority_t) osPriorityBelowNormal,
};

/* --- Touch semaphore ---------------------------------------------------- */
osSemaphoreId_t touchSemHandle;
const osSemaphoreAttr_t touchSem_attributes = {
  .name = "touchSem"
};

/* --- Display / touch driver instances ------------------------------------ */
ILI9341_t3  tft;   /* populated by ILI9341_init() in StartLCDTask          */
XPT2046_t   ts;    /* populated by XPT2046_init() in StartLCDTask           */

/* --- ESP8266 globals ------------------------------------------------------- */
static esp8266_uart_t        esp8266_uart_state;
volatile esp8266_forecast_t  g_esp8266_forecast        = {0};
volatile uint32_t            g_esp8266_frame_count     = 0;
volatile uint32_t            g_esp8266_frame_error_count = 0;
volatile HAL_StatusTypeDef   g_esp8266_last_error      = HAL_OK;
volatile bool                g_esp8266_uart_ready      = false;
volatile bool                g_esp8266_forecast_valid  = false;

/* --- SHT3x globals -------------------------------------------------------- */
static sht3x_t               sht3x_sensor;
volatile sht3x_sample_t      g_sht3x_sample      = {0};
volatile uint32_t            g_sht3x_read_count  = 0;
volatile uint32_t            g_sht3x_error_count = 0;
volatile HAL_StatusTypeDef   g_sht3x_last_error  = HAL_OK;
volatile bool                g_sht3x_sensor_ready = false;
volatile bool                g_sht3x_sample_valid = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void StartEsp8266Task(void *argument);
void StartSht3xTask(void *argument);
void StartLCDTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern esp8266_uart_t esp8266_uart_state;
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
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  touchSemHandle = osSemaphoreNew(1, 0, &touchSem_attributes);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
#if ENABLE_ESP8266_UART
  esp8266TaskHandle = osThreadNew(StartEsp8266Task, NULL, &esp8266Task_attributes);
#endif
#if ENABLE_SHT3X
  sht3xTaskHandle = osThreadNew(StartSht3xTask, NULL, &sht3xTask_attributes);
#endif
#if ENABLE_LCD_SPI1
  lcdTaskHandle = osThreadNew(StartLCDTask, NULL, &lcdTask_attributes);
#endif
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 95;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB5 PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* PB4 (TS_CS) – touch chip-select, start high (deselected) */
  GPIO_InitStruct.Pin   = GPIO_PIN_4;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* --------------------------------------------------------------------------
 * Constants used by application tasks
 * -------------------------------------------------------------------------- */
#define ESP8266_UART_TASK_RX_TIMEOUT_MS  30000U
#define ESP8266_UART_TASK_RETRY_DELAY_MS   100U

#define SHT3X_I2C_ADDRESS     0x44U
#define SHT3X_TASK_PERIOD_MS  5000U
#define SHT3X_TASK_RETRY_DELAY_MS 500U

/* --------------------------------------------------------------------------
 * UART receive interrupt – ESP8266
 * -------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    esp8266_uart_rx_byte(&esp8266_uart_state, esp8266_uart_state.rx_byte);
    HAL_UART_Receive_IT(&huart1, &esp8266_uart_state.rx_byte, 1);
  }
}

/* --------------------------------------------------------------------------
 * HAL_GPIO_EXTI_Callback
 * No touch IRQ handling needed - touch is polled via GPIO + SPI.
 * PC13 (user button) IRQ is cleared here to prevent Default_Handler trap.
 * -------------------------------------------------------------------------- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TS_IRQ_PIN)
    {
        XPT2046_irqHandler(&ts);

        /* Wake the LCD task – use FromISR-safe FreeRTOS call */
        osSemaphoreRelease(touchSemHandle);
    }
    /* PC13 user button – not used */
}
///* --------------------------------------------------------------------------
// * draw touch point
// * -------------------------------------------------------------------------- */
//static void draw_point(int16_t x, int16_t y)
//{
//  ILI9341_fillCircle(&tft, x, y, 5, ILI9341_RED);
//}

/* --------------------------------------------------------------------------
 * SPI baud rate switching helpers
 * Call before talking to each device on the shared bus.
 * -------------------------------------------------------------------------- */
static void spi_set_fast(void)   /* ILI9341: 8 MHz  (prescaler /2 on 16 MHz HSI) */
{
    while (hspi1.State != HAL_SPI_STATE_READY) osDelay(1);
    hspi1.Instance->CR1 &= ~SPI_CR1_BR_Msk;
    hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_2;
}

static void spi_set_slow(void)   /* XPT2046: 1 MHz  (prescaler /16 on 16 MHz HSI) */
{
    while (hspi1.State != HAL_SPI_STATE_READY) osDelay(1);
    hspi1.Instance->CR1 &= ~SPI_CR1_BR_Msk;
    hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_16;
}

void StartLCDTask(void *argument)
{
    (void)argument;

    /* ------------------------------------------------------------------ */
    /* STEP 1: Manual hard reset of display                               */
    /* ------------------------------------------------------------------ */
    HAL_GPIO_WritePin(TFT_CS_PORT,  TFT_CS_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_DC_PORT,  TFT_DC_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
    osDelay(10);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
    osDelay(20);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
    osDelay(150);

    /* ------------------------------------------------------------------ */
    /* STEP 2: Re-init SPI at /2 (8 MHz) for display                     */
    /* ------------------------------------------------------------------ */
    __HAL_RCC_SPI1_FORCE_RESET();
    osDelay(5);
    __HAL_RCC_SPI1_RELEASE_RESET();
    osDelay(5);

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        for (;;)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
            osDelay(100);
        }
    }

    /* ------------------------------------------------------------------ */
    /* Command/data helpers                                               */
    /* ------------------------------------------------------------------ */
    #define TFT_CMD(cmd)  do { \
        HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET); \
        uint8_t _c = (cmd); \
        HAL_SPI_Transmit(&hspi1, &_c, 1, HAL_MAX_DELAY); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET); \
    } while(0)

    #define TFT_DAT(dat)  do { \
        HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET); \
        uint8_t _d = (dat); \
        HAL_SPI_Transmit(&hspi1, &_d, 1, HAL_MAX_DELAY); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET); \
    } while(0)

    /* Helper to fill entire screen with a 16-bit RGB565 color */
    #define TFT_FILL(hi, lo) do { \
        TFT_CMD(0x2C); \
        HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET); \
        uint8_t _px[2] = { (hi), (lo) }; \
        for (uint32_t _i = 0; _i < 76800UL; _i++) \
            HAL_SPI_Transmit(&hspi1, _px, 2, HAL_MAX_DELAY); \
        HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET); \
    } while(0)

    /* ------------------------------------------------------------------ */
    /* STEP 3: Minimal ILI9341 init                                       */
    /* ------------------------------------------------------------------ */
    TFT_CMD(0x01);           /* Software reset */
    osDelay(150);
    TFT_CMD(0x11);           /* Sleep out */
    osDelay(150);
    TFT_CMD(0x3A);           /* Pixel format */
    TFT_DAT(0x55);           /* 16-bit RGB565 */

    /*
     * MADCTL (0x36) controls memory scan direction and color order.
     * Try each value below one at a time until the full screen fills.
     *
     * Portrait  (no MV bit):  use window cols 0-239, rows 0-319
     * Landscape (MV bit set): use window cols 0-319, rows 0-239
     *
     * Common values to try:
     *   0x48  MX+BGR            portrait
     *   0x08  BGR               portrait
     *   0xC8  MY+MX+BGR         portrait flipped
     *   0x88  MY+BGR            portrait flipped
     *   0x68  MV+MX+BGR         landscape
     *   0xE8  MV+MY+MX+BGR      landscape flipped
     *   0x28  MV+BGR            landscape
     *   0xA8  MV+MY+BGR         landscape flipped
     */
    TFT_CMD(0x36);
    TFT_DAT(0x48);           /* <-- START HERE, change if partial fill */

    TFT_CMD(0x29);           /* Display on */
    osDelay(50);

    /* ------------------------------------------------------------------ */
    /* STEP 4: Set address window                                         */
    /*                                                                    */
    /* Use PORTRAIT window if MADCTL has no MV bit (0x48,0x08,0xC8,0x88) */
    /* Use LANDSCAPE window if MADCTL has MV bit  (0x68,0xE8,0x28,0xA8)  */
    /*                                                                    */
    /* Only ONE of these two blocks should be active at a time.          */
    /* ------------------------------------------------------------------ */

    /* --- PORTRAIT window (cols 0-239, rows 0-319) --- */
    TFT_CMD(0x2A);
    TFT_DAT(0x00); TFT_DAT(0x00);   /* start col = 0   */
    TFT_DAT(0x00); TFT_DAT(0xEF);   /* end col   = 239 */
    TFT_CMD(0x2B);
    TFT_DAT(0x00); TFT_DAT(0x00);   /* start row = 0   */
    TFT_DAT(0x01); TFT_DAT(0x3F);   /* end row   = 319 */

    /* --- LANDSCAPE window (cols 0-319, rows 0-239) --- */
    /* Uncomment this block and comment the portrait block above
       if your MADCTL value has the MV bit set (0x68,0xE8,0x28,0xA8):

    TFT_CMD(0x2A);
    TFT_DAT(0x00); TFT_DAT(0x00);   // start col = 0
    TFT_DAT(0x01); TFT_DAT(0x3F);   // end col   = 319
    TFT_CMD(0x2B);
    TFT_DAT(0x00); TFT_DAT(0x00);   // start row = 0
    TFT_DAT(0x00); TFT_DAT(0xEF);   // end row   = 239
    */

    /* ------------------------------------------------------------------ */
    /* STEP 5: Color fill sequence                                         */
    /* Watch the screen — each color should fill the ENTIRE panel.        */
    /* If only part fills, the window or MADCTL is wrong.                 */
    /* ------------------------------------------------------------------ */
    TFT_FILL(0xF8, 0x00);    /* Red   — RGB565 0xF800 */
    osDelay(1500);

    TFT_FILL(0x07, 0xE0);    /* Green — RGB565 0x07E0 */
    osDelay(1500);

    TFT_FILL(0x00, 0x1F);    /* Blue  — RGB565 0x001F */
    osDelay(1500);

    TFT_FILL(0xFF, 0xFF);    /* White — RGB565 0xFFFF */
    osDelay(1500);

    TFT_FILL(0x00, 0x00);    /* Black — RGB565 0x0000 */
    osDelay(1500);

    /* ------------------------------------------------------------------ */
    /* STEP 6: Hang here so you can observe the final state               */
    /* If you reached black screen, all 5 colors filled correctly.        */
    /* Note the MADCTL value and window orientation that worked —         */
    /* that tells you the correct rotation to pass to ILI9341_setRotation */
    /* ------------------------------------------------------------------ */
    for (;;)
        osDelay(1000);
}
/* --------------------------------------------------------------------------
 * ESP8266 task (unchanged from original)
 * -------------------------------------------------------------------------- */
void StartEsp8266Task(void *argument)
{
  (void)argument;

  esp8266_forecast_t latest_forecast;

  if (!esp8266_uart_init(&esp8266_uart_state, &huart1))
  {
    g_esp8266_last_error    = HAL_ERROR;
    g_esp8266_uart_ready    = false;
    g_esp8266_forecast_valid = false;
    for (;;) osDelay(ESP8266_UART_TASK_RETRY_DELAY_MS);
  }

  g_esp8266_uart_ready = true;
  g_esp8266_last_error = HAL_OK;

  for (;;)
  {
    esp8266_uart_clear_forecast_flag(&esp8266_uart_state);

    if (esp8266_uart_receive_frame(&esp8266_uart_state, ESP8266_UART_TASK_RX_TIMEOUT_MS))
    {
      if (esp8266_uart_get_latest_forecast(&esp8266_uart_state, &latest_forecast))
      {
        g_esp8266_forecast           = latest_forecast;
        g_esp8266_frame_count        = esp8266_uart_state.frame_count;
        g_esp8266_frame_error_count  = esp8266_uart_state.frame_error_count;
        g_esp8266_last_error         = HAL_OK;
        g_esp8266_forecast_valid     = true;
      }
    }
    else
    {
      g_esp8266_frame_error_count = esp8266_uart_state.frame_error_count;
    }
  }
}

/* --------------------------------------------------------------------------
 * SHT3x task (unchanged from original)
 * -------------------------------------------------------------------------- */
void StartSht3xTask(void *argument)
{
  (void)argument;

#if ENABLE_SHT3X
  sht3x_sample_t sample;

  for (;;)
  {
    if (!sht3x_sensor.initialized)
    {
      g_sht3x_sensor_ready = sht3x_init(&sht3x_sensor, &hi2c1, SHT3X_I2C_ADDRESS);
      g_sht3x_last_error   = sht3x_sensor.last_error;

      if (!g_sht3x_sensor_ready)
      {
        g_sht3x_sample_valid = false;
        g_sht3x_error_count++;
        osDelay(SHT3X_TASK_RETRY_DELAY_MS);
        continue;
      }
    }

    if (sht3x_read(&sht3x_sensor, &sample))
    {
      g_sht3x_sample       = sample;
      g_sht3x_last_error   = HAL_OK;
      g_sht3x_sensor_ready = true;
      g_sht3x_sample_valid = true;
      g_sht3x_read_count++;
    }
    else
    {
      g_sht3x_last_error   = sht3x_sensor.last_error;
      g_sht3x_sensor_ready = false;
      g_sht3x_sample_valid = false;
      g_sht3x_error_count++;
    }

    osDelay(SHT3X_TASK_PERIOD_MS);
  }
#else
  for (;;) osDelay(1000);
#endif
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM5 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM5)
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
