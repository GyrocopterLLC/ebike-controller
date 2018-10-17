#ifndef _THROTTLE_H_
#define _THROTTLE_H_

#include "stm32f4xx.h"
#include "DavidsFOCLib.h"
#include "project_parameters.h"
#include "pinconfig.h"
#include "periphconfig.h"

#define PAS_TIMER_INPUT_CLOCK     84000000 // APB1 clock * 2
#define PAS_TIM_PSC               8399 // 0.1ms per tick - 10kHz clock
#define PAS_TIM_ARR               9999 // Reset at 1 second (0->9999)
#define PAS_CLK                   10000
#define PAS_PPR                   12 // pulses per rotation (number of magnets)

#define THROTTLE_START_TIME			1000
#define THROTTLE_START_DEADTIME		500
#define THROTTLE_RANGE_LIMIT		0.05f
#define THROTTLE_MIN_DEFAULT		0.85f
#define THROTTLE_MAX_DEFAULT		2.20f
#define THROTTLE_HYST_LOW			0.025f
#define THROTTLE_HYST_HIGH			0.030f
#define THROTTLE_DROPOUT			0.72f

#define THROTTLE_OUTPUT_MIN   (0.00f)
#define THROTTLE_OUTPUT_MAX   (0.99f)
// Limit the throttle climb rate to 50% / second
// The update rate is 1000Hz, so the rate limit is actually .125% per update
#define THROTTLE_SLEW_RATE  (0.00125f)

#define THROTTLE_TYPE_ANALOG        0
#define THROTTLE_TYPE_PAS           1
#define THROTTLE_TYPE_NONE          2

typedef struct
{
  uint8_t throttle_type;
  uint8_t state;
  float throttle_command;
  float prev_output;
}Throttle_Type;

typedef struct
{
	uint32_t startup_count;
	float min;
	float max;
	float scale_factor;
	float hyst;
	float filt;
	float rise;
}Throttle_Analog_Type;

typedef struct
{
  float filtered_speed;
  float scale_factor;
} Throttle_PAS_Type;

#define THROTTLE_DEFAULTS         {THROTTLE_TYPE_ANALOG, 0, 0.0f, 0.0f}
#define THROTTLE_ANALOG_DEFAULTS	{0, 0.0f, 0.0f, 1.0f}
#define THROTTLE_PAS_DEFAULTS     {0.0f, 0.015f}
#define PAS_FILTER                (0.125f) // LPF of 1/8

// Biquad filter: Fs = 1kHz, f0 = 2Hz, Q = 0.45
// Little bit sluggish response. Maybe feels safer?
#define THROTTLE_LPF_DEFAULTS  { \
                -1.972304f, \
                0.9724600f, \
                0.00003893429f, \
                0.00007786857f, \
                0.00003893429f, \
                0.0f, \
                0.0f, \
                0.0f, \
                0.0f }


void throttle_process(uint8_t thrnum);
void throttle_pas_process(uint8_t thrnum);
void throttle_pas_timer_overflow(uint8_t thrnum);
float throttle_get_command(uint8_t thrnum);

uint8_t throttle_set_type(uint8_t thrnum, uint8_t thrtype);
uint8_t throttle_get_type(uint8_t thrnum);
uint8_t throttle_set_min(uint8_t thrnum, int32_t thrmin);
int32_t throttle_get_min(uint8_t thrnum);
uint8_t throttle_set_max(uint8_t thrnum, int32_t thrmax);
int32_t throttle_get_max(uint8_t thrnum);
uint8_t throttle_set_hyst(uint8_t thrnum, int32_t thrhyst);
int32_t throttle_get_hyst(uint8_t thrnum);
uint8_t throttle_set_filt(uint8_t thrnum, int32_t thrfilt);
int32_t throttle_get_filt(uint8_t thrnum);
uint8_t throttle_set_rise(uint8_t thrnum, int32_t thrrise);
int32_t throttle_get_rise(uint8_t thrnum);

#endif /* _THROTTLE_H_ */
