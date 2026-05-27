# Sensorless BLDC Motor Controller — AVR / Arduino UNO

<p align="center">
  <img src="docs/assets/banner.png" alt="ESC Banner" width="700"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Arduino%20UNO%20(ATmega328P)-blue?logo=arduino" />
  <img src="https://img.shields.io/badge/Language-AVR%20Bare--Metal%20C-green" />
  <img src="https://img.shields.io/badge/Schematic-KiCad%209.0-orange?logo=kicad" />
  <img src="https://img.shields.io/badge/Motor%20Control-Sensorless%20BLDC-red" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

---

## Overview

This project is a **complete, sensorless Brushless DC (BLDC) motor controller** (Electronic Speed Controller — ESC) implemented from scratch on an **ATmega328P (Arduino UNO R3)** using **bare-metal AVR C** — no Arduino HAL, no motor-control library, no shortcuts.

The controller uses **6-step trapezoidal commutation**, **back-EMF (BEMF) zero-crossing detection** for rotor position sensing, a **PI speed control loop**, and a suite of hardware protection features — all coordinated by a single **interrupt-driven control loop running at ~1 kHz**.

The hardware is designed in **KiCad 9.0** and centres around three **IR2101 half-bridge gate drivers**, six **IRF6617 N-channel MOSFETs**, and an **LM393 dual comparator** for BEMF sensing conditioning.

---

## Key Features

| Feature | Implementation |
|---|---|
| Motor type | 3-phase sensorless BLDC |
| Commutation | 6-step trapezoidal |
| Rotor sensing | Back-EMF zero-crossing detection (software ADC) |
| Gate driving | IR2101 (bootstrap half-bridge × 3 phases) |
| PWM generation | AVR Timer1 (Fast PWM 8-bit) + Timer2 |
| Control loop | Timer0 CTC ISR @ ~1 kHz |
| Speed control | PI controller (proportional + integral) |
| Current limiting | ADC-based with PWM duty-cycle back-off |
| UVLO | Vbus ADC monitoring with hard-fault latch |
| Blanking time | Software blanking window post-commutation |
| Startup | Open-loop forced commutation ramp |
| Schematic | KiCad 9.0 |

---

## Repository Structure

```
sensorless-bldc-avr/
│
├── firmware/
│   └── ESC_using_Arduino.ino       # Main firmware (bare-metal AVR C)
│
├── hardware/
│   ├── esc_using_arduino_uno.kicad_pro
│   └── esc_using_arduino_uno.kicad_sch
│
├── docs/
│   ├── project_overview.md
│   ├── hardware_architecture.md
│   ├── firmware_architecture.md
│   ├── pwm_generation.md
│   ├── adc_and_backemf.md
│   ├── six_step_commutation.md
│   ├── pid_speed_control.md
│   ├── protection_features.md
│   ├── interrupt_architecture.md
│   ├── state_machine.md
│   ├── tuning_and_debugging.md
│   └── future_work.md
│
├── README.md
└── LICENSE
```

---

## Hardware Summary

| Component | Part | Quantity | Role |
|---|---|---|---|
| Microcontroller | Arduino UNO R3 (ATmega328P) | 1 | Control, PWM, ADC |
| Gate driver | IR2101 | 3 | High/low-side MOSFET driving |
| Power MOSFET | IRF6617 | 6 | H-bridge switching (3 half-bridges) |
| Comparator | LM393 (dual) | 1–2 | BEMF signal conditioning |
| Bootstrap diode | 1N5817 (Schottky) | 3 | IR2101 bootstrap charging |
| Gate resistor | 10 Ω | 6 | Switching transition control |
| Pull-down | 10 kΩ | 6 | Gate pull-down |
| Bootstrap cap | 100 nF | 3 | IR2101 bootstrap supply |
| Bulk capacitor | 2.2 mF | 1 | Vbus decoupling |
| Phase resistors | R_Small | 9 | Voltage divider for BEMF sensing |

> See [`docs/hardware_architecture.md`](docs/hardware_architecture.md) for the full schematic walk-through and BOM.

---

## Pin Mapping

