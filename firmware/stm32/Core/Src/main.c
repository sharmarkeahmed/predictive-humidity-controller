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
typedef enum
{
  HUMIDITY_LED_IDLE = 0,
  HUMIDITY_LED_INCREASING,
  HUMIDITY_LED_DECREASING
} HumidityLedState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ENABLE_ESP8266_UART  1
#define ENABLE_SHT3X         1
#define ENABLE_LCD_SPI1      1

/* --- Humidity status LEDs on GPIOC --- */
#define HUMIDITY_LED_GREEN_PORT   GPIOC
#define HUMIDITY_LED_GREEN_PIN    GPIO_PIN_4

#define HUMIDITY_LED_RED_PORT     GPIOC
#define HUMIDITY_LED_RED_PIN      GPIO_PIN_5

/* --- Display GPIO --- */
#define TFT_RST_PORT   GPIOB
#define TFT_RST_PIN    GPIO_PIN_5

#define TFT_CS_PORT    GPIOB
#define TFT_CS_PIN     GPIO_PIN_6

#define TFT_DC_PORT    GPIOB
#define TFT_DC_PIN     GPIO_PIN_7

/* --- Touchscreen GPIO --- */
#define TS_CS_PORT     GPIOB
#define TS_CS_PIN      GPIO_PIN_4

#define TS_IRQ_PORT    GPIOB
#define TS_IRQ_PIN     GPIO_PIN_2

