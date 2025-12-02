//
// Created by HaoKun Tong on 2025/12/2.
//
#include "stepper.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define STEP_DELAY_MS      1
#define CALIB_REV_COUNT    3
#define MIN_STEPS_VALID    50      // Minimum steps between index hits to be considered a full revolution
#define MAX_STEPS_GUARD    10000   // Safety upper bound to avoid infinite loops

// Global pointer for the IRQ handler (RP2040 has a single global GPIO IRQ callback)
static Stepper *s_stepper = NULL;

// Half-step sequence (LSB -> pins[0])
static const uint8_t half_steps[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

// Optical sensor IRQ: assume normal = HIGH, index gap = LOW
static void sensor_isr(uint gpio, uint32_t events) {
    if (!s_stepper) return;
    if (gpio != s_stepper->sensor_pin) return;
    if (events & GPIO_IRQ_EDGE_FALL) {
        s_stepper->index_hit = true;
    }
}

// Single half-step; dir = +1 for CW, -1 for CCW
static void step(Stepper *ptr, int dir) {
    ptr->step_index = (ptr->step_index + dir + 8) % 8;

    for (int i = 0; i < 4; i++) {
        gpio_put(ptr->pins[i], half_steps[ptr->step_index][i]);
    }

    sleep_ms(STEP_DELAY_MS);
}

// Turn off the motor (all coils off)
static void motor_off(Stepper *ptr) {
    (void)ptr; // Currently unused; keeps the compiler happy
    for (int i = 0; i < 4; i++) {
        gpio_put(s_stepper->pins[i], 0);
    }
}

void stepper_init(Stepper *ptr) {
    // Configure coil GPIOs
    for (int i = 0; i < 4; i++) {
        gpio_init(ptr->pins[i]);
        gpio_set_dir(ptr->pins[i], GPIO_OUT);
        gpio_put(ptr->pins[i], 0);
    }

    // Configure optical sensor GPIO
    gpio_init(ptr->sensor_pin);
    gpio_set_dir(ptr->sensor_pin, GPIO_IN);
    gpio_pull_up(ptr->sensor_pin);   // Normal = HIGH, index gap = LOW (change to pull-down if wiring is inverted)

    ptr->step_index    = 0;
    ptr->steps_per_rev = 0;
    ptr->calibrated    = false;
    ptr->index_hit     = false;
    ptr->slot_offset_steps=0;

    // Register GPIO IRQ callback
    s_stepper = ptr;

}

void stepper_calibrate(Stepper *ptr) {
    printf("Calibrating...\n");

    ptr->calibrated    = false;
    ptr->steps_per_rev = 0;
    ptr->index_hit     = false;

    // 1) Make sure we are not starting inside the index gap
    if (gpio_get(ptr->sensor_pin) == 0) {
        int guard = 0;
        while (gpio_get(ptr->sensor_pin) == 0) {
            step(ptr, +1);
            if (++guard > MAX_STEPS_GUARD) {
                printf("Error: stuck in index gap.\n");
                motor_off(ptr);
                return;
            }
        }
    }

    // 2) Find the first index edge (sync point, not counted as a full revolution)
    int guard = 0;
    ptr->index_hit = false;
    while (!ptr->index_hit) {
        step(ptr, +1);
        if (++guard > MAX_STEPS_GUARD) {
            printf("Error: index not detected. Check sensor.\n");
            motor_off(ptr);
            return;
        }
    }
    ptr->index_hit = false;

    // 3) Measure several full revolutions (index→index)
    int rev_done          = 0;
    int total_steps       = 0;
    int steps_since_index = 0;

    while (rev_done < CALIB_REV_COUNT) {
        step(ptr, +1);
        steps_since_index++;

        if (steps_since_index > MAX_STEPS_GUARD) {
            printf("Error: no index within expected range.\n");
            motor_off(ptr);
            return;
        }

        if (ptr->index_hit) {
            ptr->index_hit = false;

            if (steps_since_index >= MIN_STEPS_VALID) {
                rev_done++;
                total_steps += steps_since_index;
                printf("Rev %d: %d steps\n", rev_done, steps_since_index);
                steps_since_index = 0;
            } else {
                // Too short; treat as noise/bounce
                steps_since_index = 0;
            }
        }
    }

    ptr->steps_per_rev = total_steps / CALIB_REV_COUNT;
    ptr->calibrated    = true;
    motor_off(ptr);

    printf("Calibration OK. steps_per_rev = %d\n", ptr->steps_per_rev);
}

// Internal helper: run "section × 1/8 revolutions"; section=8 => full revolution
static void stepper_run_sections(Stepper *ptr, int section, int dir) {
    if (!ptr->calibrated) {
        printf("Not calibrated. Call stepper_calibrate() first.\n");
        return;
    }

    int steps = (ptr->steps_per_rev * section) / 8;
    if (steps <= 0) return;

    while (steps-- > 0) {
        step(ptr, dir);
    }
    motor_off(ptr);
}

// Public API: one pill slot = 1/8 revolution (CW)
void stepper_step_one_slot(Stepper *ptr) {
    stepper_run_sections(ptr, 1, +1);
}



void stepper_apply_slot_offset(Stepper *ptr) {
    int steps = ptr->slot_offset_steps;
    if (steps == 0) {
        printf("No slot offset applied.\n");
        return;
    }

    int dir = (steps >= 0) ? +1 : -1;
    if (steps < 0) steps = -steps;

    printf("Apply slot offset: %d half-steps (%s)\n",
           steps, dir > 0 ? "CW" : "CCW");

    while (steps-- > 0) {
        step(ptr, dir);
    }
    motor_off(ptr);
}