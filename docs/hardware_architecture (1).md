# Hardware Architecture

## 1. Overview

The power stage implements a **three-phase voltage-source inverter** using six N-channel MOSFETs arranged as three half-bridges. Each half-bridge drives one motor phase (A, B, C). High-side gate driving is handled by three **IR2101** bootstrap half-bridge gate driver ICs, which solve the floating high-side drive problem without requiring an isolated power supply. BEMF voltage sensing is routed through resistor dividers to the ATmega328P's ADC inputs, with the **LM393** dual comparator available for hardware-assisted zero-crossing detection.

---

## 2. Three-Phase Half-Bridge Topology

```
        Vbus (+)
          │
    ┌─────┼─────┬─────────────┐
    │     │     │             │
   [AH]  [BH]  [CH]           │   (High-side MOSFETs — IRF6617)
    │     │     │             │
   PhA   PhB   PhC ──── BLDC Motor (star-wound)
    │     │     │             │
   [AL]  [BL]  [CL]           │   (Low-side MOSFETs — IRF6617)
    │     │     │             │
    └─────┴─────┴─────────────┘
          │
        GND (-)
```

The motor is assumed to be **star (Y) wound**, producing a virtual neutral point at the junction of the three stator windings. Two phases are driven at any instant (one high, one low) while the third phase floats — this floating phase carries the back-EMF signal used for rotor position estimation.

---

## 3. Gate Driver — IR2101

### 3.1 Why a Gate Driver Is Needed

Power MOSFETs require a gate-to-source voltage (V_GS) of typically 10–12 V to turn on fully into the low-resistance triode region. For low-side MOSFETs, the source is tied to GND, so a logic-level 5 V signal is insufficient (and would result in a partially-on MOSFET with high conduction losses and thermal stress). For high-side MOSFETs, the source sits at the phase output voltage — during conduction it can approach Vbus — so the gate must be driven to V_GS(th) _above_ a floating reference, which the microcontroller cannot do directly.

The **IR2101** solves both problems. It is a monolithic half-bridge driver providing:

- A **low-side output (LO)** referenced to GND — drives the low-side MOSFET gate
- A **high-side output (HO)** referenced to the switch node (VS pin) — drives the high-side MOSFET gate using a bootstrap-charged floating supply

### 3.2 Bootstrap Operation

The bootstrap capacitor (100 nF) is charged through the **1N5817 Schottky diode** when the low-side MOSFET is on: current flows from VCC (5 V) → diode → bootstrap capacitor → VS (switch node at ≈ GND). When the low-side turns off and the high-side is commanded on, the bootstrap capacitor's charged voltage rides up with the switch node and provides the floating supply (V_B) for the high-side driver output. The Schottky diode (low forward voltage, fast recovery) prevents the bootstrap capacitor from discharging back into the VCC rail when VS rises.

```
           VCC (+5V)
              │
             [D: 1N5817 Schottky]
              │
              ├──── VB (IR2101 bootstrap supply)
              │
             [C_boot: 100 nF]
              │
         VS ──┴──── Switch Node (Phase Output)
```

### 3.3 Dead Time

The IR2101 includes internal dead time between its HO and LO outputs, preventing simultaneous conduction of both MOSFETs in a half-bridge (shoot-through). The firmware's `all_off()` function adds a software-level dead band by turning all switches off before applying the next commutation state.

### 3.4 Input Logic

The IR2101 accepts two independent inputs (HIN and LIN) from the microcontroller:

- **HIN** — connected to the PWM output of the corresponding timer channel (OC1A, OC1B, OC2A)
- **LIN** — connected to a GPIO pin (PD4, PD7, PB0) driven directly

The firmware controls whether PWM is active on a given high-side output by enabling or disabling the timer compare output mode bits (`COM1A1`, `COM1B1`, `COM2A1`). When disabled, the pin reverts to GPIO low, which turns off the high-side driver.

---

## 4. Power MOSFETs — IRF6617

The **IRF6617** is an N-channel MOSFET in a DirectFET package. Key parameters relevant to this design:

| Parameter | Value |
|---|---|
| V_DS (max) | 25 V |
| I_D (continuous) | 13 A |
| R_DS(on) @ V_GS = 10 V | ~11 mΩ |
| Gate charge (Q_g) | ~18 nC |
| Package | DirectFET (compact, low-inductance) |

Six devices are used: three high-side (AH, BH, CH) and three low-side (AL, BL, CL).

Gate resistors (R_Small, typically 10 Ω) in series with each gate limit the gate current inrush and control the switching transition speed, reducing EMI and voltage overshoot at the cost of slightly slower switching transitions. Pull-down resistors (10 kΩ) ensure the gate is held low when the driver output is high-impedance (e.g., during startup before the driver IC is enabled).

