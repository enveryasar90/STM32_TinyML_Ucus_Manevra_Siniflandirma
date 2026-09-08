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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "maneuver_model.h"
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

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

#define MPU6050_ADDR 0xD0
#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1   0x6B
#define SMPLRT_DIV   0x19
#define ACCEL_CONFIG 0x1C
#define GYRO_CONFIG  0x1B
#define ACCEL_XOUT_H 0x3B

int16_t Accel_X_RAW, Accel_Y_RAW, Accel_Z_RAW;
int16_t Gyro_X_RAW, Gyro_Y_RAW, Gyro_Z_RAW;

float Ax, Ay, Az;
float Gx, Gy, Gz;
float roll = 0.0f;
float pitch = 0.0f;

uint32_t prev_time = 0;
char msg_buf[256];

// Model Girdisi: 75 örnek x 6 kanal = 450 float
#define WINDOW_SIZE 75
float input_window[INPUT_SIZE];
int window_index = 0;
int current_maneuver = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t MPU6050_Init(void) {
    uint8_t check = 0;
    uint8_t data = 0;

    // 1. Sensör kimliğini kontrol et (0x68 dönmeli)
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, WHO_AM_I_REG, 1, &check, 1, 100);

    if (check == 0x68) {
        // 2. Uyku modundan çıkar (PWR_MGMT_1 = 0x00)
        data = 0x00;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, PWR_MGMT_1, 1, &data, 1, 100);

        // 3. Örnekleme frekansı: 1 kHz / (1 + 7) = 125 Hz
        data = 0x07;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, SMPLRT_DIV, 1, &data, 1, 100);

        // 4. İvmeölçer aralığı: +-4g (FS_SEL = 1)
        data = 0x08;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, ACCEL_CONFIG, 1, &data, 1, 100);

        // 5. Jiroskop aralığı: +-500 dps (FS_SEL = 1)
        data = 0x08;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, GYRO_CONFIG, 1, &data, 1, 100);

        return 1; // Başarılı
    }
    return 0; // Sensör bulunamadı
}

