# SMT Head Controller (STM32G0 + FDCAN)

Firmware for an SMT Pick-and-Place head controller (OpenPNP) based on the **STM32G0** microcontroller using **FDCAN** in Tx Queue mode (Standard 11-bit ID).

---

## 1. Network Addressing & Protocol
* **Base CAN ID:** `0x100` (Inbound commands from Master)
* **OpenPNP Command:** `M777 P256 N{Actuator} S{State/Value};`
  * `P256` — Parsed into CAN ID `0x100`
  * `N` — Mapped to `RxData[0]` (Actuator index, range `0–255`)
  * `S` — Mapped to `RxData[1]` (State or value, raw `uint8_t`, range `0–255`)

---

## 2. Valves & Lighting Actuators (Port B)
Hardware switching is handled asynchronously via the fast register `GPIOB->BSRR`.

| Parameter `N` | Function | MCU Pin | Array Index |
| :--- | :--- | :--- | :--- |
| **N0** | Blow-off Head 1 (Blow 1) | `PB12` | `0` |
| **N1** | Blow-off Head 2 (Blow 2) | `PB13` | `1` |
| **N2** | Vacuum Head 1 (Vacuum 1) | `PB14` | `2` |
| **N3** | Vacuum Head 2 (Vacuum 2) | `PB15` | `3` |
| **N4** | Head Illumination (Light) | `PB11` | `4` |

**Manual Control Syntax:**
* **Turn ON:** `M777 P256 N{0..4} S1;`
* **Turn OFF:** `M777 P256 N{0..4} S0;`

---

## 3. Intelligent Automatic Local Blow-Off (Smart Macros)
Timeout execution is shifted to the head controller to declutter the communication channel and eliminate blocking `G4` pauses on the motion controller. Timeouts are evaluated non-blockingly inside the main loop using `uwTick`.

### Channel 1 (Head 1): `M777 P256 N10 S{Time_ms};`
1. Instantly turns OFF Vacuum 1 (`PB14`).
2. Instantly turns ON Blow-off 1 (`PB12`).
3. Starts a non-blocking software timer for `S` milliseconds (max `255 ms`), automatically resetting `PB12` to `0` upon expiration.

### Channel 2 (Head 2): `M777 P256 N11 S{Time_ms};`
1. Instantly turns OFF Vacuum 2 (`PB15`).
2. Instantly turns ON Blow-off 2 (`PB13`).
3. Starts a non-blocking software timer for `S` milliseconds (max `255 ms`), automatically resetting `PB13` to `0` upon expiration.


---

## 4. Telemetry Streaming & Combat Endstops (Port A)
Edge transitions are caught via hardware interrupts (EXTI). The first packet of a burst is fired instantly from the EXTI ISR, while remaining duplicates are queued in the main loop to ensure reliable reception over a noisy bus.

* **Hardware Debounce Window (`DEBOUNCE_MS`):** `20 ms`
* **Duplicate Burst Interval (`DUP_INTERVAL_MS`):** `2 ms` (sends 2 extra clones)
* **Background Streaming Period (`STATUS_PERIOD_MS`):** `500 ms` (X and Z state matrix)

| Axis | MCU Pin | Interrupt Handler | CAN ID (Triggered / Opened) | CAN ID (Idle / Closed) |
| :--- | :--- | :--- | :--- | :--- |
| **Axis X** | `PA2` | `EXTI2_3_IRQHandler` | `0x10` | `0x11` |
| **Axis Z** | `PA3` | `EXTI2_3_IRQHandler` | `0x20` | `0x21` |

---

## 5. LED Indicators & Hardware Bus Diagnostics (Port D)
Monitors network health via hardware error counters (`ECR` / `PSR` registers).

* **STATUS LED (RX Packets):** Pin `PD0`. Turns ON inside the RX ISR and turns OFF non-blockingly via the main loop after a `70 ms` countdown.
* **ACT LED (TX Packets):** Pin `PD1`. Turns ON during any telemetry or burst transmission and turns OFF non-blockingly after an `8 ms` countdown.

### Safety Emergency Mode (CAN Bus Fault / Bus-Off Condition)
Activated when the Transmit Error Counter exceeds `64` (`TEC > 64`) or a `Bus-Off` state occurs, exiting via hysteresis logic once `TEC` drops below `8`.
1. **Emergency Actuator Reset:** Instantly forces all valve pins (`PB11–PB15`) to logical zero via a single-cycle register write (`GPIOB->BRR = (0x1FU << 11)`).
2. **Timer Purge:** Software auto-blow execution flags (`blowX_active`) are immediately cleared to protect the valve coils from continuous load.
3. **Visual Alarm:** Standard LED timing is suppressed, and LEDs `PD0` and `PD1` switch to synchronous blinking at a fast `150 ms` period.
