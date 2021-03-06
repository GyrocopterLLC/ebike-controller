/******************************************************************************
 * Filename: pwm.c
 * Description: Initializes timer hardware on the STM32F4 for pulse width
 *              modulation (PWM) output. This PWM is configured specifically
 *              for motor control: 6 outputs in 3 complementary (high-side and
 *              low-side) channels.
 ******************************************************************************

 Copyright (c) 2019 David Miller

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include "pwm.h"
#include "gpio.h"
#include "project_parameters.h"
//#include "ui.h"
#include "main.h"
#include "eeprom_emulation.h"

float pwm_timer_arr_f = PWM_PERIOD_F;

uint16_t PWM_DT_ns_to_reg(uint32_t dtns);
uint32_t PWM_DT_reg_to_ns(uint16_t dtreg);

uint16_t PWM_DT_ns_to_reg(uint32_t dtns) {
    uint16_t temp_bdtr = 0x00;
    if (dtns < DT_RANGE1_MAX) {
        temp_bdtr = (uint32_t) (((float) dtns) * 0.168f);
        temp_bdtr &= 0x7F;
    } else if (dtns < DT_RANGE2_MAX) {
        temp_bdtr = (uint32_t) (((float) dtns) * 0.084f);
        temp_bdtr = (temp_bdtr > 64) ? (temp_bdtr - 64) : 0;
        if (temp_bdtr > 63)
            temp_bdtr = 63;
        temp_bdtr |= 0x80;
    } else if (dtns < DT_RANGE3_MAX) {
        temp_bdtr = (uint32_t) (((float) dtns) * 0.021f);
        temp_bdtr = (temp_bdtr > 32) ? (temp_bdtr - 32) : 0;
        if (temp_bdtr > 31)
            temp_bdtr = 31;
        temp_bdtr |= 0xC0;
    } else if (dtns < DT_RANGE4_MAX) {
        temp_bdtr = (uint32_t) (((float) dtns) * 0.0105f);
        temp_bdtr = (temp_bdtr > 32) ? (temp_bdtr - 32) : 0;
        if (temp_bdtr > 31)
            temp_bdtr = 31;
        temp_bdtr |= 0xE0;
    }

    return temp_bdtr;
}

uint32_t PWM_DT_reg_to_ns(uint16_t dtreg) {
    float temp;
    if ((dtreg & 0x80) == 0) {
        temp = ((float) (dtreg)) / 0.168f;
    } else if ((dtreg & 0xC0) == 0x80) {
        temp = ((float) ((0x3F & dtreg) + 64)) / 0.084f;
    } else if ((dtreg & 0xE0) == 0xC0) {
        temp = ((float) ((0x1F & dtreg) + 32)) / 0.021f;
    } else {
        temp = ((float) ((0x1F & dtreg) + 32)) / 0.0105f;
    }

    return ((uint32_t) (temp));
}

void PWM_Init(int32_t freq) {
    GPIO_Clk(PWM_HI_PORT);
    GPIO_Clk(PWM_LO_PORT);

    // Force GPIO outputs low. If the Timer releases the output pins, this ensures that the
    // FETs will not be turned on inadvertently.
    PWM_HI_PORT->ODR &= ~((1 << PWM_AHI_PIN) | (1 << PWM_BHI_PIN)
            | (1 << PWM_CHI_PIN));
    PWM_LO_PORT->ODR &= ~((1 << PWM_ALO_PIN) | (1 << PWM_BLO_PIN)
            | (1 << PWM_CLO_PIN));

    GPIO_AF(PWM_HI_PORT, PWM_AHI_PIN, PWM_AF);
    GPIO_AF(PWM_HI_PORT, PWM_BHI_PIN, PWM_AF);
    GPIO_AF(PWM_HI_PORT, PWM_CHI_PIN, PWM_AF);
    GPIO_AF(PWM_LO_PORT, PWM_ALO_PIN, PWM_AF);
    GPIO_AF(PWM_LO_PORT, PWM_BLO_PIN, PWM_AF);
    GPIO_AF(PWM_LO_PORT, PWM_CLO_PIN, PWM_AF);

    PWM_TIM_CLK_ENABLE();

    PWM_SetFreq(freq);
    PWM_TIMER->PSC = 0; // No prescaler
    PWM_TIMER->RCR = 1; // Update occurs every full cycle of the PWM timer
    PWM_TIMER->CR1 = TIM_CR1_CMS_0; // Center aligned 1 mode
    PWM_TIMER->CR2 = (TIM_CR2_MMS_2 | TIM_CR2_MMS_1 | TIM_CR2_MMS_0); // OC4REF is trigger out
    PWM_TIMER->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1
            | TIM_CCMR1_OC2M_2;
    PWM_TIMER->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
    PWM_TIMER->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1
            | TIM_CCMR2_OC4M_2;
    PWM_TIMER->CCMR2 |= TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE;
    PWM_TIMER->CCER = TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E
            | TIM_CCER_CC2NE |
            TIM_CCER_CC3E | TIM_CCER_CC3NE;
    PWM_TIMER->BDTR = PWM_DEFAULT_DT_REG | TIM_BDTR_OSSI; // Dead time selection, and drive outputs low when
    // Motor Enable (MOE) bit is zero

    PWM_TIMER->CCR1 = PWM_PERIOD / 2 + 1;
    PWM_TIMER->CCR2 = PWM_PERIOD / 2 + 1;
    PWM_TIMER->CCR3 = PWM_PERIOD / 2 + 1;
    PWM_TIMER->CCR4 = PWM_PERIOD - 1; // Triggers during downcounting, just after reload

    NVIC_SetPriority(PWM_IRQn, PRIO_PWM); // Highest priority
    NVIC_EnableIRQ(PWM_IRQn);

    PWM_TIMER->SR = ~(TIM_SR_UIF); // Clear update interrupt (if it was triggered)
    PWM_TIMER->DIER = TIM_DIER_UIE; // Enable update interrupt

    PWM_TIMER->CR1 |= TIM_CR1_CEN; // Start the timer
    PWM_TIMER->RCR = 1; // This picks the underflow as the update event
    PWM_TIMER->EGR |= TIM_EGR_UG; // Generate an update event to latch in all the settings

    /* NOTE: MOE bit is still zero, so outputs are at their inactive level. */
}

