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
