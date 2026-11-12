/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fdcan.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_MS        20    /* Hardware debounce window (ms) */
#define DUP_INTERVAL_MS    2     /* Interval between duplicates in a burst (ms) */
#define STATUS_PERIOD_MS   500   /* Background status period in normal mode (ms) */
#define STATUS_PERIOD_ERR  40    /* Background status period in error mode (ms) */
#define STATUS_GAP_MS      2     /* Gap between X and Z (ms) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t blink_timer    = 0;
volatile uint32_t rx_blink_timer = 0;

volatile uint32_t error_blink_timer = 0;
volatile uint8_t  is_can_bus_error  = 0;

/* Background status */
volatile uint32_t status_timer     = 0;
volatile uint8_t  status_x_sent    = 0;
volatile uint32_t status_gap_timer = 0;

/* Endstop X */
volatile uint32_t x_debounce_timer = 0;
volatile uint32_t x_dup_timer      = 0;
volatile uint8_t  x_dup_counter    = 0;
volatile uint32_t x_dup_id         = 0;

/* Endstop Z */
volatile uint32_t z_debounce_timer = 0;
volatile uint32_t z_dup_timer      = 0;
volatile uint8_t  z_dup_counter    = 0;
volatile uint32_t z_dup_id         = 0;

/* Tx FIFO address in Message RAM — initialized in main() */
volatile uint32_t *can_tx_base = 0;


volatile uint32_t blow1_end_time    = 0;
volatile uint8_t  blow1_active      = 0;

volatile uint32_t blow2_end_time    = 0;
volatile uint8_t  blow2_active      = 0;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void CAN_Send_ZeroData_Fast(uint32_t can_id);
void Process_Background_Status(uint32_t now);
void Process_Combat_Endstops(uint32_t now);
void Process_CAN_Diagnostics(uint32_t now);
void Process_Auto_Blow(uint32_t now);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ============================================================
   SEND 0-BYTE FRAME (Queue mode)
   ============================================================ */
void CAN_Send_ZeroData_Fast(uint32_t can_id)
{
    uint32_t tx_fqs = FDCAN1->TXFQS;
    if (tx_fqs & FDCAN_TXFQS_TFQF) return;

    uint32_t put_idx = (tx_fqs & FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;

    volatile uint32_t *slot = can_tx_base + (put_idx * 4U);
    slot[0] = (can_id << 18);
    slot[1] = 0U;

    FDCAN1->TXBAR = (1U << put_idx);
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
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();

  /* USER CODE BEGIN 2 */

  /* Status LEDs PD0 (STATUS) and PD1 (ACT) */
  RCC->IOPENR |= RCC_IOPENR_GPIODEN;
  GPIOD->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);
  GPIOD->MODER |=  (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0);
  GPIOD->BRR    =  (GPIO_BRR_BR0 | GPIO_BRR_BR1);

  /* Hardware filter: accept only ID 0x100 into FIFO0 */
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0;
  sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = 0x100;
  sFilterConfig.FilterID2    = 0x100;
  HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);

  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                               FDCAN_REJECT, FDCAN_REJECT);

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) Error_Handler();

  can_tx_base = (volatile uint32_t *)hfdcan1.msgRam.TxFIFOQSA;

  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  FDCAN1->IE  |= (FDCAN_IE_TCE | FDCAN_IE_TFEE);
  FDCAN1->ILE |= FDCAN_ILE_EINT0;

  NVIC_SetPriority(TIM16_FDCAN_IT0_IRQn, 1);
  NVIC_EnableIRQ(TIM16_FDCAN_IT0_IRQn);
  /* TIM17 not used */

  /* Initial endstop state sampling */
  x_dup_id = (GPIOA->IDR & GPIO_IDR_ID2) ? 0x10 : 0x11;
  z_dup_id = (GPIOA->IDR & GPIO_IDR_ID3) ? 0x20 : 0x21;

  /* Timers init */
  status_timer     = uwTick;
  status_x_sent    = 0;
  status_gap_timer = uwTick;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = uwTick;

    Process_Background_Status(now);
    Process_Combat_Endstops(now);
    Process_CAN_Diagnostics(now);
		Process_Auto_Blow(now);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief 1. Periodic background status of X and Z.
  *        In error mode the period is shortened to help TEC drop faster.
  */
void Process_Background_Status(uint32_t now)
{
    if (x_dup_counter != 0 || z_dup_counter != 0) return;

    /* Fast burst in error mode to drop TEC faster */
    uint32_t period = is_can_bus_error ? STATUS_PERIOD_ERR : STATUS_PERIOD_MS;

    if (!status_x_sent && (now - status_timer >= period))
    {
        x_dup_id = (GPIOA->IDR & GPIO_IDR_ID2) ? 0x10 : 0x11;
        CAN_Send_ZeroData_Fast(x_dup_id);

        status_gap_timer = now;
        status_x_sent    = 1;
    }
    else if (status_x_sent && (now - status_gap_timer >= STATUS_GAP_MS))
    {
        z_dup_id = (GPIOA->IDR & GPIO_IDR_ID3) ? 0x20 : 0x21;
        CAN_Send_ZeroData_Fast(z_dup_id);

        status_x_sent = 0;
        status_timer  = now;

        if (!is_can_bus_error)
        {
            GPIOD->BSRR = GPIO_BSRR_BS1;    /* LED ACT */
            blink_timer = now;
        }
    }
}

