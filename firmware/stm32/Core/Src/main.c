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
 * draw calibration target
 * -------------------------------------------------------------------------- */
static void draw_target(int16_t x, int16_t y)
{
    ILI9341_drawFastHLine(&tft, x - 10, y, 20, ILI9341_RED);
    ILI9341_drawFastVLine(&tft, x, y - 10, 20, ILI9341_RED);
    ILI9341_fillCircle(&tft, x, y, 3, ILI9341_RED);
}

/* --------------------------------------------------------------------------
 * lcd_draw_idle  –  paint the waiting screen
 * -------------------------------------------------------------------------- */
static void lcd_draw_idle(void)
{
  ILI9341_fillScreen(&tft, ILI9341_WHITE);
  ILI9341_setTextSize(&tft, 2);
  ILI9341_setTextColorBG(&tft, ILI9341_WHITE, ILI9341_DARKGREY);

  const char *msg = "Touch anywhere";
  uint16_t tw = ILI9341_measureTextWidth (&tft, msg, 0);
  uint16_t th = ILI9341_measureTextHeight(&tft, msg, 0);
  ILI9341_setCursor(&tft,
                    (ILI9341_width (&tft) - (int16_t)tw) / 2,
                    (ILI9341_height(&tft) - (int16_t)th) / 2);
  ILI9341_writeString(&tft, msg);
}



