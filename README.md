# Predictive Humidity Controller

Embedded environmental controller designed to **predictively regulate indoor humidity using weather forecast data and building response modeling**.

This project is being developed as part of **EE4951 Senior Design at the University of Minnesota (Spring 2026)**.

The system monitors indoor environmental conditions and proactively adjusts humidity setpoints based on forecasted outdoor temperatures and modeled humidity response lag in buildings.

---

## Project Status

**In Development**

Current repository contents include:

- Initial firmware architecture for the controller
- System block diagram and design documentation

Additional features and hardware development are ongoing.


---

## Firmware

The firmware is written in **C for STM32 microcontrollers** using the STM32 development ecosystem.

Current firmware work includes:

- Sensor interface development
- Firmware project structure and architecture
- Initial peripheral configuration

Planned firmware features:

- Predictive humidity control algorithm
- Weather forecast data integration
- Real-time system tasks
- Display interface
- Humidifier control output

Firmware source code can be found in the firmware/ folder

---

## Hardware (In Progress)

Planned hardware components include:

- STM32 microcontroller
- SHT3x humidity and temperature sensor
- TFT display interface
- ESP8266 WiFi module
- Custom PCB with integrated power regulation

Hardware schematics and PCB design files will be added as development progresses.

---

## Development Tools

Tools used in this project include:

- STM32CubeIDE
- C / embedded firmware development
- Git for version control

Additional tools and frameworks may be added as the project evolves.
