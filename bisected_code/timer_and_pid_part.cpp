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