/**
  * @brief 2. Duplicate bursts and debounce handling for endstops
  */
void Process_Combat_Endstops(uint32_t now)
{
    /* ================== ENDSTOP X ================== */
    if (x_dup_counter > 0 && (now - x_dup_timer >= DUP_INTERVAL_MS))
    {
        x_dup_timer = now;
        CAN_Send_ZeroData_Fast(x_dup_id);
        x_dup_counter--;
    }

    if (!(EXTI->IMR1 & EXTI_IMR1_IM2) && x_dup_counter == 0 &&
        (now - x_debounce_timer >= DEBOUNCE_MS))
    {
        uint32_t real_x_id = (GPIOA->IDR & GPIO_IDR_ID2) ? 0x10 : 0x11;
        EXTI->FPR1 = EXTI_FPR1_FPIF2;
        EXTI->RPR1 = EXTI_RPR1_RPIF2;
        EXTI->IMR1 |= EXTI_IMR1_IM2;

        if (real_x_id != x_dup_id)
        {
            x_dup_id = real_x_id;
            CAN_Send_ZeroData_Fast(x_dup_id);
            x_dup_timer   = now;
            x_dup_counter = 2;   /* send 2 more duplicates */
        }
    }

    /* ================== ENDSTOP Z ================== */
    if (z_dup_counter > 0 && (now - z_dup_timer >= DUP_INTERVAL_MS))
    {
        z_dup_timer = now;
        CAN_Send_ZeroData_Fast(z_dup_id);
        z_dup_counter--;
    }

    if (!(EXTI->IMR1 & EXTI_IMR1_IM3) && z_dup_counter == 0 &&
        (now - z_debounce_timer >= DEBOUNCE_MS))
    {
        uint32_t real_z_id = (GPIOA->IDR & GPIO_IDR_ID3) ? 0x20 : 0x21;
        EXTI->FPR1 = EXTI_FPR1_FPIF3;
        EXTI->RPR1 = EXTI_RPR1_RPIF3;
        EXTI->IMR1 |= EXTI_IMR1_IM3;

        if (real_z_id != z_dup_id)
        {
            z_dup_id = real_z_id;
            CAN_Send_ZeroData_Fast(z_dup_id);
            z_dup_timer   = now;
            z_dup_counter = 2;
        }
    }
}

/**
  * @brief 3. CAN diagnostics, error latch, valve shutdown, LED timing.
  */
void Process_CAN_Diagnostics(uint32_t now)
{   static uint8_t error_led_state = 0;
    uint32_t tec = FDCAN1->ECR & 0xFF;
    uint32_t bo  = FDCAN1->PSR & FDCAN_PSR_BO;

    /* Hysteresis: enter at TEC > 64 or Bus-Off, exit at TEC < 8 */
    if (is_can_bus_error)
    {
        if (tec < 8) is_can_bus_error = 0;
    }
    else
    {
        if (bo || (tec > 64)) is_can_bus_error = 1;
    }

		 if (is_can_bus_error)
		{
				GPIOB->BRR = (0x1FU << 11);   /* valves OFF */
			
			  /* Reset auto-blow software flags to fully clear */
        /* timer logic in case of connection loss */
        blow1_active = 0;
        blow2_active = 0;

				if (now - error_blink_timer >= 150)
				{
						error_blink_timer = now;
						error_led_state ^= 1;

						error_led_state ? (GPIOD->BSRR = (GPIO_BSRR_BS0 | GPIO_BSRR_BS1))
                            : (GPIOD->BRR  = (GPIO_BRR_BR0  | GPIO_BRR_BR1));
				}
		}
		else
		{
				if (now - rx_blink_timer >= 70) GPIOD->BRR = GPIO_BRR_BR0;
				if (now - blink_timer    >= 8)  GPIOD->BRR = GPIO_BRR_BR1;
		}
}

/**
  * @brief 4. Non-blocking control of automatic head blow-off timing
  */
void Process_Auto_Blow(uint32_t now)
{
    /* Blow-off 1 control */
    if (blow1_active && (now - blow1_end_time < 0x80000000U))
    {
        /* Timeout reached: Instantly turn OFF Blow-off 1 (PB12) */
        GPIOB->BSRR = (GPIO_BSRR_BS12 << 16U);
        blow1_active = 0;
    }

    /* Blow-off 2 control */
    if (blow2_active && (now - blow2_end_time < 0x80000000U))
    {
        /* Timeout reached: Instantly turn OFF Blow-off 2 (PB13) */
        GPIOB->BSRR = (GPIO_BSRR_BS13 << 16U);
        blow2_active = 0;
    }
}


/* USER CODE END 4 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN            = 8;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV5;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
