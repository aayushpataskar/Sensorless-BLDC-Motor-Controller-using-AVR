#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

/* ---------------- CONFIG ---------------- */

#define NEUTRAL_ADC        512
#define BLANKING_TICKS    5

#define CURRENT_LIMIT_ADC 650
#define MIN_PWM_DUTY      20
#define MAX_PWM_DUTY      180

/* -------- SPEED RAMP CONFIG -------- */
#define START_PERIOD   400
#define TARGET_PERIOD  120
#define RAMP_RATE      1
#define RAMP_INTERVAL  20

/* -------- UVLO CONFIG -------- */
#define VBUS_ADC_CH     4
#define VBUS_MIN_ADC   420

/* -------- PID CONFIG -------- */
#define PID_DT         0.001f   // 1 ms loop

float Kp = 0.6f;
float Ki = 20.0f;

/* ---------------- GLOBALS ---------------- */

volatile uint8_t step = 1;

volatile uint16_t zc_time = 0;
volatile uint8_t  zc_detected = 0;

volatile uint8_t blanking_active = 0;
volatile uint8_t blanking_count  = 0;

volatile uint8_t pwm_duty = 80;

/* ---- SPEED RAMP ---- */
volatile uint16_t comm_period = START_PERIOD;
volatile uint16_t ramp_counter = 0;

/* ---- FAULT FLAG ---- */
volatile uint8_t fault = 0;

/* ---- PID STATE ---- */
float pid_integral = 0;
float target_speed = 200.0f;     // arbitrary units
float speed_est    = 0;
