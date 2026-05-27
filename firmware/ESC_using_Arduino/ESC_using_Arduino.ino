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

/* ---------------- PWM MACROS ---------------- */

#define AH_ON()   (TCCR1A |=  (1 << COM1A1))
#define AH_OFF()  (TCCR1A &= ~(1 << COM1A1))

#define BH_ON()   (TCCR1A |=  (1 << COM1B1))
#define BH_OFF()  (TCCR1A &= ~(1 << COM1B1))

#define CH_ON()   (TCCR2A |=  (1 << COM2A1))
#define CH_OFF()  (TCCR2A &= ~(1 << COM2A1))

/* ---------------- LOW SIDE GPIO ---------------- */

#define AL_ON()   (PORTD |=  (1 << PD4))
#define AL_OFF()  (PORTD &= ~(1 << PD4))

#define BL_ON()   (PORTD |=  (1 << PD7))
#define BL_OFF()  (PORTD &= ~(1 << PD7))

#define CL_ON()   (PORTB |=  (1 << PB0))
#define CL_OFF()  (PORTB &= ~(1 << PB0))

/* ---------------- SAFETY OFF ---------------- */

void all_off(void)
{
    AH_OFF(); BH_OFF(); CH_OFF();
    AL_OFF(); BL_OFF(); CL_OFF();
}

/* ---------------- PWM INIT ---------------- */

void pwm_init(void)
{
    DDRB |= (1 << PB1) | (1 << PB2) | (1 << PB3);
    DDRD |= (1 << PD4) | (1 << PD7);
    DDRB |= (1 << PB0);

    // TIMER1 – Fast PWM 8-bit
    TCCR1A = (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS11);

    // TIMER2 – Fast PWM
    TCCR2A = (1 << WGM20) | (1 << WGM21);
    TCCR2B = (1 << CS21);

    OCR1A = pwm_duty;
    OCR1B = pwm_duty;
    OCR2A = pwm_duty;

    all_off();
}

/* ---------------- COMMUTATION ---------------- */

void commutate(uint8_t s)
{
    if (fault) return;

    all_off();

    switch (s)
    {
        case 1: AH_ON(); BL_ON(); break;
        case 2: AH_ON(); CL_ON(); break;
        case 3: BH_ON(); CL_ON(); break;
        case 4: BH_ON(); AL_ON(); break;
        case 5: CH_ON(); AL_ON(); break;
        case 6: CH_ON(); BL_ON(); break;
    }

    blanking_active = 1;
    blanking_count  = 0;
}

/* ---------------- ADC ---------------- */

void adc_init(void)
{
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(uint8_t ch)
{
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

/* ---------------- FLOATING PHASE MAP ---------------- */

uint8_t floating_phase_adc(uint8_t s)
{
    switch (s)
    {
        case 1: return 2;
        case 2: return 1;
        case 3: return 0;
        case 4: return 2;
        case 5: return 1;
        case 6: return 0;
    }
    return 0;
}

/* ---------------- ZC DETECTION ---------------- */

uint8_t detect_zc(uint8_t ch)
{
    static int16_t prev_error = 0;

    int16_t v = adc_read(ch);
    int16_t error = v - NEUTRAL_ADC;

    if ((prev_error < 0 && error > 0) ||
        (prev_error > 0 && error < 0))
    {
        prev_error = error;
        return 1;
    }

    prev_error = error;
    return 0;
}

/* ---------------- CURRENT LIMIT ---------------- */

void current_limit_check(void)
{
    uint16_t cur = adc_read(3);

    if (cur > CURRENT_LIMIT_ADC)
    {
        if (pwm_duty > MIN_PWM_DUTY)
            pwm_duty--;

        OCR1A = pwm_duty;
        OCR1B = pwm_duty;
        OCR2A = pwm_duty;
    }
}

/* ---------------- UVLO ---------------- */

void uvlo_check(void)
{
    uint16_t vbus = adc_read(VBUS_ADC_CH);

    if (vbus < VBUS_MIN_ADC)
    {
        all_off();
        fault = 1;
    }
}

/* ---------------- SPEED ESTIMATION ---------------- */

float compute_speed(void)
{
    if (zc_time == 0) return 0;
    return 1000.0f / zc_time;   // scaled speed
}

/* ---------------- PID ---------------- */

float pid_update(float target, float measured)
{
    float error = target - measured;

    pid_integral += error * PID_DT;

    // anti-windup
    if (pid_integral >  200) pid_integral =  200;
    if (pid_integral < -200) pid_integral = -200;

    return (Kp * error) + (Ki * pid_integral);
}

/* ---------------- TIMER0 ---------------- */

void timer0_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 200;
    TIMSK0 = (1 << OCIE0A);
}

/* ---------------- ISR ---------------- */

ISR(TIMER0_COMPA_vect)
{
    static uint16_t counter = 0;

    /* ---- BLANKING ---- */
    if (blanking_active)
    {
        blanking_count++;
        if (blanking_count >= BLANKING_TICKS)
            blanking_active = 0;
        return;
    }

    /* ---- UVLO ---- */
    uvlo_check();
    if (fault) return;

    /* ---- CURRENT LIMIT ---- */
    current_limit_check();

    /* ---- SPEED RAMP ---- */
    ramp_counter++;
    if (ramp_counter >= RAMP_INTERVAL)
    {
        ramp_counter = 0;
        if (comm_period > TARGET_PERIOD)
            comm_period -= RAMP_RATE;
    }

    /* ---- SPEED ESTIMATE ---- */
    speed_est = compute_speed();

    /* ---- PID CONTROL ---- */
    float pid_out = pid_update(target_speed, speed_est);

    pwm_duty += (int)pid_out;

    if (pwm_duty > MAX_PWM_DUTY) pwm_duty = MAX_PWM_DUTY;
    if (pwm_duty < MIN_PWM_DUTY) pwm_duty = MIN_PWM_DUTY;

    OCR1A = pwm_duty;
    OCR1B = pwm_duty;
    OCR2A = pwm_duty;

    /* ---- SENSORLESS CORE ---- */

    uint8_t adc_ch = floating_phase_adc(step);

    if (!zc_detected)
    {
        if (detect_zc(adc_ch))
        {
            zc_time = counter;
            counter = 0;
            zc_detected = 1;
        }
    }
    else
    {
        if (counter >= zc_time)
        {
            step++;
            if (step > 6) step = 1;
            commutate(step);

            zc_detected = 0;
            counter = 0;
        }
    }

    counter++;
}

/* ---------------- MAIN ---------------- */

int main(void)
{
    pwm_init();
    adc_init();
    timer0_init();

    commutate(step);
    sei();

    while (1)
    {
        // everything runs in ISR
    }
}
