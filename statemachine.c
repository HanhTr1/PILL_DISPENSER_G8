// statemachine.c
#include "statemachine.h"
#include "dispenser_initialize.h"
#include "button_handler.h"
#include"stepper.h"
#include <stdio.h>
#include"board_config.h"
// Half-step offset between optical index and the first pill slot.
// You measured that one slot ≈ 144 half-steps.


void statemachine_init(Dispenser *dis,
                       Stepper *motor,
                       pillSensorState *sensor,
                       uint8_t pills_to_dispense,
                       uint32_t interval_ms)
{
    // Use existing helper to configure button / LED / piezo pins
    dispenser_init(dis, SW_0,SW_2, LED_PIN, PIEZO_PIN);

    // Attach modules
    dis->motor  = motor;
    dis->sensor = sensor;

    // High-level logic parameters
    dis->pills_left  = pills_to_dispense;
    dis->interval_ms = interval_ms;

    // Statistics
    dis->total_dispense_count  = 0;
    dis->failed_dispense_count = 0;

    // First target time for dispensing
    dis->next_dispense_time = make_timeout_time_ms(interval_ms);
}

void statemachine_step(Dispenser *dis) {
    switch (dis->state) {

    case ST_WAIT_FOR_BUTTON:
        // First button press -> go to calibration state
        wait_button_handler(dis);
        break;

    case ST_CALIBRATION:
        // Motor calibration + apply the measured slot offset
        if (dis->motor) {
            printf("[FSM] Calibrating motor...\n");
            stepper_calibrate(dis->motor);

            // Apply the fixed offset between optical index and slot 0
            dis->motor->slot_offset_steps = SLOT_OFFSET_STEPS;
            stepper_apply_slot_offset(dis->motor);
        }
        dis->state = ST_WAIT_FOR_START;
        break;

    case ST_WAIT_FOR_START:
        // Second button press -> start dispensing loop
        wait_start_handler(dis);
        break;

    case ST_DISPENSING: {
        if (dis->pills_left == 0) {
            printf("[FSM] All pills dispensed.\n");
            dis->state = ST_FINISHED;
            break;
        }

        // Time to dispense one pill
        if (time_reached(dis->next_dispense_time)) {
            printf("[FSM] Dispensing one slot... pills_left=%u\n",
                   dis->pills_left);

            // 1) Rotate wheel by one slot
            if (dis->motor) {
                stepper_step_one_slot(dis->motor);
            }

            // 2) Wait within the pre-computed time window for a piezo hit
            bool hit = false;
            if (dis->sensor) {
                hit = pill_sensor_is_ready(dis->sensor);
            }

            if (hit) {
                // Successful dispense: increase pill count and decrease remaining pills
                dis->total_dispense_count++;
                if (dis->pills_left > 0) {
                    dis->pills_left--;
                }
                printf("[FSM] PILL DETECTED. total=%lu, left=%u\n",
                       (unsigned long)dis->total_dispense_count,
                       dis->pills_left);
            } else {
                // No hit within the window: count as a failed dispense
                dis->failed_dispense_count++;
                printf("[FSM] NO PILL detected. failed=%lu\n",
                       (unsigned long)dis->failed_dispense_count);
            }

            // Schedule next dispensing time
            dis->next_dispense_time =
                make_timeout_time_ms(dis->interval_ms);
        }
        break;
    }

    case ST_FINISHED:
        // Simple LED blink pattern to indicate the cycle is finished,
        // then return to the initial wait state.
        for (int i = 0; i < 3; ++i) {
            gpio_put(dis->led_pin, 1);
            sleep_ms(150);
            gpio_put(dis->led_pin, 0);
            sleep_ms(150);
        }
        dis->state      = ST_WAIT_FOR_BUTTON;
        dis->pills_left = 0;   // or reset to default pills_to_dispense if you want
        break;
    }
}