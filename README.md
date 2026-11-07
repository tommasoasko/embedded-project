# E-Agle TRT - Embedded Acoustic Analyzer (Multi-Tasking Branch)

## Overview
This repository contains the firmware for an embedded acoustic analyzer and sound level meter, developed for the STM32C0 microcontroller as part of the E-Agle Trento Racing Team recruitment project.

**This specific branch (`bonus/task5-scheduler`) implements Task 5**, migrating the system from a monolithic Finite State Machine (FSM) to a concurrent, decoupled multi-tasking architecture using the `eagletrt-tasks-api` and `timebase-api` libraries.

## Multi-Tasking System Architecture
The firmware utilizes a custom timebase scheduler executing three decoupled tasks, communicating safely via a centralized `SharedData` structure. The monolithic FSM has been split into dedicated, independent FSMs to handle specific hardware domains.

### 1. Sensor Task
Handles ADC DMA acquisitions and DSP filtering. Runs its own FSM:
- **POST:** Initializes ADC and DMA bounds.
- **PAUSED:** Halts sensor processing.
- **LISTENING:** Actively processes raw data into filtered streams (runs at 100Hz).
- **ERROR:** Catches sensor bounds violations.

### 2. Control Task
Handles the user interface, external EXTI button inputs, and PWM LED feedback. Runs a responsive FSM designed to avoid race conditions:
- **POST:** Hardware sanity check.
- **OK:** Manages the "breathing" LED effect during pause, and tracks the POT/NTC delta during reading.
- **WARNING:** Triggered if `NTC > POT` for 5 consecutive seconds. Disables sensor readings. Cleared by a button press.
- **FATAL:** Unrecoverable timeout or hardware fault. Flashes the board LED rapidly until a hard reset.

### 3. Serial Task
An independent telemetry task that streams the active system state, selected filters, and processed data via UART to the host machine without blocking the critical control paths.

### Inter-Task Communication
To prevent race conditions and ensure thread-safe-like behavior in a bare-metal environment, tasks do not call each other directly. Instead, they read and write to a globally accessible `SharedData` struct (acting as a lock-free semaphore mechanism for flags like `is_reading_active` and `is_warning_active`).

## Flash Memory Optimization
Due to the strict hardware constraints of the STM32C0 series (limited Flash memory), deploying a task scheduler alongside the STM32 HAL required extreme bare-metal optimization:
- **Newlib-Nano:** Replaced the standard C library with `nano.specs` and `nosys.specs` to drastically reduce the binary footprint.
- **Link-Time Optimization (LTO):** Enabled `-Os` and `-flto` flags, combined with garbage collection of unused sections (`-Wl,--gc-sections`), to strip unused HAL and DSP components.
- **Custom Formatting:** Replaced `sprintf` with a custom `uint_to_string` implementation to prevent the linker from importing heavy formatting libraries.

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