uint8_t PWM_SetDeadTime(int32_t newDT) {
    uint32_t temp_bdtr = PWM_TIMER->BDTR & (0xFF00);
    temp_bdtr |= PWM_DT_ns_to_reg(newDT);
    PWM_TIMER->BDTR = temp_bdtr;
    return DATA_PACKET_SUCCESS;
}
int32_t PWM_GetDeadTime(void) {
    uint32_t temp_bdtr = PWM_TIMER->BDTR & (0x00FF);
    return PWM_DT_reg_to_ns(temp_bdtr);
}

uint8_t PWM_SetFreq(int32_t newFreq) {
    int32_t temp = PWM_TIMER_FREQ;
    int32_t tempcr1;
    int32_t tempbdtr;
    if((newFreq >= PWM_MIN_FREQ) && (newFreq <= PWM_MAX_FREQ)) {
        temp = temp / newFreq;
        temp = temp / 2;
        temp = temp - 1;
        tempbdtr = PWM_TIMER->BDTR;
        PWM_MotorOFF();
        tempcr1 = PWM_TIMER->CR1; // Save current CR1 value
        PWM_TIMER->CR1 &= ~TIM_CR1_CEN; // Stop the timer if it's running
        PWM_TIMER->ARR = temp;
        PWM_TIMER->EGR |= TIM_EGR_UG; // Generate an update event to latch in all the settings
        PWM_TIMER->CR1 = tempcr1; // Restart the timer if it was running
        PWM_TIMER->BDTR = tempbdtr; // Turn on outputs if they were on

        return DATA_PACKET_SUCCESS;
    }

    return DATA_PACKET_FAIL;
}

int32_t PWM_GetFreq(void) {
    int32_t temp = PWM_TIMER_FREQ;
    temp = temp / (PWM_PERIOD + 1);
    return temp;
}

/* Changed so that tA->CCR3, tC->CCR1. tB unchanged (tB->CCR2)
 * The implemented pinout on the STM32F4x5 is TIM1_CH1 -> PA8,
 * TIM1_CH2 -> PA9, and TIM1_CH3 -> PA10. However, the board
 * design has (PWMA+) -> PA10, (PWMB+) -> PA9, and (PWMC+) -> PA8.
 * Since the 1st and 3rd channels are swapped in hardware, here
 * I'm swapping them in software to match.
 *
 * The negative versions of these signals are likewise swapped.
 * E.G., TIM1_CH1N -> PB13, TIM1_CH2N -> PB14, TIM1_CH3N -> PB15
 * but... (PWMA-) -> PB15, (PWMB-) -> PB14, (PWMC-) -> PB13
 */
void PWM_SetDuty(uint16_t tA, uint16_t tB, uint16_t tC) {
    // scale from 65536 to the maximum counter value, PWM_PERIOD
    uint32_t temp;
    temp = tA * PWM_TIMER->ARR / 65536;
    //PWM_TIMER->CCR1 = temp;
    PWM_TIMER->CCR3 = temp;
    temp = tB * PWM_TIMER->ARR / 65536;
    PWM_TIMER->CCR2 = temp;
    temp = tC * PWM_TIMER->ARR / 65536;
    //PWM_TIMER->CCR3 = temp;
    PWM_TIMER->CCR1 = temp;
}

void PWM_SetDutyF(float tA, float tB, float tC) {
    PWM_TIMER->CCR1 = (uint16_t) (tC * pwm_timer_arr_f);
    PWM_TIMER->CCR2 = (uint16_t) (tB * pwm_timer_arr_f);
    PWM_TIMER->CCR3 = (uint16_t) (tA * pwm_timer_arr_f);
}