void MPU6050_Read_Raw(void) {
    uint8_t raw[14];

    // 14 byte tek seferde çekilir: Ax(2), Ay(2), Az(2), Temp(2), Gx(2), Gy(2), Gz(2)
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, ACCEL_XOUT_H, 1, raw, 14, 100) == HAL_OK) {
        Accel_X_RAW = (int16_t)(raw[0] << 8 | raw[1]);
        Accel_Y_RAW = (int16_t)(raw[2] << 8 | raw[3]);
        Accel_Z_RAW = (int16_t)(raw[4] << 8 | raw[5]);

        Gyro_X_RAW  = (int16_t)(raw[8] << 8 | raw[9]);
        Gyro_Y_RAW  = (int16_t)(raw[10] << 8 | raw[11]);
        Gyro_Z_RAW  = (int16_t)(raw[12] << 8 | raw[13]);
    }
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  char info_buf[64];
  int len;

  len = snprintf(info_buf, sizeof(info_buf), "\r\n--- I2C Taramasi Basliyor ---\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)info_buf, len, 100);

  uint8_t found = 0;
  for (uint16_t addr = 1; addr < 128; addr++) {
      // HAL_I2C_IsDeviceReady adresin 1 bit sola kaydırılmış halini bekler
      if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 10) == HAL_OK) {
          len = snprintf(info_buf, sizeof(info_buf), "Cihaz bulundu! 7-bit Adres: 0x%02X (HAL Adresi: 0x%02X)\r\n", addr, (addr << 1));
          HAL_UART_Transmit(&huart2, (uint8_t*)info_buf, len, 100);
          found = 1;
      }
  }

  if (!found) {
      len = snprintf(info_buf, sizeof(info_buf), "Hicbir I2C cihazi bulunamadi!\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t*)info_buf, len, 100);
  }
  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED2);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (HAL_GetTick() - prev_time >= 20) { // 50 Hz periyot
          prev_time = HAL_GetTick();

          uint8_t raw[14];
          if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, ACCEL_XOUT_H, 1, raw, 14, 20) == HAL_OK)
          {
              Accel_X_RAW = (int16_t)(raw[0] << 8 | raw[1]);
              Accel_Y_RAW = (int16_t)(raw[2] << 8 | raw[3]);
              Accel_Z_RAW = (int16_t)(raw[4] << 8 | raw[5]);
              Gyro_X_RAW  = (int16_t)(raw[8] << 8 | raw[9]);
              Gyro_Y_RAW  = (int16_t)(raw[10] << 8 | raw[11]);
              Gyro_Z_RAW  = (int16_t)(raw[12] << 8 | raw[13]);

              Ax = Accel_X_RAW / 8192.0f;
              Ay = Accel_Y_RAW / 8192.0f;
              Az = Accel_Z_RAW / 8192.0f;
              Gx = Gyro_X_RAW / 65.5f;
              Gy = Gyro_Y_RAW / 65.5f;
              Gz = Gyro_Z_RAW / 65.5f;

              // Filtreleme
              float roll_acc = atan2f(Ay, Az) * 57.29578f;
              float pitch_acc = atan2f(-Ax, sqrtf(Ay * Ay + Az * Az)) * 57.29578f;
              roll = 0.96f * (roll + Gx * 0.02f) + 0.04f * roll_acc;
              pitch = 0.96f * (pitch + Gy * 0.02f) + 0.04f * pitch_acc;

              // Kayan Pencereye (Sliding Window) Yeni Örneği Ekle
              // Sıralama: Ax, Ay, Az, Gx, Gy, Gz
              if (window_index < WINDOW_SIZE) {
                  input_window[window_index * 6 + 0] = Ax;
                  input_window[window_index * 6 + 1] = Ay;
                  input_window[window_index * 6 + 2] = Az;
                  input_window[window_index * 6 + 3] = Gx;
                  input_window[window_index * 6 + 4] = Gy;
                  input_window[window_index * 6 + 5] = Gz;
                  window_index++;
              } else {
                  // Buffer dolduğunda verileri 1 adım sola kaydır (Shift)
                  memmove(&input_window[0], &input_window[6], (INPUT_SIZE - 6) * sizeof(float));
                  input_window[INPUT_SIZE - 6] = Ax;
                  input_window[INPUT_SIZE - 5] = Ay;
                  input_window[INPUT_SIZE - 4] = Az;
                  input_window[INPUT_SIZE - 3] = Gx;
                  input_window[INPUT_SIZE - 2] = Gy;
                  input_window[INPUT_SIZE - 1] = Gz;

                  // --- YAPAY ZEKA ÇIKARIMI (INFERENCE) ---
                  current_maneuver = predict_maneuver(input_window);

                  // --- GELİŞMİŞ ÇIKARIM VE FİLTRELEME ---

                  // 1. Fiziksel Durgunluk Kontrolü (Gyro Eşiği)
                  // Eğer eksenlerdeki dönüş hızlarının toplamı çok düşükse doğrudan Düz Uçuş kabul et
                  float gyro_magnitude = fabsf(Gx) + fabsf(Gy) + fabsf(Gz);

                  if (gyro_magnitude < 25.0f) {
                      current_maneuver = 0; // Durgun/Düz Uçuş
                  } else {
                      // Hareket varken yapay zeka karar versin
                      current_maneuver = predict_maneuver(input_window);
                  }

                  // 2. LED Kontrolleri (D2=PA10, D3=PB3, D4=PB5)
                  if (current_maneuver == 0) {
                      // Düz Uçuş -> D2 Açık
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
                  }
                  else if (current_maneuver == 1) {
                      // Barrel Roll -> D3 Açık
                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
                  }
                  else if (current_maneuver == 2) {
                  // Inside Loop -> D4 Açık
                  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
                  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
                  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
                  }
              }

              // CSV Telemetri Paketi (9. değer artık gerçek yapay zeka tahmini)
              int len = snprintf(msg_buf, sizeof(msg_buf),
                                 "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d\r\n",
                                 roll, pitch, Gx, Gy, Gz, Ax, Ay, Az, current_maneuver);

              HAL_UART_Transmit(&huart2, (uint8_t*)msg_buf, len, 20);
          }
          else
          {
              HAL_I2C_DeInit(&hi2c1);
              HAL_I2C_Init(&hi2c1);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
