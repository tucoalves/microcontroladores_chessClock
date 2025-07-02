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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "max7219.h"


#define FLASH_USER_START_ADDR   ((uint32_t)0x0800FC00)
#define MAX7219_CS_PORT GPIOA
#define MAX7219_CS_PIN GPIO_PIN_12

typedef struct {
    uint32_t player1_timer;
    uint32_t player2_timer;
    uint32_t last_timer;
    uint8_t  select_minute;
    uint8_t  enable_p1;
    uint8_t  enable_p2;
    uint32_t checksum;  // Verificação de integridade
} AppData_t;
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
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
int PLAYER_1_TIMER = 120, PLAYER_2_TIMER = 120;
int LAST_TIMER = 10;
bool SELECT_MINUTE = false;
bool ENABLE_PLAYER_1_TIMER = false, ENABLE_PLAYER_2_TIMER = false;
volatile bool IS_CONFIG_MODE = false;

// Estados anteriores dos botões
GPIO_PinState prev_PT1 = GPIO_PIN_RESET;
GPIO_PinState prev_PT2 = GPIO_PIN_RESET;
GPIO_PinState prev_CONF = GPIO_PIN_RESET;
GPIO_PinState prev_CONF_SALVAR = GPIO_PIN_RESET;
GPIO_PinState prev_SEL = GPIO_PIN_RESET;
GPIO_PinState prev_INC = GPIO_PIN_RESET;
GPIO_PinState prev_DEC = GPIO_PIN_RESET;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void update_display(){
	int p1_minutes = PLAYER_1_TIMER / 60;
	int p1_seconds = PLAYER_1_TIMER % 60;

	int p2_minutes = PLAYER_2_TIMER / 60;
	int p2_seconds = PLAYER_2_TIMER % 60;

	const uint8_t digit_to_segment[] = {
	        MAX7219_SYM_0,
	        MAX7219_SYM_1,
	        MAX7219_SYM_2,
	        MAX7219_SYM_3,
	        MAX7219_SYM_4,
	        MAX7219_SYM_5,
	        MAX7219_SYM_6,
	        MAX7219_SYM_7,
	        MAX7219_SYM_8,
	        MAX7219_SYM_9
	    };

	 // PLAYER 1
	    max7219_SendData(8, digit_to_segment[p1_minutes / 10]);
	    max7219_SendData(7, digit_to_segment[p1_minutes % 10] | 0x80);  // ponto
	    max7219_SendData(6, digit_to_segment[p1_seconds / 10]);
	    max7219_SendData(5, digit_to_segment[p1_seconds % 10]);

	    // PLAYER 2
	    max7219_SendData(4, digit_to_segment[p2_minutes / 10]);
	    max7219_SendData(3, digit_to_segment[p2_minutes % 10] | 0x80);  // ponto
	    max7219_SendData(2, digit_to_segment[p2_seconds / 10]);
	    max7219_SendData(1, digit_to_segment[p2_seconds % 10]);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    if (htim->Instance == TIM2){
    	if (!IS_CONFIG_MODE) {
			// Decrementa o timer do player ativo
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			if(ENABLE_PLAYER_1_TIMER){
				if(PLAYER_1_TIMER >= 1){
					PLAYER_1_TIMER--;
				}
			}
			else if(ENABLE_PLAYER_2_TIMER){
				if(PLAYER_2_TIMER >= 1){
					PLAYER_2_TIMER--;
				}
			}
			update_display();
		}
    }
}

void format_time(int total_seconds) {
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    char buffer[100]; // Espaço suficiente para a string formatada

    // Aqui está o erro: você usou "buffer_size" e "length" sem declará-los corretamente.
    int length = snprintf(buffer, sizeof(buffer), "TIME: %02d:%02d\r\n", minutes, seconds);

    HAL_UART_Transmit(&huart1, (uint8_t *)buffer, length, HAL_MAX_DELAY);
}


void players_time(int time_p1, int time_p2) {
    int p1_minutes = time_p1 / 60;
    int p1_seconds = time_p1 % 60;

    int p2_minutes = time_p2 / 60;
    int p2_seconds = time_p2 % 60;
    char buffer[100]; // Espaço suficiente para a string formatada

    // Aqui está o erro: você usou "buffer_size" e "length" sem declará-los corretamente.
    int length = snprintf(buffer, sizeof(buffer), "PLAYER_1: %02d:%02d PLAYER_2: %02d:%02d\r\n", p1_minutes, p1_seconds,p2_minutes, p2_seconds);
    HAL_UART_Transmit(&huart1, (uint8_t *)buffer, length, HAL_MAX_DELAY);
}