| Arduino Pin | AVR Register | Signal | Direction |
|---|---|---|---|
| D9 (OC1A) | PB1 | Phase A — High Side (AH) | Output / PWM |
| D10 (OC1B) | PB2 | Phase B — High Side (BH) | Output / PWM |
| D11 (OC2A) | PB3 | Phase C — High Side (CH) | Output / PWM |
| D4 | PD4 | Phase A — Low Side (AL) | Output / GPIO |
| D7 | PD7 | Phase B — Low Side (BL) | Output / GPIO |
| D8 | PB0 | Phase C — Low Side (CL) | Output / GPIO |
| A0 | ADC0 | Phase A BEMF | Input / ADC |
| A1 | ADC1 | Phase B BEMF | Input / ADC |
| A2 | ADC2 | Phase C BEMF | Input / ADC |
| A3 | ADC3 | Phase current sense | Input / ADC |
| A4 | ADC4 | Vbus voltage sense | Input / ADC |

---

## Quick Start

### Prerequisites

- Arduino IDE 2.x or avr-gcc toolchain
- KiCad 9.0 (for schematic viewing/editing)
- A 3-phase BLDC motor (suitable for target Vbus)

### Building & Flashing

```bash
# Using Arduino IDE
# Open firmware/ESC_using_Arduino.ino → Select "Arduino UNO" → Upload

# Using avr-gcc directly
avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -O2 -o ESC.elf ESC_using_Arduino.ino
avr-objcopy -O ihex ESC.elf ESC.hex
avrdude -c arduino -p m328p -P /dev/ttyUSB0 -b 115200 -U flash:w:ESC.hex
```

### Key Configuration Parameters

Edit the `CONFIG` block at the top of `ESC_using_Arduino.ino`:

```c
#define NEUTRAL_ADC        512   // Virtual neutral point (midscale)
#define BLANKING_TICKS       5   // ISR ticks to suppress BEMF after commutation
#define CURRENT_LIMIT_ADC  650   // ADC counts → current trip threshold
#define MIN_PWM_DUTY        20   // Minimum duty cycle (stall prevention)
#define MAX_PWM_DUTY       180   // Maximum duty cycle (out of 255)
#define VBUS_MIN_ADC       420   // UVLO trip — Vbus too low
#define START_PERIOD       400   // Open-loop start commutation period (ticks)
#define TARGET_PERIOD      120   // Target commutation period after ramp
float Kp = 0.6f;
float Ki = 20.0f;
float target_speed = 200.0f;    // Desired speed setpoint (arbitrary units)
```

---

## Documentation Index

| Document | Contents |
|---|---|
| [Project Overview](docs/project_overview.md) | System goals, architecture summary, design decisions |
| [Hardware Architecture](docs/hardware_architecture.md) | Gate driver topology, MOSFET selection, BEMF sensing, BOM |
| [Firmware Architecture](docs/firmware_architecture.md) | Code structure, module breakdown, ISR/main interaction |
| [PWM Generation](docs/pwm_generation.md) | Timer1/Timer2 configuration, fast PWM, duty cycle update |
| [ADC & Back-EMF Sensing](docs/adc_and_backemf.md) | ADC configuration, floating phase selection, ZC detection |
| [6-Step Commutation](docs/six_step_commutation.md) | Trapezoidal theory, commutation table, timing diagram |
| [PID Speed Control](docs/pid_speed_control.md) | Speed estimation, PI equations, anti-windup, tuning |
| [Protection Features](docs/protection_features.md) | UVLO, current limiting, blanking time, fault latch |
| [Interrupt Architecture](docs/interrupt_architecture.md) | Timer0 ISR flow, ISR execution budget, latency analysis |
| [State Machine](docs/state_machine.md) | System states, transitions, startup sequence |
| [Tuning & Debugging](docs/tuning_and_debugging.md) | Parameter tuning guide, common failure modes |
| [Future Work](docs/future_work.md) | STM32 migration, FOC, SVPWM, hardware sensors |

---

## Suggested GitHub Topics

`bldc-motor-controller` · `esc` · `sensorless-bldc` · `back-emf` · `zero-crossing-detection` · `avr` · `atmega328p` · `arduino` · `bare-metal` · `motor-control` · `trapezoidal-commutation` · `pwm` · `pid-controller` · `kicad` · `ir2101` · `embedded-c` · `power-electronics`

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Author

Developed as an embedded systems engineering portfolio project.  
Platform: ATmega328P (Arduino UNO R3) · Schematic: KiCad 9.0 · Firmware: Bare-metal AVR C
