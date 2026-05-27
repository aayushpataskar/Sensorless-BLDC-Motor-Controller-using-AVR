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
