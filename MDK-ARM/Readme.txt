/* ============================================================================
 * 📑 TECHNICAL SPECIFICATION & ARCHITECTURE ANNOTATION: SMT HEAD CONTROLLER
 * ============================================================================
 * Target MCU: STM32G0 Series
 * Interface:  FDCAN in Tx Queue Mode (Standard 11-bit ID)
 * 
 * 1. NETWORK ADDRESSING & PROTOCOL
 * ----------------------------------------------------------------------------
 * - Base CAN ID:     0x100 (Inbound commands from Master)
 * - OpenPNP Command: M777 P256 N{Actuator} S{State/Value};
 *   * P256 -> Parsed into CAN ID 0x100
 *   * N    -> Mapped to RxData[0] (Actuator index, range 0-255)
 *   * S    -> Mapped to RxData[1] (State or value, raw uint8_t range 0-255)
 * 
 * 2. VALVES & LIGHTING ACTUATORS (PORT B)
 * ----------------------------------------------------------------------------
 * Hardware switching is handled asynchronously via fast register GPIOB->BSRR.
 * 
 * - N0 : Blow-off Head 1 (Blow 1)    -> Pin: PB12 | Array Index: 0
 * - N1 : Blow-off Head 2 (Blow 2)    -> Pin: PB13 | Array Index: 1
 * - N2 : Vacuum Head 1   (Vacuum 1)  -> Pin: PB14 | Array Index: 2
 * - N3 : Vacuum Head 2   (Vacuum 2)  -> Pin: PB15 | Array Index: 3
 * - N4 : Head Illumination (Light)   -> Pin: PB11 | Array Index: 4
 * 
 * Manual Control Syntax:
 * - Turn ON : M777 P256 N{0..4} S1;
 * - Turn OFF: M777 P256 N{0..4} S0;
 * 
 * 3. INTELLIGENT AUTOMATIC LOCAL BLOW-OFF (Smart Macros)
 * ----------------------------------------------------------------------------
 * Shifts timeout execution to the head controller to declutter the communication 
 * channel and eliminate blocking G4 pauses on the motion controller.
 * Timeouts are evaluated non-blockingly inside main loop using 'uwTick'.
 * 
 * 🔹 Channel 1 (Head 1): M777 P256 N10 S{Time_ms};
 *   1. Instantly turns OFF Vacuum 1 on PB14.
 *   2. Instantly turns ON Blow-off 1 on PB12.
 *   3. Starts non-blocking software timer for S milliseconds (max 255ms).
 *      PB12 is automatically reset to 0 upon timeout expiration.
 * 
 * 🔹 Channel 2 (Head 2): M777 P256 N11 S{Time_ms};
 *   1. Instantly turns OFF Vacuum 2 on PB15.
 *   2. Instantly turns ON Blow-off 2 on PB13.
 *   3. Starts non-blocking software timer for S milliseconds (max 255ms).
 *      PB13 is automatically reset to 0 upon timeout expiration.
 * 
 * 4. TELEMETRY STREAMING & COMBAT ENDSTOPS (PORT A)
 * ----------------------------------------------------------------------------
 * Edge transitions are caught via hardware interrupts. The first packet of 
 * a burst is fired instantly from the EXTI ISR, remaining duplicates are 
 * queued in the main loop to ensure reception over the noisy bus.
 * 
 * - Hardware Debounce Window (DEBOUNCE_MS):       20 ms
 * - Duplicate Burst Interval (DUP_INTERVAL_MS):   2 ms (sends 2 extra clones)
 * - Background Streaming Period (STATUS_PERIOD_MS): 500 ms (X and Z matrix)
 * 
 * 🗲 Endstop Axis X:
 * - Hardware Pin: PA2 (EXTI2_3_IRQHandler)
 * - Outbound CAN IDs:
 *   * 0x10 -> Endstop X triggered / opened
 *   * 0x11 -> Endstop X idle / closed
 * 
 * 🗲 Endstop Axis Z:
 * - Hardware Pin: PA3 (EXTI2_3_IRQHandler)
 * - Outbound CAN IDs:
 *   * 0x20 -> Endstop Z triggered / opened
 *   * 0x21 -> Endstop Z idle / closed
 * 
 * 5. LED INDICATORS & HARDWARE BUS DIAGNOSTICS (PORT D)
 * ----------------------------------------------------------------------------
 * Monitors network sanity via hardware error counters (ECR/PSR registers).
 * 
 * - STATUS LED (RX Packets): Pin PD0. Turns ON inside RX ISR, turns OFF 
 *   non-blockingly via main loop after a 70 ms countdown.
 * - ACT LED    (TX Packets): Pin PD1. Turns ON during any telemetry or burst 
 *   transmission, turns OFF non-blockingly after an 8 ms countdown.
 * 
 * 🛑 Safety Emergency Mode (CAN Bus Fault / Bus-Off Condition)
 * - Activation Criteria: Enter error state when TEC > 64 or Bus-Off occurs 
 *   (handled via built-in hysteresis logic), exit when TEC drops below 8.
 * - Emergency Actions Taken:
 *   1. Emergency Actuator Reset: Instantly forces all valve pins (PB11-PB15) 
 *      to logical zero via single-cycle register write (GPIOB->BRR = (0x1FU << 11)).
 *   2. Timer Purge: Software auto-blow execution flags ('blowX_active') are 
 *      immediately cleared to protect the coils from continuous load.
 *   3. Visual Alarm: Standard LED timing is suppressed. LEDs PD0 and PD1 
 *      switch to fully synchronous, monotonic blinking at a fast 150 ms period.
 * ============================================================================
 */