---

## 5. BEMF Sensing Network

### 5.1 Resistor Divider

Motor phase voltages during operation swing from 0 V to Vbus. The ATmega328P ADC input is limited to 0–5 V (AVCC reference). A resistor divider scales the phase voltage to a safe ADC input range. Three identical dividers (one per phase) feed ADC channels 0, 1, and 2.

```
Phase Voltage (0 → Vbus)
        │
       [R_top]
        │
        ├──── ADC Input (0 → 5 V scaled)
        │
       [R_bot]
        │
       GND
```

The virtual neutral point for zero-crossing comparison is set in firmware as `NEUTRAL_ADC = 512`, corresponding to the midpoint of the 10-bit ADC range (2.5 V on the divided input, which corresponds to Vbus/2 on the phase).

### 5.2 LM393 Comparator Path

The **LM393** dual comparator is schematically connected to compare the BEMF-divided phase voltage against the virtual neutral. Its open-collector output can be connected to a microcontroller GPIO/INT pin for hardware interrupt-driven zero-crossing detection. In the current firmware, this path is not used — zero-crossing is detected in software via the ADC — but the hardware is in place for an upgrade to comparator-interrupt detection (see [`future_work.md`](future_work.md)).

---

## 6. Current Sensing

A shunt resistor in the low-side current path produces a voltage proportional to motor current. This voltage is routed to **ADC channel 3**. The firmware reads this channel each ISR tick and reduces the PWM duty cycle if the ADC count exceeds `CURRENT_LIMIT_ADC = 650`.

> **Note:** A production design would use a dedicated current-sense amplifier (e.g., INA219, INA240) to amplify the small shunt voltage and provide common-mode rejection. The current sensing in this design is approximate.

---

## 7. Vbus Sensing (UVLO)

A resistor divider from the Vbus rail to GND feeds **ADC channel 4**. The firmware monitors this continuously and asserts a fault latch if the reading falls below `VBUS_MIN_ADC = 420`, corresponding to an under-voltage condition on the bus. This protects the power electronics and prevents erratic operation caused by a sagging supply.

---

## 8. Decoupling and Filtering

A bulk electrolytic capacitor (2.2 mF, referenced as `C2.2MF` in the schematic) is placed across the Vbus supply. This capacitor:

- Absorbs current spikes during MOSFET switching transitions
- Reduces ripple on Vbus caused by motor commutation currents
- Stores energy during regenerative braking pulses

Small ceramic decoupling capacitors (100 nF) are placed on each IR2101 VCC and VB pin for high-frequency noise bypass.

---

## 9. Bill of Materials (BOM)

| Reference | Value / Part | Description | Qty |
|---|---|---|---|
| U1, U2, U3 | IR2101 | Half-bridge MOSFET gate driver | 3 |
| Q1–Q6 | IRF6617 | N-channel power MOSFET | 6 |
| U4 | LM393 | Dual voltage comparator | 1 |
| A1 | Arduino UNO R3 | ATmega328P control module | 1 |
| D1, D2, D3 | 1N5817 | Schottky bootstrap diode | 3 |
| C1, C2, C3 | 100 nF (ceramic) | Bootstrap capacitor | 3 |
| C4 | 2.2 mF (electrolytic) | Vbus bulk decoupling | 1 |
| R1–R6 | 10 Ω | Gate resistors | 6 |
| R7–R12 | 10 kΩ | Gate pull-downs | 6 |
| R13–R18 | R_Small (see note) | BEMF voltage divider resistors | 9 |
| R_SHUNT | R_Small | Current sense shunt | 1 |
| R_VBUS | 10 kΩ / R_Small pair | Vbus sense divider | 2 |

> Resistor values for the BEMF dividers and Vbus divider should be calculated for the specific Vbus operating voltage to ensure the divided signal fits within the 0–5 V ADC input range.

---

## 10. Schematic Navigation (KiCad)

Open `hardware/esc_using_arduino_uno.kicad_sch` in KiCad 9.0. The schematic is organised as follows:

- **Top section**: Vbus input, bulk capacitor, three half-bridge legs (IR2101 + MOSFET pairs)
- **Middle section**: Arduino UNO module with PWM and GPIO connections to gate drivers
- **Bottom section**: BEMF sensing resistor dividers, LM393 comparator circuit, ADC connections
- **Right section**: Current sense and Vbus sense circuits

The KiCad project file (`esc_using_arduino_uno.kicad_pro`) contains ERC rule settings and net class definitions.