void buzzer() {
      char buffer[100]; // Espaço suficiente para a string formatada
		  int length = snprintf(buffer, sizeof(buffer), "BUZZERRR\r\n");
		  HAL_UART_Transmit(&huart1, (uint8_t *)buffer, length, HAL_MAX_DELAY);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_SET);   // Liga o buzzer
      HAL_Delay(500);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET); // Desliga o buzzer
      HAL_Delay(500);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_SET);   // Liga o buzzer
      HAL_Delay(500);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET); // Desliga o buzzer
      HAL_Delay(500);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_SET);   // Liga o buzzer
      HAL_Delay(1000);
      HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET); // Desliga o buzzer
}

void Flash_Save_LastTimer(uint32_t time)
{
    HAL_FLASH_Unlock();  // Desbloqueia a flash para escrita

    // Apaga a página onde será salvo
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return;
    }

    // Grava o dado (4 bytes)
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_START_ADDR, time);

    HAL_FLASH_Lock(); // Bloqueia a flash novamente
}

uint32_t Flash_Read_LastTimer()
{
    return *(uint32_t*)FLASH_USER_START_ADDR;
}


uint32_t CalculateChecksum(AppData_t *data) {
    uint32_t *ptr = (uint32_t *)data;
    uint32_t sum = 0;

    for (int i = 0; i < (sizeof(AppData_t) - sizeof(uint32_t)) / 4; i++) {
        sum += ptr[i];
    }

    return sum;
}


void SaveAppDataToFlash(AppData_t *data) {
    HAL_FLASH_Unlock();

    // Apaga a página
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError = 0;

    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = FLASH_USER_START_ADDR;
    eraseInit.NbPages = 1;

    if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return;
    }

    // Calcula o checksum
    data->checksum = CalculateChecksum(data);

    // Grava palavra por palavra (32 bits)
    uint32_t *src = (uint32_t *)data;
    for (uint32_t i = 0; i < sizeof(AppData_t) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_START_ADDR + i * 4, src[i]);
    }

    HAL_FLASH_Lock();
}

void Save(){
     AppData_t appData = {
    .player1_timer = PLAYER_1_TIMER,
    .player2_timer = PLAYER_2_TIMER,
    .last_timer = LAST_TIMER,
    .select_minute = SELECT_MINUTE,
    .enable_p1 = ENABLE_PLAYER_1_TIMER,
    .enable_p2 = ENABLE_PLAYER_2_TIMER};
    SaveAppDataToFlash(&appData);
}


bool LoadAppDataFromFlash(AppData_t *data) {
    AppData_t *stored = (AppData_t *)FLASH_USER_START_ADDR;

    // Copia da flash para RAM
    memcpy(data, stored, sizeof(AppData_t));

    // Verifica se está apagado
    if (*(uint32_t *)stored == 0xFFFFFFFF) {
        return false;
    }

    // Valida checksum
    uint32_t expected = CalculateChecksum(data);
    return (expected == data->checksum);
}

