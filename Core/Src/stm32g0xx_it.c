/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32g0xx_it.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fdcan.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static const uint32_t valve_pins[] = {
    GPIO_BSRR_BS12,
    GPIO_BSRR_BS13,
    GPIO_BSRR_BS14,
    GPIO_BSRR_BS15,
    GPIO_BSRR_BS11
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern FDCAN_HandleTypeDef hfdcan1;
/* USER CODE BEGIN EV */
extern volatile uint32_t x_dup_timer;
extern volatile uint8_t  x_dup_counter;
extern volatile uint32_t x_dup_id;
extern volatile uint32_t z_dup_timer;
extern volatile uint8_t  z_dup_counter;
extern volatile uint32_t z_dup_id;

extern volatile uint32_t x_debounce_timer;
extern volatile uint32_t z_debounce_timer;
extern volatile uint32_t blink_timer;
extern volatile uint32_t rx_blink_timer;
extern volatile uint8_t  is_can_bus_error;

extern volatile uint32_t blow1_end_time;
extern volatile uint8_t  blow1_active;
extern volatile uint32_t blow2_end_time;
extern volatile uint8_t  blow2_active;

void CAN_Send_ZeroData_Fast(uint32_t can_id);
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/

/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  while (1) { }
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G0xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/* ============================================================
   EXTI for endstops X (PA2) / Z (PA3).
   First packet of the burst is fired HERE — instantly, from the ISR.
   Remaining duplicates are handled in the main loop.
   ============================================================ */
void EXTI2_3_IRQHandler(void)
{
    uint32_t fpr = EXTI->FPR1;
    uint32_t rpr = EXTI->RPR1;

    /* Clear pending flags only for lines 2 and 3 */
    EXTI->FPR1 = (EXTI_FPR1_FPIF2 | EXTI_FPR1_FPIF3);
    EXTI->RPR1 = (EXTI_RPR1_RPIF2 | EXTI_RPR1_RPIF3);

    uint32_t now = uwTick;   /* direct read, faster than HAL_GetTick() */

    /* ===================== ENDSTOP X ===================== */
    if ((fpr & EXTI_FPR1_FPIF2) || (rpr & EXTI_RPR1_RPIF2))
    {
        if (!(EXTI->IMR1 & EXTI_IMR1_IM2)) return;   /* Already muted by debounce */

        EXTI->IMR1 &= ~EXTI_IMR1_IM2;   /* Hardware lock the line for DEBOUNCE_MS */

        x_dup_id = (GPIOA->IDR & GPIO_IDR_ID2) ? 0x10 : 0x11;

        /* INSTANT 1st shot of the burst */
        CAN_Send_ZeroData_Fast(x_dup_id);

        x_dup_timer      = now;
        x_dup_counter    = 2;   /* queue 2 more duplicates in main loop */
        x_debounce_timer = now;

        GPIOD->BSRR = GPIO_BSRR_BS1;    /* LED ACT */
        blink_timer = now;
    }

    /* ===================== ENDSTOP Z ===================== */
    if ((fpr & EXTI_FPR1_FPIF3) || (rpr & EXTI_RPR1_RPIF3))
    {
        if (!(EXTI->IMR1 & EXTI_IMR1_IM3)) return;

        EXTI->IMR1 &= ~EXTI_IMR1_IM3;

        z_dup_id = (GPIOA->IDR & GPIO_IDR_ID3) ? 0x20 : 0x21;

        /* INSTANT 1st shot of the burst */
        CAN_Send_ZeroData_Fast(z_dup_id);

        z_dup_timer      = now;
        z_dup_counter    = 2;
        z_debounce_timer = now;

        GPIOD->BSRR = GPIO_BSRR_BS1;
        blink_timer = now;
    }
}

/**
  * @brief This function handles TIM16, FDCAN1_IT0 and FDCAN2_IT0 Interrupt.
  *        Reception of valve commands, ID 0x100, DLC = 2.
  */
void TIM16_FDCAN_IT0_IRQHandler(void)
{
    if (FDCAN1->IR & FDCAN_IR_RF0L) { FDCAN1->IR = FDCAN_IR_RF0L; }

    if (FDCAN1->IR & FDCAN_IR_RF0N)
    {
        FDCAN1->IR = FDCAN_IR_RF0N;

        FDCAN_RxHeaderTypeDef RxHeader;
        uint8_t RxData[8];

        /* === SINGLE UNIFIED RX FIFO PROCESSING LOOP === */
        while ((FDCAN1->RXF0S & FDCAN_RXF0S_F0FL) != 0U)
        {
            if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
                break;

            uint8_t valve_num   = RxData[0];
            uint8_t valve_state = RxData[1]; /* S_Value time in ms is passed here for N10/N11 */

            /* Standard direct manual valve control (N0 - N4) */
            if (valve_num < 5)
            {
                uint32_t mask = valve_pins[valve_num];
                GPIOB->BSRR = (valve_state == 1U) ? mask : (mask << 16U);
            }
            /* --- AUTOMATIC BLOW-OFF CHANNEL 1 (N10) --- */
            else if (valve_num == 10)
            {
                /* 1. Instantly turn OFF Vacuum 1 (PB14) */
                GPIOB->BSRR = (GPIO_BSRR_BS14 << 16U);
                
                /* 2. Instantly turn ON Blow-off 1 (PB12) */
                GPIOB->BSRR = GPIO_BSRR_BS12;
                
                /* 3. Set timeout for automatic shutdown */
                blow1_end_time = uwTick + valve_state;
                blow1_active   = 1;
            }
            /* --- AUTOMATIC BLOW-OFF CHANNEL 2 (N11) --- */
            else if (valve_num == 11)
            {
                /* 1. Instantly turn OFF Vacuum 2 (PB15) */
                GPIOB->BSRR = (GPIO_BSRR_BS15 << 16U);
                
                /* 2. Instantly turn ON Blow-off 2 (PB13) */
                GPIOB->BSRR = GPIO_BSRR_BS13;
                
                /* 3. Set timeout for automatic shutdown */
                blow2_end_time = uwTick + valve_state;
                blow2_active   = 1;
            }
        }				
        /* === END RX FIFO PROCESSING LOOP === */
			
        /* LED STATUS (RX) — on, start 70 ms countdown (only when NOT in error mode) */
        if (!is_can_bus_error)
        {
            GPIOD->BSRR    = GPIO_BSRR_BS0;
            rx_blink_timer = uwTick;
        }
    }
}

/* USER CODE END 1 */
