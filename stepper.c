#include "stepper.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <stdbool.h>
#include "eeprom.h"

#define STEP_DELAY_MS      3
#define CALIB_REV_COUNT    3
#define MIN_STEPS_VALID    50      // Minimum steps between index hits to be considered a full revolution
#define MAX_STEPS_GUARD    10000   // Safety upper bound to avoid infinite loops

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
        gpio_put(ptr->pins[i], 0);
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
    gpio_pull_up(ptr->sensor_pin);   // Normal = HIGH, index gap = LOW

    ptr->step_index        = 0;
    ptr->steps_per_rev     = 0;
    ptr->calibrated        = false;
    ptr->index_hit         = false;
    ptr->slot_offset_steps = 0;

    // For power-loss recovery
    ptr->current_steps_slot = 0;
    ptr->in_motion          = false;
}
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
void stepper_calibrate(Stepper *ptr,Dispenser*dis) {
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
    save_sm_state(dis);
}

// Move forward exactly one pill slot (CW).
// During the motion we periodically save the state to EEPROM
// so that a power-loss in the middle can be detected & recovered.



void stepper_step_one_slot(Stepper *ptr, Dispenser *dis)
{
    if (!ptr->calibrated) {
        printf("[Stepper] Not calibrated.\n");
        return;
    }

    const uint16_t STEPS_PER_SLOT = HALF_STEPS;
    ptr->in_motion         = true;
    ptr->current_steps_slot = 0;

    printf("[Stepper] step_one_slot: target_steps=%u\n", STEPS_PER_SLOT);

    for (uint16_t i = 0; i < STEPS_PER_SLOT; ++i) {
        step(ptr,+1);              // CW as before
        ptr->current_steps_slot++;

        // Optional: every 8 steps, persist state
        if (dis && ((ptr->current_steps_slot % 8) == 0)) {
            save_sm_state(dis);
        }
    }

    // Finished one full slot: we are exactly at the new slot boundary
    ptr->current_steps_slot = 0;
    ptr->in_motion          = false;
    motor_off(ptr);

    // Save final “slot boundary” state
    if (dis) {
        save_sm_state(dis);
    }
}

// Apply fixed offset from index gap to pill-slot 0
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

// Power-loss recovery: if we lost power in the middle of a slot,
// we rewind back to the previous slot boundary (CCW), without dispensing.
void stepper_recovery(Stepper *ptr, Dispenser *dis) {
    if (!ptr) return;

    // If not flagged as in-motion or no partial steps recorded,
    // there is nothing to recover.
    if (!ptr->in_motion || ptr->current_steps_slot == 0) {
        printf("[Stepper] No partial slot to recover.\n");
        return;
    }

    if (ptr->current_steps_slot > HALF_STEPS) {
        ptr->current_steps_slot = HALF_STEPS;
    }

    uint16_t rollback = ptr->current_steps_slot;
    printf("[Stepper] Recovering slot (rewind CCW): already=%u, rollback=%u\n",
           ptr->current_steps_slot, rollback);

    // Rewind back to the previous slot boundary (CCW).
    for (uint16_t i = 0; i < rollback; ++i) {
        step(ptr,-1); // CCW
    }

    ptr->current_steps_slot = 0;
    ptr->in_motion          = false;
    motor_off(ptr);

    if (dis) {
        // Save the recovered "slot boundary" state
        save_sm_state(dis);
    }
}