void touchscreen_debug_calibrate(void)
{
    TS_Point p;
    int16_t w = ILI9341_width(&tft);
    int16_t h = ILI9341_height(&tft);
    char text[64];

    /* Store values for each corner */
    int16_t tl_x = 0, tl_y = 0;
    int16_t tr_x = 0, tr_y = 0;
    int16_t br_x = 0, br_y = 0;
    int16_t bl_x = 0, bl_y = 0;

    ILI9341_setTextSize(&tft, 2);

    /* Draw target at (20, 20) */
    draw_target(20, 20);

    /* Draw instruction */
    ILI9341_setCursor(&tft, 20, 50);
    ILI9341_writeString(&tft, "Touch TOP-LEFT corner");
    ILI9341_setCursor(&tft, 20, 80);
    ILI9341_writeString(&tft, "Press firmly");

    /* Wait for touch */
    p = XPT2046_waitTouch(&ts);
    tl_x = p.x;
    tl_y = p.y;

    /* Display the captured values */
    ILI9341_setTextSize(&tft, 2);
    ILI9341_setCursor(&tft, 20, 40);
    snprintf(text, sizeof(text), "TL: X=%d  Y=%d", tl_x, tl_y);
    ILI9341_writeString(&tft, text);
    ILI9341_setCursor(&tft, 20, 70);

    ILI9341_setCursor(&tft, 20, 110);
    ILI9341_writeString(&tft, "Next: TOP-RIGHT");
    osDelay(2000);

    /* === TOP-RIGHT CORNER === */
    draw_target(w - 20, 20);
    ILI9341_setCursor(&tft, w - 100, 50);
    ILI9341_writeString(&tft, "Touch TOP-RIGHT");

    p = XPT2046_waitTouch(&ts);
    tr_x = p.x;
    tr_y = p.y;

    ILI9341_setCursor(&tft, 20, 40);
    snprintf(text, sizeof(text), "TR: X=%d  Y=%d", tr_x, tr_y);
    ILI9341_writeString(&tft, text);
    ILI9341_setCursor(&tft, 20, 70);

    ILI9341_setCursor(&tft, 20, 110);
    ILI9341_writeString(&tft, "Next: BOTTOM-RIGHT");
    osDelay(2000);

    /* === BOTTOM-RIGHT CORNER === */
    draw_target(w - 20, h - 20);
    ILI9341_setCursor(&tft, w - 120, h - 60);
    ILI9341_writeString(&tft, "Touch BOTTOM-RIGHT");

    p = XPT2046_waitTouch(&ts);
    br_x = p.x;
    br_y = p.y;

    ILI9341_setCursor(&tft, 20, 40);
    snprintf(text, sizeof(text), "BR: X=%d  Y=%d", br_x, br_y);
    ILI9341_writeString(&tft, text);
    ILI9341_setCursor(&tft, 20, 70);

    ILI9341_setCursor(&tft, 20, 110);
    ILI9341_writeString(&tft, "Next: BOTTOM-LEFT");
    osDelay(2000);

    /* === BOTTOM-LEFT CORNER === */
    draw_target(20, h - 20);
    ILI9341_setCursor(&tft, 20, h - 60);
    ILI9341_writeString(&tft, "Touch BOTTOM-LEFT");

    p = XPT2046_waitTouch(&ts);
    bl_x = p.x;
    bl_y = p.y;

    ILI9341_setCursor(&tft, 20, 40);
    snprintf(text, sizeof(text), "BL: X=%d  Y=%d", bl_x, bl_y);
    ILI9341_writeString(&tft, text);
    ILI9341_setCursor(&tft, 20, 70);
    osDelay(2000);

    /* === DISPLAY ALL VALUES === */
    ILI9341_setTextSize(&tft, 1);

    ILI9341_setCursor(&tft, 10, 10);
    ILI9341_writeString(&tft, "Raw Touch Values (Rotation 0):");

    ILI9341_setCursor(&tft, 10, 40);
    snprintf(text, sizeof(text), "TL: (%4d, %4d)", tl_x, tl_y);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 60);
    snprintf(text, sizeof(text), "TR: (%4d, %4d)", tr_x, tr_y);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 80);
    snprintf(text, sizeof(text), "BR: (%4d, %4d)", br_x, br_y);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 100);
    snprintf(text, sizeof(text), "BL: (%4d, %4d)", bl_x, bl_y);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 130);
    ILI9341_writeString(&tft, "Press to continue...");

    /* Wait for any touch to continue */
    XPT2046_waitTouch(&ts);

    /* Now calculate calibration based on actual measured values */
    ts.minx = (tl_x + bl_x) / 2;  /* Left side */
    ts.maxx = (tr_x + br_x) / 2;  /* Right side */
    ts.miny = (tl_y + tr_y) / 2;  /* Top side */
    ts.maxy = (bl_y + br_y) / 2;  /* Bottom side */

    /* Ensure min < max */
    if (ts.minx > ts.maxx) {
        int16_t temp = ts.minx;
        ts.minx = ts.maxx;
        ts.maxx = temp;
    }
    if (ts.miny > ts.maxy) {
        int16_t temp = ts.miny;
        ts.miny = ts.maxy;
        ts.maxy = temp;
    }

    /* Display calibration results */
    ILI9341_fillScreen(&tft, ILI9341_BLACK);
    ILI9341_setTextSize(&tft, 1);

    ILI9341_setCursor(&tft, 10, 10);
    ILI9341_writeString(&tft, "Calibration Values:");

    ILI9341_setCursor(&tft, 10, 40);
    snprintf(text, sizeof(text), "minX: %d  maxX: %d", ts.minx, ts.maxx);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 60);
    snprintf(text, sizeof(text), "minY: %d  maxY: %d", ts.miny, ts.maxy);
    ILI9341_writeString(&tft, text);

    ILI9341_setCursor(&tft, 10, 100);
    ILI9341_writeString(&tft, "Press to continue...");

    XPT2046_waitTouch(&ts);
}
/* --------------------------------------------------------------------------
 * LCD task  –  polling mode, no IRQ or semaphore required
 *
 * T_IRQ (PB2) is read as a plain GPIO_Input (active low).
 * XPT2046_touched() checks GPIO first (fast), then confirms with SPI Z read.
 * Poll interval: 20 ms → worst-case 20 ms touch latency (imperceptible).
 * -------------------------------------------------------------------------- */
