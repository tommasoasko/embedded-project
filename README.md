# E-Agle TRT - Embedded Acoustic Analyzer

## Overview
This repository contains the firmware for an embedded acoustic analyzer and sound level meter, developed for the STM32C0 microcontroller as part of the E-Agle Trento Racing Team recruitment project. 

The system acquires data from two analog sensors (a potentiometer simulating a microphone/phonometer and an NTC thermistor) via ADC and DMA, processes the signals, and sends telemetry data over UART. It also provides visual feedback using onboard and external LEDs via PWM.

## System Architecture
The firmware is built around a non-blocking **Finite State Machine (FSM)** utilizing the `eagletrt-fsm-api` library. The system transitions between the following states:
- **POST (Power-On Self-Test):** Initializes peripherals and verifies sensor bounds via DMA.
- **WAITING:** Idle state waiting for user input. Board LED blinks, external LED performs a 2-second "breathing" effect. Command Line Interface (CLI) is active.
- **LISTENING:** Active state. Streams processed sensor data via UART every 100ms. Disables CLI to prevent interruptions.
- **PAUSE:** Halts data streaming while keeping the system active and re-enabling the CLI.
- **WARNING:** Triggered if the NTC value exceeds the potentiometer value by a specific threshold for 5 continuous seconds.
- **FATAL:** Unrecoverable error state (e.g., hardware fault or warning timeout). Halts operations and triggers specific LED patterns until a reset.

## Features & Signal Processing
The system supports real-time data filtering, selectable via the CLI during the `WAITING` or `PAUSE` states:
1. **Raw Data:** Bypasses filters and streams raw ADC values.
2. **Moving Average Filter:** Smooths the signal using a discrete sliding window.
3. **Random Noise Filter:** Applies a simulated noise profile to the signal.

## Hardware Requirements
- STM32C0 series development board
- 1x Potentiometer (ADC Input)
- 1x NTC Thermistor (ADC Input)
- 1x External LED (PWM Output)
- 1x Push Button (EXTI Input)

## Host Dashboard (Python)
A Python-based application (`audio_synth.py`) is provided to receive, parse, and utilize the UART telemetry stream from the STM32.

### Prerequisites
Ensure you have Python 3.8+ installed on your host machine. Install the required dependencies using `pip`:

```bash
pip install pyserial numpy
