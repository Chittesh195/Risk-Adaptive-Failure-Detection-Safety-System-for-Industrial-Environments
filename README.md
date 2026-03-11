# Risk-Adaptive & Failure Detection Safety System for Industrial Environments

## Overview

This project presents an IoT-based industrial safety system that monitors environmental hazards and machinery risks in real time. The system integrates multiple sensors and microcontrollers to detect fire, smoke, unsafe proximity to machines, and unauthorized access while automatically triggering alerts and safety controls. 

## Features

* Fire and smoke detection with alarm alerts
* Ultrasonic-based proximity monitoring for machine safety
* OTP-based secure worker and visitor authentication
* Automatic door and window control using servo motors
* Real-time monitoring through LCD, OLED, and web dashboard
* Emergency machine shutdown during dangerous conditions

## Hardware Components

* ESP32 (IoT communication & web dashboard)
* STM32F4 (risk-adaptive machine control)
* Raspberry Pi Pico (environment monitoring)
* Arduino (motion and temperature monitoring)
* Sensors: Smoke, Flame, Rain, PIR, Ultrasonic, DHT11
* Actuators: Servo motors, relay module, buzzer, LEDs
* Displays: 16x2 LCD and OLED display

## Software Used

* Embedded C (STM32)
* Arduino C++
* MicroPython (ESP32 & Pico)

## Applications

* Smart factories
* Manufacturing industries
* Power plants
* Chemical industries
* Industrial motor control rooms

## Future Improvements

* Cloud data storage and analytics
* AI-based hazard prediction
* Face recognition authentication
* Mobile app integration for alerts