#define TOUCH_POLL_MS       20U
#define TOUCH_DEBOUNCE_MS   50U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

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
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void StartEsp8266Task(void *argument);
void StartSht3xTask(void *argument);
void StartLCDTask(void *argument);
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
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
	HAL_GPIO_WritePin(HUMIDITY_LED_GREEN_PORT, HUMIDITY_LED_GREEN_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(HUMIDITY_LED_RED_PORT,   HUMIDITY_LED_RED_PIN,   GPIO_PIN_RESET);
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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC4 PC5 PC6 PC7
                           PC8 PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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
// Local helper prototypes
static void HumidityLED_SetState(HumidityLedState_t state);
static void HumidityLED_UpdateFromTrend(float current_humidity,
                                        float previous_humidity,
                                        float deadband_percent);

/* --------------------------------------------------------------------------
 * Humidity LED helpers
 * PC4 = Green LED  -> humidity increasing
 * PC5 = Red LED    -> humidity decreasing
 * Both off         -> humidity unchanged / within deadband
 * -------------------------------------------------------------------------- */
static void HumidityLED_SetState(HumidityLedState_t state)
{
  switch (state)
  {
    case HUMIDITY_LED_INCREASING:
      HAL_GPIO_WritePin(HUMIDITY_LED_GREEN_PORT, HUMIDITY_LED_GREEN_PIN, GPIO_PIN_SET);
      HAL_GPIO_WritePin(HUMIDITY_LED_RED_PORT,   HUMIDITY_LED_RED_PIN,   GPIO_PIN_RESET);
      break;

    case HUMIDITY_LED_DECREASING:
      HAL_GPIO_WritePin(HUMIDITY_LED_GREEN_PORT, HUMIDITY_LED_GREEN_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(HUMIDITY_LED_RED_PORT,   HUMIDITY_LED_RED_PIN,   GPIO_PIN_SET);
      break;

    case HUMIDITY_LED_IDLE:
    default:
      HAL_GPIO_WritePin(HUMIDITY_LED_GREEN_PORT, HUMIDITY_LED_GREEN_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(HUMIDITY_LED_RED_PORT,   HUMIDITY_LED_RED_PIN,   GPIO_PIN_RESET);
      break;
  }
}

static void HumidityLED_UpdateFromTrend(float current_humidity,
                                        float previous_humidity,
                                        float deadband_percent)
{
  float delta = current_humidity - previous_humidity;

  if (delta > deadband_percent)
  {
    HumidityLED_SetState(HUMIDITY_LED_INCREASING);
  }
  else if (delta < -deadband_percent)
  {
    HumidityLED_SetState(HUMIDITY_LED_DECREASING);
  }
  else
  {
    HumidityLED_SetState(HUMIDITY_LED_IDLE);
  }
}
/* --------------------------------------------------------------------------
 * Constants used by application tasks
 * -------------------------------------------------------------------------- */
#define ESP8266_UART_TASK_RX_TIMEOUT_MS  30000U
#define ESP8266_UART_TASK_RETRY_DELAY_MS   100U
#define ESP8266_UART_TASK_REQUEST_PERIOD_MS 1000U // one status frame each second

#define SHT3X_I2C_ADDRESS     0x44U
#define SHT3X_TASK_PERIOD_MS  5000U
#define SHT3X_TASK_RETRY_DELAY_MS 500U

/* --------------------------------------------------------------------------
 * UART receive interrupt – ESP8266
 * -------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    esp8266_uart_rx_byte(&esp8266_uart_state, esp8266_uart_state.rx_byte);
    HAL_UART_Receive_IT(&huart2, &esp8266_uart_state.rx_byte, 1);
  }
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    esp8266_uart_tx_complete(&esp8266_uart_state);
  }
}


void StartEsp8266Task(void *argument)
{
  (void)argument;
  #if ENABLE_ESP8266_UART
  esp8266_tx_status_t tx_status;

  g_esp8266_uart_ready = esp8266_uart_init(&esp8266_uart_state, &huart2);
  g_esp8266_last_error = esp8266_uart_state.last_hal_status;

  if (!g_esp8266_uart_ready)
  {
    g_esp8266_forecast_valid = false;
    for (;;)
    {
      osDelay(1000);
    }
  }

  for (;;)
  {
    if (g_sht3x_sample_valid)
    {
      tx_status.humidity_percent = g_sht3x_sample.humidity_percent;
      tx_status.temperature_c    = g_sht3x_sample.temperature_c;
      tx_status.control_signal   = 50.0f;

      if (!esp8266_uart_send_status(&esp8266_uart_state, &tx_status))
      {
        g_esp8266_last_error = esp8266_uart_state.last_hal_status;
      }
    }

    osDelay(1000);
  }
  #endif
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

/* Touch calibration structure */
typedef struct {
    int16_t x_min;      /* Minimum X raw value (usually top-left) */
    int16_t x_max;      /* Maximum X raw value (usually bottom-right) */
    int16_t y_min;      /* Minimum Y raw value (usually top-left) */
    int16_t y_max;      /* Maximum Y raw value (usually bottom-right) */
    bool calibrated;
} TouchCalibration;

TouchCalibration touch_cal = {0};


// BUNCH OF HELPER FUNCTIONS FOR DEBUG UNCOMMENT, NOT CALLED IN MAIN
///* Function to display calibration instructions and wait for touch */
//TS_Point wait_for_touch_with_indicator(XPT2046_t *ts, ILI9341_t3 *tft,
//                                        int16_t target_x, int16_t target_y,
//                                        const char *message)
//{
//    TS_Point point;
//
//    /* Clear area and show instruction */
//    ILI9341_fillRect(tft, 0, tft->_height - 40, tft->_width, 40, ILI9341_BLACK);
//    ILI9341_setTextSize(tft, 1);
//    ILI9341_setTextColorBG(tft, ILI9341_YELLOW, ILI9341_BLACK);
//    ILI9341_setCursor(tft, 10, tft->_height - 30);
//    ILI9341_writeString(tft, message);
//
//    /* Draw target circle */
//    ILI9341_fillCircle(tft, target_x, target_y, 10, ILI9341_RED);
//    ILI9341_drawCircle(tft, target_x, target_y, 12, ILI9341_WHITE);
//
//    /* Wait for touch */
//    while (1) {
//        if (XPT2046_touched(ts)) {
//            point = XPT2046_getPoint(ts);
//
//            /* Show raw values while touching */
//            char raw_str[32];
//            ILI9341_fillRect(tft, 0, tft->_height - 20, 200, 20, ILI9341_BLACK);
//            ILI9341_setTextSize(tft, 1);
//            ILI9341_setTextColorBG(tft, ILI9341_CYAN, ILI9341_BLACK);
//            ILI9341_setCursor(tft, 10, tft->_height - 15);
//            sprintf(raw_str, "Raw: X=%4d Y=%4d", point.x, point.y);
//            ILI9341_writeString(tft, raw_str);
//
//            /* Wait for touch release */
//            while (XPT2046_touched(ts)) {
//                osDelay(10);
//            }
//
//            /* Confirm with user */
//            ILI9341_fillRect(tft, 0, tft->_height - 40, tft->_width, 40, ILI9341_BLACK);
//            ILI9341_setTextColorBG(tft, ILI9341_GREEN, ILI9341_BLACK);
//            ILI9341_setCursor(tft, 10, tft->_height - 30);
//            ILI9341_writeString(tft, "Touch registered! Releasing...");
//            osDelay(500);
//
//            break;
//        }
//        osDelay(10);
//    }
//
//    /* Clear the target circle */
//    ILI9341_fillCircle(tft, target_x, target_y, 12, ILI9341_BLACK);
//
//    return point;
//}
//
///* Calibration routine */
//bool calibrate_touchscreen(XPT2046_t *ts, ILI9341_t3 *tft)
//{
//    TS_Point points[4];
//    int16_t screen_points[4][2] = {
//        {10, 10},           /* Top-left */
//        {tft->_width - 10, 10},  /* Top-right */
//        {10, tft->_height - 10}, /* Bottom-left */
//        {tft->_width - 10, tft->_height - 10}  /* Bottom-right */
//    };
//
//    const char *messages[] = {
//        "Touch the RED circle at TOP-LEFT corner",
//        "Touch the RED circle at TOP-RIGHT corner",
//        "Touch the RED circle at BOTTOM-LEFT corner",
//        "Touch the RED circle at BOTTOM-RIGHT corner"
//    };
//
//    ILI9341_fillScreen(tft, ILI9341_BLACK);
//    ILI9341_setTextSize(tft, 2);
//    ILI9341_setTextColorBG(tft, ILI9341_WHITE, ILI9341_BLACK);
//    ILI9341_setCursor(tft, 10, 10);
//    ILI9341_writeString(tft, "Touch Screen Calibration");
//    ILI9341_setCursor(tft, 10, 40);
//    ILI9341_writeString(tft, "Follow the instructions");
//    osDelay(2000);
//
//    /* Collect calibration points */
//    for (int i = 0; i < 4; i++) {
//        points[i] = wait_for_touch_with_indicator(ts, tft,
//                                                   screen_points[i][0],
//                                                   screen_points[i][1],
//                                                   messages[i]);
//
//        /* Validate raw values are within range */
//        if (points[i].x < 0 || points[i].x > 4095 ||
//            points[i].y < 0 || points[i].y > 4095) {
//            ILI9341_fillScreen(tft, ILI9341_BLACK);
//            ILI9341_setTextColorBG(tft, ILI9341_RED, ILI9341_BLACK);
//            ILI9341_setCursor(tft, 10, 10);
//            ILI9341_writeString(tft, "Calibration Error!");
//            ILI9341_setCursor(tft, 10, 40);
//            ILI9341_writeString(tft, "Invalid touch values");
//            osDelay(2000);
//            return false;
//        }
//    }
//
//    /* Calculate min/max values */
//    touch_cal.x_min = points[0].x;
//    touch_cal.x_max = points[0].x;
//    touch_cal.y_min = points[0].y;
//    touch_cal.y_max = points[0].y;
//
//    for (int i = 1; i < 4; i++) {
//        if (points[i].x < touch_cal.x_min) touch_cal.x_min = points[i].x;
//        if (points[i].x > touch_cal.x_max) touch_cal.x_max = points[i].x;
//        if (points[i].y < touch_cal.y_min) touch_cal.y_min = points[i].y;
//        if (points[i].y > touch_cal.y_max) touch_cal.y_max = points[i].y;
//    }
//
//    /* Add a small margin */
//    touch_cal.x_min -= 20;
//    touch_cal.x_max += 20;
//    touch_cal.y_min -= 20;
//    touch_cal.y_max += 20;
//
//    /* Clamp to valid range */
//    if (touch_cal.x_min < 0) touch_cal.x_min = 0;
//    if (touch_cal.x_max > 4095) touch_cal.x_max = 4095;
//    if (touch_cal.y_min < 0) touch_cal.y_min = 0;
//    if (touch_cal.y_max > 4095) touch_cal.y_max = 4095;
//
//    touch_cal.calibrated = true;
//
//    /* Show calibration results */
//    ILI9341_fillScreen(tft, ILI9341_BLACK);
//    ILI9341_setTextSize(tft, 2);
//    ILI9341_setTextColorBG(tft, ILI9341_GREEN, ILI9341_BLACK);
//    ILI9341_setCursor(tft, 10, 10);
//    ILI9341_writeString(tft, "Calibration Complete!");
//
//    ILI9341_setTextSize(tft, 1);
//    ILI9341_setTextColorBG(tft, ILI9341_WHITE, ILI9341_BLACK);
//    ILI9341_setCursor(tft, 10, 50);
//    char buf[64];
//    sprintf(buf, "X Range: %d - %d", touch_cal.x_min, touch_cal.x_max);
//    ILI9341_writeString(tft, buf);
//    ILI9341_setCursor(tft, 10, 70);
//    sprintf(buf, "Y Range: %d - %d", touch_cal.y_min, touch_cal.y_max);
//    ILI9341_writeString(tft, buf);
//
//    ILI9341_setCursor(tft, 10, 100);
//    ILI9341_writeString(tft, "Touch anywhere to test");
//
//    /* Test calibration */
//    while (1) {
//        if (XPT2046_touched(ts)) {
//            TS_Point test = XPT2046_getPoint(ts);
//
//            /* Convert raw to screen coordinates */
//            int16_t screen_x = (int16_t)((uint32_t)(test.x - touch_cal.x_min) *
//                                         tft->_width / (touch_cal.x_max - touch_cal.x_min));
//            int16_t screen_y = (int16_t)((uint32_t)(test.y - touch_cal.y_min) *
//                                         tft->_height / (touch_cal.y_max - touch_cal.y_min));
//
//            /* Clamp to screen bounds */
//            if (screen_x < 0) screen_x = 0;
//            if (screen_x >= tft->_width) screen_x = tft->_width - 1;
//            if (screen_y < 0) screen_y = 0;
//            if (screen_y >= tft->_height) screen_y = tft->_height - 1;
//
//            /* Show calibrated coordinates */
//            ILI9341_fillRect(tft, 10, 130, 220, 50, ILI9341_BLACK);
//            ILI9341_setTextSize(tft, 2);
//            ILI9341_setTextColorBG(tft, ILI9341_YELLOW, ILI9341_BLACK);
//            ILI9341_setCursor(tft, 10, 130);
//            sprintf(buf, "X:%3d Y:%3d", screen_x, screen_y);
//            ILI9341_writeString(tft, buf);
//
//            /* Draw touch point */
//            ILI9341_fillCircle(tft, screen_x, screen_y, 5, ILI9341_RED);
//
//            /* Wait for touch release */
//            while (XPT2046_touched(ts)) {
//                osDelay(10);
//            }
//
//            /* Clear touch point */
//            ILI9341_fillCircle(tft, screen_x, screen_y, 5, ILI9341_BLACK);
//        }
//        osDelay(10);
//    }
//
//    return true;
//}
//
///* Function to convert raw touch coordinates to screen coordinates */
//bool touch_to_screen(XPT2046_t *ts, int16_t *screen_x, int16_t *screen_y)
//{
//    if (!touch_cal.calibrated) {
//        return false;
//    }
//
//    TS_Point raw = XPT2046_getPoint(ts);
//
//    /* Apply rotation first (if needed) */
//    int16_t raw_x = raw.x;
//    int16_t raw_y = raw.y;
//
//    /* Convert raw values to screen coordinates using calibration */
//    *screen_x = (int16_t)((uint32_t)(raw_x - touch_cal.x_min) *
//                          ILI9341_width(&tft) / (touch_cal.x_max - touch_cal.x_min));
//    *screen_y = (int16_t)((uint32_t)(raw_y - touch_cal.y_min) *
//                          ILI9341_height(&tft) / (touch_cal.y_max - touch_cal.y_min));
//
//    /* Clamp to screen bounds */
//    if (*screen_x < 0) *screen_x = 0;
//    if (*screen_x >= ILI9341_width(&tft)) *screen_x = ILI9341_width(&tft) - 1;
//    if (*screen_y < 0) *screen_y = 0;
//    if (*screen_y >= ILI9341_height(&tft)) *screen_y = ILI9341_height(&tft) - 1;
//
//    return true;
//}

void initLCD(void){
	/* Hard reset */
	    HAL_GPIO_WritePin(TFT_CS_PORT,  TFT_CS_PIN,  GPIO_PIN_SET);
	    HAL_GPIO_WritePin(TFT_DC_PORT,  TFT_DC_PIN,  GPIO_PIN_SET);
	    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
	    osDelay(10);
	    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
	    osDelay(20);
	    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
	    osDelay(150);
}

static void lcd_draw_idle(void)
{
  ILI9341_fillScreen(&tft, ILI9341_BLACK);
  ILI9341_setTextSize(&tft, 2);
  ILI9341_setTextColorBG(&tft, ILI9341_YELLOW, ILI9341_BLACK);

  const char *msg = "Touch anywhere";
  uint16_t tw = ILI9341_measureTextWidth (&tft, msg, 0);
  uint16_t th = ILI9341_measureTextHeight(&tft, msg, 0);
  ILI9341_setCursor(&tft,
                    (ILI9341_width (&tft) - (int16_t)tw) / 2,
                    (ILI9341_height(&tft) - (int16_t)th) / 2);
  ILI9341_writeString(&tft, msg);
}

/* Updated StartLCDTask with calibration */
void StartLCDTask(void *argument)
{
    (void)argument;
    char text[32];

    initLCD();
    /* Init display */
    ILI9341_init(&tft,
                 &hspi1,
                 TFT_CS_PORT,  TFT_CS_PIN,
                 TFT_DC_PORT,  TFT_DC_PIN,
                 TFT_RST_PORT, TFT_RST_PIN);
    ILI9341_begin(&tft);
    ILI9341_setRotation(&tft, 1);
    ILI9341_fillScreen(&tft, ILI9341_BLACK);

    /* Init touch */
    XPT2046_init(&ts,
                 &hspi1,
                 TS_CS_PORT,  TS_CS_PIN,
                 TS_IRQ_PORT, TS_IRQ_PIN);
    XPT2046_begin(&ts);

    /* IMPORTANT: Don't set touch rotation - we'll handle mapping via calibration */
    XPT2046_setRotation(&ts, 1);  /* Raw mode, no rotation */

    /* Run calibration */
//    ILI9341_fillScreen(&tft, ILI9341_BLACK);
//    calibrate_touchscreen(&ts, &tft);

    /* The calibration function runs indefinitely for testing */
    /* If you want to exit calibration after testing, you would need to add a timeout or button press */
    lcd_draw_idle();

     for (;;)
     {

       TS_Point p;

       /* Poll every 20 ms – yields CPU to other tasks between checks */
       osDelay(20);

       /* Fast GPIO pre-check: T_IRQ HIGH = no touch, skip SPI entirely */
       if (!XPT2046_touched(&ts))
         continue;

       /* Debounce: wait 50 ms then confirm touch is still present */
       osDelay(50);
       if (!XPT2046_touched(&ts))
         continue;

       /* Read position – single SPI transaction */
       p = XPT2046_getPoint(&ts);
       if (p.z < XPT2046_Z_THRESHOLD)
         continue;

       /* Map 12-bit ADC (0-4095) → pixel coords */
       int16_t px = (int16_t)((p.x * (uint32_t)ILI9341_width (&tft)) / 4096);
       int16_t py = (int16_t)((p.y * (uint32_t)ILI9341_height(&tft)) / 4096);
       if (px < 0)                     px = 0;
       if (py < 0)                     py = 0;
       if (px >= ILI9341_width (&tft)) px = ILI9341_width (&tft)  - 1;
       if (py >= ILI9341_height(&tft)) py = ILI9341_height(&tft) - 1;

       /* Redraw screen */
       snprintf(text, sizeof(text), "X:%-4d  Y:%-4d", (int)px, (int)py);

       ILI9341_fillScreen(&tft, ILI9341_BLACK);
       ILI9341_setTextSize(&tft, 2);
       ILI9341_setTextColorBG(&tft, ILI9341_YELLOW, ILI9341_BLACK);
       {
         uint16_t tw = ILI9341_measureTextWidth (&tft, text, 0);
         uint16_t th = ILI9341_measureTextHeight(&tft, text, 0);
         ILI9341_setCursor(&tft,
                           (ILI9341_width (&tft) - (int16_t)tw) / 2,
                           (ILI9341_height(&tft) - (int16_t)th) / 2);
       }
       ILI9341_writeString(&tft, text);

//       lcdDrawCursor(); // Create cursor object
       ILI9341_drawFastHLine(&tft, px - 10, py,      21, ILI9341_RED);
       ILI9341_drawFastVLine(&tft, px,      py - 10, 21, ILI9341_RED);
       ILI9341_fillCircle   (&tft, px, py, 4, ILI9341_RED);

       /* Wait for finger-lift */
       do { osDelay(20); } while (XPT2046_touched(&ts));

       /* Restore idle screen */
       lcd_draw_idle();
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
      g_sht3x_sensor_ready = sht3x_init(&sht3x_sensor, &hi2c2, SHT3X_I2C_ADDRESS);
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
  (void)argument;

  float previous_humidity = 0.0f;
  float current_humidity  = 0.0f;
  bool first_sample = true;

  /* Adjust this to avoid flicker from tiny sensor noise */
  const float humidity_deadband = 0.5f;   // percent RH

  for (;;)
  {
    if (g_sht3x_sample_valid)
    {
      current_humidity = g_sht3x_sample.humidity_percent;

      if (first_sample)
      {
        previous_humidity = current_humidity;
        HumidityLED_SetState(HUMIDITY_LED_IDLE);
        first_sample = false;
      }
      else
      {
        HumidityLED_UpdateFromTrend(current_humidity,
                                    previous_humidity,
                                    humidity_deadband);

        previous_humidity = current_humidity;
      }
    }
    else
    {
      /* No valid sensor reading yet -> both LEDs off */
      HumidityLED_SetState(HUMIDITY_LED_IDLE);
    }

    osDelay(250);
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
