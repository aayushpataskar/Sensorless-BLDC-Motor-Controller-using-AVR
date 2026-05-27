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
