// statemachine.c
#include "statemachine.h"
#include "dispenser_initialize.h"
#include "button_handler.h"
#include"stepper.h"
#include <stdio.h>
#include"board_config.h"
#include "lorawan.h"
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

    //set initial state here!
    dis->state = ST_BOOT;

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
    case ST_BOOT: // i havent write anything yet in this state, just for it to move to lora to test
        printf("[FSM] Booting system...\n");
        dis->state = ST_LORA_CONNECT;
        break;

    case ST_LORA_CONNECT:
        printf("[FSM] Connecting to LoRaWAN...\n");
        lorawan_init();

        bool lora_connected = handle_lorawan();

        if (lora_connected) {
            printf("[FSM] LORA connection is done!!!\n");
            lorawan_send_message("Group 8 LoraWan Connected!");
            dis->is_lorawan_connected = true;
        }
        else {
            printf("Can't connect to LoRaWan. Continue to run without!\n");
            dis->is_lorawan_connected = false;
        }

        send_status_to_lorawan(dis, "BOOT_DONE & LORAWAN_CONNECTED!");
        dis->state = ST_WAIT_CALIBRATION;

        break;

    case ST_WAIT_CALIBRATION:
        // First button press -> go to calibration state
        wait_calib_button_handler(dis); //I CHANGED THE NAME OF THE FUNCTION so it's easier to understand
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
        send_status_to_lorawan(dis, "CALIBRATION_DONE!");
        dis->state = ST_WAIT_DISPENSING;
        break;

    case ST_WAIT_DISPENSING:
        // Second button press -> start dispensing loop
        wait_dispensing_button_handler(dis);   //I CHANGED THE NAME OF THE FUNCTION so it's easier to understand
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

    case ST_RECOVERY:
        break;

    case ST_FINISHED:
        // Simple LED blink pattern to indicate the cycle is finished,
        // then return to the initial wait state.
        for (int i = 0; i < 3; ++i) {
            gpio_put(dis->led_pin, 1);
            sleep_ms(150);
            gpio_put(dis->led_pin, 0);
            sleep_ms(150);
        }
        dis->state      = ST_WAIT_CALIBRATION;
        dis->pills_left = 0;   // or reset to default pills_to_dispense if you want
        break;
    }
}