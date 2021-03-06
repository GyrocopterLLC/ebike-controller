/******************************************************************************
 * Filename: hallSensor.h
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

// Used resources:
// TIM3, TIM4
// DMA2 Stream 1
#ifndef HALLSENSOR_H_
#define HALLSENSOR_H_

#include "stm32f4xx.h"
#include "pinconfig.h"
#include "periphconfig.h"

#define USE_FLOATING_POINT
//#define TESTING_2X
#define TESTING_PLL

#define HALL_TIMER_INPUT_CLOCK			84000000 // APB1 clock * 2
#define HALL_TIMER_INPUT_CLOCK_MHZ		84 // APB1 clock * 2 / 1000000

#define HALL_SAMPLE_PERIOD				1176	// 7us on the APB2 clock (NOT a multiple of 50us!)
#define HALL_NUM_SAMPLES				32 // Number of samples taken of the GPIO

#define HALL_PSC_MIN					15  // 84MHz clock / 16  = 5.25MHz -> 12.5millisec total period
#define HALL_PSC_MAX					127 // 84MHz clock / 128 = 656.25kHz -> .0998sec total period
#define HALL_PSC_CHG_AMT				16
#define HALL_MIN_CAPTURE				16384 // First 1/4 of the period
#define HALL_MAX_OVERFLOWS				3

#define HALL_PSC_CHANGED_UP				1
#define HALL_PSC_CHANGED_DOWN			2
#define HALL_PSC_CHANGED				(HALL_PSC_CHANGED_UP | HALL_PSC_CHANGED_DOWN)
#define HALL_STOPPED					4

#define	HALL_ROT_UNKNOWN				0
#define HALL_ROT_FORWARD				1
#define HALL_ROT_REVERSE				2

// Error checking
#define HALL_MAX_SPEED_CHANGE           (3.0f)
#define HALL_MIN_STEADY_ROTATION_COUNT  (6) // One full electrical rotation

typedef struct _hallsensor{
#if defined(USE_FLOATING_POINT)
    float Speed;
    float PreviousSpeed;
    uint32_t CallingFrequency;
    float AngleIncrement;
    float Angle;
    uint32_t CaptureValue;
    uint32_t CaptureForState[6];
    uint16_t Prescaler;
    uint16_t PrescalerForState[6];
    uint8_t Status;
    uint8_t OverflowCount;
    uint8_t SteadyRotationCount;
    uint8_t RotationDirection;
    uint8_t PreviousRotationDirection;
    uint8_t CurrentState;
    uint8_t Valid;
#else
    uint32_t Speed; // Expressed in Hz * 2^16 (aka Q16 number)
    uint32_t CallingFrequency;// How rapidly the speed calculation will be updated
    uint32_t CaptureValue;
    uint16_t AngleIncrement;
    uint16_t Angle;
    uint16_t Prescaler;
    uint8_t Status;
    uint8_t OverflowCount;
    uint8_t RotationDirection;
    uint8_t CurrentState;
#endif

} HallSensor_HandleTypeDef;

#ifdef TESTING_PLL
typedef struct _hallsensorpll{
    float Alpha; // Gain for phase difference
    float Beta; // Gain for frequency (fixed at alpha^2/2)
    float dt; // Timestep
    float Frequency; // Output frequency
    float Phase; // Output angle
    uint8_t Valid; // Is phase locked?
    uint16_t ValidCounter; // Increments to saturation while locked

} HallSensorPLL_HandleTypeDef;

#define PLL_LOCKED_PHASE_ERROR      (0.2f)
#define PLL_LOCKED_COUNTS           (1000)

#define PLL_UNLOCKED                (0)
#define PLL_LOCKED                  (1)

#define ANGLE_INVALID               (0)
#define ANGLE_VALID                 (1)

#endif

/************ Functions ************/

uint8_t HallSensor_AutoGenFwdTable(float* angleTab, uint8_t* fwdTab);
uint8_t HallSensor_AutoGenFwdInvTable(float* angleTab, uint8_t* fwdInvTab);
uint8_t HallSensor_AutoGenRevTable(float* angleTab, uint8_t* revTab);
uint8_t HallSensor_AutoGenRevInvTable(float* angleTab, uint8_t* revInvTab);

uint8_t HallSensor_Get_State(void);
void HallSensor_Inc_Angle(void);
uint16_t HallSensor_Get_Angle(void);
float HallSensor_Get_Anglef(void);
uint32_t HallSensor_Get_Speed(void);
float HallSensor_Get_Speedf(void);
uint8_t HallSensor_Get_Direction(void);
uint8_t HallSensor_Is_Valid(void);
#ifdef TESTING_2X
uint8_t HallSensor2_Get_State(void);
void HallSensor2_Inc_Angle(void);
uint16_t HallSensor2_Get_Angle(void);
float HallSensor2_Get_Anglef(void);
uint32_t HallSensor2_Get_Speed(void);
float HallSensor2_Get_Speedf(void);
uint8_t HallSensor2_Get_Direction(void);
#endif
#ifdef TESTING_PLL
void HallSensorPLL_Update(void);
uint16_t HallSensorPLL_Get_Angle(void);
float HallSensorPLL_Get_Anglef(void);
uint32_t HallSensorPLL_Get_Speed(void);
float HallSensorPLL_Get_Speedf(void);
uint8_t HallSensorPLL_Is_Valid(void);
#endif

uint8_t HallSensor_SetAngle(uint8_t state, float newAngle);
uint8_t HallSensor_SetAngleTable(float* angleTab);
float* HallSensor_GetAngleTable(void);
float HallSensor_GetAngle(uint8_t state);
float HallSensor_GetStateMidpoint(uint8_t state);
//void HallSensor_Init(uint32_t callingFrequency);
void HallSensor_Init_NoHal(uint32_t callingFrequency);
void HallSensor_Change_Frequency(uint32_t newfreq);
void HallSensor_Enable_Hall_Detection(float* angleTable, uint8_t tableLength);
void HallSensor_Disable_Hall_Detection(void);
void HallSensor_UpdateCallback(void);
void HallSensor_CaptureCallback(void);

void HallSensor_Save_Variables(void);
void HallSensor_Load_Variables(void);

#endif /* HALLSENSOR_H_ */