void StartLCDTask(void *argument)
{
    (void)argument;

    char     text[64];
    TS_Point p;
    int16_t screen_x, screen_y;
    int16_t raw_x, raw_y;

    /* 1. Init display */
    ILI9341_init(&tft,
                 &hspi1,
                 TFT_CS_PORT,  TFT_CS_PIN,
                 TFT_DC_PORT,  TFT_DC_PIN,
                 TFT_RST_PORT, TFT_RST_PIN);
    ILI9341_begin(&tft);
    ILI9341_setRotation(&tft, 3);

    /* 2. Init touch - NO rotation in driver */
    XPT2046_init(&ts,
                 &hspi1,
                 TS_CS_PORT,  TS_CS_PIN,
                 TS_IRQ_PORT, TS_IRQ_PIN);
    XPT2046_begin(&ts);

    touchscreen_full_diagnostic();
    /* Set rotation to 0 - we'll handle transformation manually */
    XPT2046_setRotation(&ts, 0);

    /* 3. Debug calibration - shows raw values for each corner */
    touchscreen_debug_calibrate();

    /* 4. Clear screen and show idle screen */
    ILI9341_fillScreen(&tft, ILI9341_DARKGREY);
    lcd_draw_idle();

    static int16_t oldx = -1, oldy = -1;
    int16_t w = ILI9341_width(&tft);   /* Should be 320 */
    int16_t h = ILI9341_height(&tft);  /* Should be 240 */

    for (;;)
    {
        osSemaphoreAcquire(touchSemHandle, osWaitForever);

        osDelay(30);
        if (!XPT2046_touched(&ts))
            continue;

        p = XPT2046_getPoint(&ts);
        if (p.z < XPT2046_Z_THRESHOLD)
            continue;

        /* Get raw coordinates (no rotation applied) */
        raw_x = p.x;
        raw_y = p.y;

        /* Map raw coordinates to screen coordinates using calibration */
        screen_x = (int32_t)(raw_x - ts.minx) * w / (ts.maxx - ts.minx);
        screen_y = (int32_t)(raw_y - ts.miny) * h / (ts.maxy - ts.miny);

        /* Clamp to screen bounds */
        if (screen_x < 0) screen_x = 0;
        if (screen_x >= w) screen_x = w - 1;
        if (screen_y < 0) screen_y = 0;
        if (screen_y >= h) screen_y = h - 1;

        /* Show raw values at bottom of screen */
        snprintf(text, sizeof(text), "Raw:%4d,%4d Scr:%3d,%3d",
                 raw_x, raw_y, screen_x, screen_y);
        ILI9341_setTextSize(&tft, 1);
        ILI9341_setCursor(&tft, 10, h - 20);
        ILI9341_writeString(&tft, text);

        /* Erase previous target */
        if (oldx != -1)
        {
            ILI9341_drawFastHLine(&tft, oldx - 10, oldy, 20, ILI9341_DARKGREY);
            ILI9341_drawFastVLine(&tft, oldx, oldy - 10, 20, ILI9341_DARKGREY);
            ILI9341_fillCircle(&tft, oldx, oldy, 3, ILI9341_DARKGREY);
        }

        /* Draw new target */
        draw_target(screen_x, screen_y);

        oldx = screen_x;
        oldy = screen_y;

        /* Display coordinates */
        snprintf(text, sizeof(text), "X:%-4d  Y:%-4d", screen_x, screen_y);
        ILI9341_setTextSize(&tft, 2);

        uint16_t tw = ILI9341_measureTextWidth(&tft, text, 0);
        uint16_t th = ILI9341_measureTextHeight(&tft, text, 0);
        ILI9341_setCursor(&tft,
                          (w - (int16_t)tw) / 2,
                          (h - (int16_t)th) / 2);
        ILI9341_writeString(&tft, text);

        /* Wait for finger lift */
        while (XPT2046_touched(&ts))
            osDelay(20);

        /* Drain semaphore tokens */
        while (osSemaphoreAcquire(touchSemHandle, 0) == osOK) {}

        lcd_draw_idle();
        oldx = oldy = -1;
    }
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
