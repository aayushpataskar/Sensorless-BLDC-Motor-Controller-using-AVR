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