void display_timer_config() {
    int minutes = LAST_TIMER / 60;
    int seconds = LAST_TIMER % 60;

    const uint8_t digit_to_segment[] = {
        MAX7219_SYM_0,
        MAX7219_SYM_1,
        MAX7219_SYM_2,
        MAX7219_SYM_3,
        MAX7219_SYM_4,
        MAX7219_SYM_5,
        MAX7219_SYM_6,
        MAX7219_SYM_7,
        MAX7219_SYM_8,
        MAX7219_SYM_9
    };

    uint8_t min_tens = digit_to_segment[minutes / 10];
    uint8_t min_units = digit_to_segment[minutes % 10];
    uint8_t sec_tens = digit_to_segment[seconds / 10];
    uint8_t sec_units = digit_to_segment[seconds % 10];

    // Acende o ponto decimal na parte selecionada
    if (SELECT_MINUTE) {
        min_units |= 0x80;  // bit 7 acende o ponto decimal
    } else {
        sec_units |= 0x80;
    }

    // Mostra no display do Player 1 (4 dígitos do meio)
    max7219_SendData(8, min_tens);
    max7219_SendData(7, min_units);
    max7219_SendData(6, sec_tens);
    max7219_SendData(5, sec_units);

    // Limpa o display do Player 2
    max7219_SendData(4, MAX7219_SYM_BLANK);
    max7219_SendData(3, MAX7219_SYM_BLANK);
    max7219_SendData(2, MAX7219_SYM_BLANK);
    max7219_SendData(1, MAX7219_SYM_BLANK);
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
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  max7219_Init();
  max7219_SetIntensivity(7);
  max7219_TurnOn();
  update_display();


  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
      /* Starting Error */
      Error_Handler();
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
	char buffer[100]; // Enough space for both messages

    players_time(PLAYER_1_TIMER,PLAYER_2_TIMER);

    // JOGADOR 1
    GPIO_PinState current_PT1 = HAL_GPIO_ReadPin(GPIOA, PT_1_Pin);
    if (prev_PT1 == GPIO_PIN_RESET && current_PT1 == GPIO_PIN_SET) {
        HAL_Delay(50); // Wait for bouncing to settle
        if (HAL_GPIO_ReadPin(GPIOA, PT_1_Pin) == GPIO_PIN_SET){
          ENABLE_PLAYER_1_TIMER = false;
          ENABLE_PLAYER_2_TIMER = true;
          Save();
        }
    }
    prev_PT1 = current_PT1;

    // JOGADOR 2
    GPIO_PinState current_PT2 = HAL_GPIO_ReadPin(GPIOA, PT_2_Pin);
    if (prev_PT2 == GPIO_PIN_RESET && current_PT2 == GPIO_PIN_SET)
	  	  {
	  	      HAL_Delay(50); // Wait for bouncing to settle
	  	      if (HAL_GPIO_ReadPin(GPIOA, PT_2_Pin) == GPIO_PIN_SET)
	  	      {
	  	    	  ENABLE_PLAYER_1_TIMER = true;
	  	    	  ENABLE_PLAYER_2_TIMER = false;
	  	    	  Save();
	  	      }
	  	  }

    prev_PT2 = current_PT2;

    // CONFIG
    GPIO_PinState current_CONF = HAL_GPIO_ReadPin(GPIOA, CONF_Pin);
        if (prev_CONF == GPIO_PIN_RESET && current_CONF == GPIO_PIN_SET)
        {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(GPIOA, CONF_Pin) == GPIO_PIN_SET)
            {
                int length = snprintf(buffer, sizeof(buffer), "CONFIG\r\n");
                HAL_UART_Transmit(&huart1, (uint8_t *)buffer, length, HAL_MAX_DELAY);

                ENABLE_PLAYER_1_TIMER = false;
                ENABLE_PLAYER_2_TIMER = false;

                int pressed = 0;
                IS_CONFIG_MODE = true;
                while (pressed == 0)
                {
                    format_time(LAST_TIMER);
                    display_timer_config();

                    // SEL - Alternar entre minutos e segundos
                    GPIO_PinState current_SEL = HAL_GPIO_ReadPin(GPIOA, SEL_Pin);
                    if (prev_SEL == GPIO_PIN_RESET && current_SEL == GPIO_PIN_SET)
                    {
                        HAL_Delay(50);
                        if (HAL_GPIO_ReadPin(GPIOA, SEL_Pin) == GPIO_PIN_SET)
                        {
                            SELECT_MINUTE = !SELECT_MINUTE;
                            Save();
                        }
                    }
                    prev_SEL = current_SEL;

                    // INC - Incrementar tempo
                    GPIO_PinState current_INC = HAL_GPIO_ReadPin(GPIOA, INC_Pin);
                    if (prev_INC == GPIO_PIN_RESET && current_INC == GPIO_PIN_SET)
                    {
                        HAL_Delay(50);
                        if (HAL_GPIO_ReadPin(GPIOA, INC_Pin) == GPIO_PIN_SET)
                        {
                            if (SELECT_MINUTE)
                            {
                                if (LAST_TIMER < 59 * 60)
                                    LAST_TIMER += 60;
                            }
                            else
                            {
                                if (LAST_TIMER < (59 * 60) + 59)
                                    LAST_TIMER += 1;
                            }
                        }
                        Save();
                    }
                    prev_INC = current_INC;

                    // DEC - Decrementar tempo
                    GPIO_PinState current_DEC = HAL_GPIO_ReadPin(GPIOA, DEC_Pin);
                    if (prev_DEC == GPIO_PIN_RESET && current_DEC == GPIO_PIN_SET)
                    {
                        HAL_Delay(50);
                        if (HAL_GPIO_ReadPin(GPIOA, DEC_Pin) == GPIO_PIN_SET)
                        {
                            if (SELECT_MINUTE)
                            {
                                if (LAST_TIMER >= 60)
                                    LAST_TIMER -= 60;
                                else
                                    LAST_TIMER = 10;
                            }
                            else
                            {
                                if (LAST_TIMER > 10)
                                    LAST_TIMER -= 1;
                                else
                                    LAST_TIMER = 10;
                            }
                            Save();
                        }
                    }
                    prev_DEC = current_DEC;

                    // CONF - Confirmar configuração e sair do menu
                    GPIO_PinState current_CONF_SALVAR = HAL_GPIO_ReadPin(GPIOA, CONF_Pin);
                    if (prev_CONF_SALVAR == GPIO_PIN_RESET && current_CONF_SALVAR == GPIO_PIN_SET)
                    {
                        HAL_Delay(50);
                        if (HAL_GPIO_ReadPin(GPIOA, CONF_Pin) == GPIO_PIN_SET)
                        {
                            PLAYER_1_TIMER = LAST_TIMER;
                            PLAYER_2_TIMER = LAST_TIMER;
                            Save();
                            pressed = 1;
                        }
                    }
                    prev_CONF_SALVAR = current_CONF_SALVAR;



                        // JOGADOR 1
                        GPIO_PinState current_PT1 = HAL_GPIO_ReadPin(GPIOA, PT_1_Pin);
                        if (prev_PT1 == GPIO_PIN_RESET && current_PT1 == GPIO_PIN_SET) {
                            HAL_Delay(50); // Wait for bouncing to settle
                            if (HAL_GPIO_ReadPin(GPIOA, PT_1_Pin) == GPIO_PIN_SET){
                              ENABLE_PLAYER_1_TIMER = false;
                              ENABLE_PLAYER_2_TIMER = true;
                              Save();
                              pressed = 1;
                            }
                        }
                        prev_PT1 = current_PT1;

                        // JOGADOR 2
                        GPIO_PinState current_PT2 = HAL_GPIO_ReadPin(GPIOA, PT_2_Pin);
                        if (prev_PT2 == GPIO_PIN_RESET && current_PT2 == GPIO_PIN_SET)
                            {
                                HAL_Delay(50); // Wait for bouncing to settle
                                if (HAL_GPIO_ReadPin(GPIOA, PT_2_Pin) == GPIO_PIN_SET)
                                {
                                  ENABLE_PLAYER_1_TIMER = true;
                                  ENABLE_PLAYER_2_TIMER = false;
                                  Save();
                                  pressed = 1;
                                }
                            }

                        prev_PT2 = current_PT2;
                }
                IS_CONFIG_MODE = false;
            }
        }
    prev_CONF = current_CONF;


    // TEMPO ESGOTADO
	  if (PLAYER_1_TIMER == 0 || PLAYER_2_TIMER == 0)
	  {


      players_time(PLAYER_1_TIMER,PLAYER_2_TIMER);
      buzzer();

      int pressed = 0;
        while (pressed == 0)
        {
          current_CONF = HAL_GPIO_ReadPin(GPIOA, CONF_Pin);
          if (prev_CONF == GPIO_PIN_RESET && current_CONF == GPIO_PIN_SET)
            {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(GPIOA, CONF_Pin) == GPIO_PIN_SET)
            {
            ENABLE_PLAYER_1_TIMER = false;
            ENABLE_PLAYER_2_TIMER = false;
            PLAYER_1_TIMER = LAST_TIMER;
            PLAYER_2_TIMER = LAST_TIMER;
            Save();
            pressed = 1;
            }}
          }
          prev_CONF = current_CONF;
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PT_1_Pin PT_2_Pin CONF_Pin SEL_Pin
                           INC_Pin DEC_Pin */
  GPIO_InitStruct.Pin = PT_1_Pin|PT_2_Pin|CONF_Pin|SEL_Pin
                          |INC_Pin|DEC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZ_Pin */
  GPIO_InitStruct.Pin = BUZ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

#ifdef  USE_FULL_ASSERT
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
