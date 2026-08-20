/*
 * Stepper.h — L298N stepper sweep component
 * -----------------------------------------
 * Public interface. State and definitions live in Stepper.cpp, so the timer
 * handle and position counter exist exactly once no matter how many files
 * include this header.
 *
 * Wiring:
 *   IN1..IN4 GPIO -> L298N IN1..IN4   Coil A = OUT1/OUT2, Coil B = OUT3/OUT4
 *   L298N ENA,ENB : jumpered to 5V (always enabled)
 *   L298N GND     : MUST be common with the ESP32 GND
 */
#pragma once

#include <cstdint>
#include "driver/gpio.h"

/* ---- Wiring: four L298N inputs ----
 * General-purpose, output-capable on every ESP32 module. Avoid 34-39
 * (input-only), 6-11 (SPI flash), 1/3 (UART console), 18 (servo here),
 * the I2C pins, and 16/17 on WROVER (PSRAM).
 */
#define STEP_IN1_GPIO        GPIO_NUM_32    /* Coil A */
#define STEP_IN2_GPIO        GPIO_NUM_33
#define STEP_IN3_GPIO        GPIO_NUM_25   /* Coil B */
#define STEP_IN4_GPIO        GPIO_NUM_26

/* ---- Motor / sweep parameters (tune to your motor) ---- */
#define STEP_STEPS_PER_REV   200      /* full-step count for one shaft rev (NEMA17=200) */
#define STEP_SWEEP_STEPS     100      /* how many steps wide the sweep is */
#define STEP_INTERVAL_US     2500     /* time between steps; smaller = faster */

/* Configure the four GPIOs and create (but don't start) the step timer. */
void stepper_init(void);

/* Start / stop the automatic bounce sweep (runs in the background). */
void stepper_start_sweep(void);
void stepper_stop(void);

/* Advance exactly one step yourself — for a step/sample/step scan loop.
   Don't mix with the auto sweep; use one or the other. */
void stepper_step_once(bool forward);

/* Current position in steps — tag your detections with this. */
int32_t stepper_position(void);

/* Current position expressed in degrees of shaft rotation. */
float stepper_degrees(void);

/* Cut current to all coils (drops holding torque, saves power, runs cool). */
void stepper_release(void);
