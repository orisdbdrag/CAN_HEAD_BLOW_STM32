/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{
  /* 1. Enable clocks for all required GPIO ports */
  RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIODEN;

  /* 2. Configure LED outputs (PD0, PD1) */
  GPIOD->MODER   &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);
  GPIOD->MODER   |= (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0);      /* Output mode */
  GPIOD->OSPEEDR |= (GPIO_OSPEEDR_OSPEED0 | GPIO_OSPEEDR_OSPEED1);  /* Very High Speed */

  /* 3. Configure valve control outputs (PB11, PB12, PB13, PB14, PB15) */
  uint32_t pin_mask_b = GPIO_MODER_MODE11 | GPIO_MODER_MODE12 | GPIO_MODER_MODE13 |
                        GPIO_MODER_MODE14 | GPIO_MODER_MODE15;
  GPIOB->MODER   &= ~pin_mask_b;
  GPIOB->MODER   |= (GPIO_MODER_MODE11_0 | GPIO_MODER_MODE12_0 | GPIO_MODER_MODE13_0 |
                     GPIO_MODER_MODE14_0 | GPIO_MODER_MODE15_0);
  GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12 |
                     GPIO_OSPEEDR_OSPEED13 | GPIO_OSPEEDR_OSPEED14 |
                     GPIO_OSPEEDR_OSPEED15);

  /* All outputs default to 0 */
  GPIOD->BRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1);
  GPIOB->BRR = (GPIO_BSRR_BR11 | GPIO_BSRR_BR12 | GPIO_BSRR_BR13 |
                GPIO_BSRR_BR14 | GPIO_BSRR_BR15);

  /* 4. Configure endstops X (PA2) and Z (PA3) as inputs */
  GPIOA->MODER &= ~(GPIO_MODER_MODE2 | GPIO_MODER_MODE3);            /* Input mode */
  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD3);
  GPIOA->PUPDR |= (GPIO_PUPDR_PUPD2_0 | GPIO_PUPDR_PUPD3_0);         /* Pull-up */

  /* 5. Configure EXTI for PA2 and PA3 */
  /* On STM32G0, EXTI lines are hardwired to pin numbers. PA2 -> EXTI2, PA3 -> EXTI3. */
  /* Port routing on G0 is done via EXTICR. Default value is 0 = Port A. Set explicitly. */
  EXTI->EXTICR[0] &= ~(EXTI_EXTICR1_EXTI2 | EXTI_EXTICR1_EXTI3);     /* 0000 = Port A */

  /* Trigger on both rising and falling edges */
  EXTI->RTSR1 |= (EXTI_RTSR1_RT2 | EXTI_RTSR1_RT3);                  /* Rising */
  EXTI->FTSR1 |= (EXTI_FTSR1_FT2 | EXTI_FTSR1_FT3);                  /* Falling */

  /* Enable EXTI lines in the EXTI controller */
  EXTI->IMR1 |= (EXTI_IMR1_IM2 | EXTI_IMR1_IM3);

  /* 6. Enable NVIC interrupts. EXTI2 and EXTI3 share the same vector EXTI2_3_IRQn */
  NVIC_SetPriority(EXTI2_3_IRQn, 0);   /* Highest priority (0) for endstops! */
  NVIC_EnableIRQ(EXTI2_3_IRQn);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
