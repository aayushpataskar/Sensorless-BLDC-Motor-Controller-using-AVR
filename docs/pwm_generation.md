# PWM Generation

## 1. Background — Why PWM for Motor Control

A BLDC motor is driven by controlling the average voltage applied to each phase. Since the gate drivers and MOSFETs are switching devices (on or off), average voltage control is achieved through **Pulse-Width Modulation (PWM)**: the high-side MOSFET for the active phase is rapidly switched on and off at a fixed frequency, and the ratio of on-time to total period (duty cycle, D) sets the average phase voltage:

```
V_phase_avg = D × V_bus,    where D = OCR / 255   (for 8-bit Fast PWM)
```

A higher duty cycle means more average voltage, which drives more current through the motor windings and produces more torque and speed.

---

## 2. Timer Configuration

The ATmega328P has three hardware timers available: Timer0 (8-bit), Timer1 (16-bit), and Timer2 (8-bit). In this design:

| Timer | Role | Mode | Output Channels Used |
|---|---|---|---|
| Timer0 | Control tick ISR | CTC (Clear Timer on Compare) | None (ISR only) |
| Timer1 | PWM for Phase A and B high-side | Fast PWM 8-bit (WGM = 0b101) | OC1A (PB1/D9), OC1B (PB2/D10) |
| Timer2 | PWM for Phase C high-side | Fast PWM (WGM = 0b11) | OC2A (PB3/D11) |

Timer0 is deliberately kept separate from the PWM timers so that the control ISR frequency is independent of the PWM frequency.

---

## 3. Timer1 — Fast PWM 8-bit

### Register Configuration

```c
// Fast PWM 8-bit: WGM1[2:0] = 0b101 (WGM10 + WGM12)
TCCR1A = (1 << WGM10);                       // WGM10 = 1
TCCR1B = (1 << WGM12) | (1 << CS11);         // WGM12 = 1, prescaler /8
```

**Waveform Generation Mode (WGM):**

| Bit | WGM12 | WGM11 | WGM10 | Mode |
|---|---|---|---|---|
| Value | 1 | 0 | 1 | Fast PWM, 8-bit (TOP = 0xFF = 255) |

In Fast PWM mode, the counter increments from BOTTOM (0x00) to TOP (0xFF), then resets immediately to BOTTOM. The output compare pin is set at BOTTOM and cleared when the counter matches OCRnX (non-inverting mode, COM1A1=1, COM1B1=1).

**Prescaler Selection (CS11 = 1 → /8):**

```
f_PWM = f_CPU / (prescaler × (TOP + 1))
      = 16,000,000 / (8 × 256)
      = 7,812.5 Hz ≈ 7.8 kHz
```

A PWM frequency of ~7.8 kHz is above the audible range (> 20 kHz is ideal, but 7.8 kHz reduces audible switching noise sufficiently for most applications) while keeping switching losses manageable with the IRF6617's gate charge.

**Compare Output Mode:**

The `AH_ON()` and `BH_ON()` macros set `COM1A1` and `COM1B1` respectively, connecting OC1A/OC1B to their output pins in non-inverting fast PWM mode. `AH_OFF()` and `BH_OFF()` clear these bits, disconnecting the PWM and driving the pin low — effectively turning off the high-side gate driver input.

```c
#define AH_ON()   (TCCR1A |=  (1 << COM1A1))   // OC1A → PWM output
#define AH_OFF()  (TCCR1A &= ~(1 << COM1A1))   // OC1A → GPIO low
```

This mechanism is the key insight of the commutation implementation: rather than reconfiguring timer hardware between steps, the firmware simply connects or disconnects the already-running PWM output to the pin.

---

## 4. Timer2 — Fast PWM

```c
// Fast PWM: WGM2[1:0] = 0b11 (WGM20 + WGM21), prescaler /8
TCCR2A = (1 << WGM20) | (1 << WGM21);        // Fast PWM, TOP = 0xFF
TCCR2B = (1 << CS21);                         // prescaler /8
```

Timer2 mirrors Timer1's PWM parameters for Phase C (OC2A, PB3/D11). Both timers run at the same nominal frequency (~7.8 kHz), which is desirable to keep all three phases switching in phase — though Timer1 and Timer2 are not hardware-synchronised in this design and will accumulate phase drift over time. A future improvement would synchronise the two timers' counter values at startup.

---

## 5. Duty Cycle Update

The duty cycle is stored in the global `pwm_duty` variable (0–255) and written to all three compare registers simultaneously during the ISR:

```c
OCR1A = pwm_duty;   // Phase A high-side
OCR1B = pwm_duty;   // Phase B high-side
OCR2A = pwm_duty;   // Phase C high-side
```

Since only one high-side is active at any commutation step, writing to all three registers is safe — the inactive channels are disconnected from their pins by the `AH_OFF()`/`BH_OFF()`/`CH_OFF()` macros.

**Duty cycle is bounded:**

```c
if (pwm_duty > MAX_PWM_DUTY) pwm_duty = MAX_PWM_DUTY;   // 180 / 255 ≈ 70.6%
if (pwm_duty < MIN_PWM_DUTY) pwm_duty = MIN_PWM_DUTY;   // 20  / 255 ≈  7.8%
```

The maximum duty cycle is intentionally limited to 180/255 to prevent the bootstrap capacitor from discharging completely (the low-side must turn on periodically to recharge the bootstrap capacitor; a 100% duty cycle would starve the bootstrap supply and cause gate drive failure).

---

## 6. Low-Side Drive — GPIO

The low-side MOSFETs (AL, BL, CL) are driven directly by GPIO pins. Since the low-side source is tied to GND, a 5 V logic signal provides adequate V_GS to drive the IR2101 LIN input. The low-side is either fully on or fully off — PWM is only applied to the high side (asymmetric PWM, complementary to the high-side PWM). This is a common simplification for trapezoidal control.

```c
#define AL_ON()   (PORTD |=  (1 << PD4))
#define AL_OFF()  (PORTD &= ~(1 << PD4))
```

---

## 7. PWM Timing Diagram (Single Step)

```
                  PWM Period (~128 µs @ 7.8 kHz)
          │◄─────────────────────────────────────►│
          │                                        │
High-side │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░│▓▓...
(e.g. AH) │ ON (OCR counts)          OFF            │
          │                                        │
Low-side  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│▓▓...
(e.g. BL) │ ALWAYS ON during this step             │
          │                                        │
Phase C   │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│...
(floating)│ FLOATING — BEMF measurable here        │
```

The floating phase carries the back-EMF voltage which crosses the virtual neutral at the zero-crossing point used for commutation timing.

---

## 8. Timer0 — Control Tick

Timer0 operates in **CTC (Clear Timer on Compare) mode**:

```c
TCCR0A = (1 << WGM01);                         // CTC mode
TCCR0B = (1 << CS01) | (1 << CS00);            // prescaler /64
OCR0A  = 200;                                   // Compare value
TIMSK0 = (1 << OCIE0A);                         // Enable compare-match interrupt
```

ISR period:

```
f_ISR  = f_CPU / (prescaler × (OCR0A + 1))
       = 16,000,000 / (64 × 201)
       ≈ 1,245 Hz
T_ISR  ≈ 803 µs
```

This ~1.25 kHz tick drives all control and monitoring functions. It is entirely independent of the PWM frequency, which is correct — the PWM runs continuously at 7.8 kHz while the control loop updates duty cycle at ~1.25 kHz.
