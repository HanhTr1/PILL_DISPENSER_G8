// statemachine.c
#include "statemachine.h"
#include "dispenser_initialize.h"
#include "button_handler.h"
#include"stepper.h"
#include <stdio.h>
#include"board_config.h"
#include "eeprom.h"
#include "lorawan.h"
#include "hardware/rtc.h"

//restore data from eeprom
bool restore_from_eeprom(Dispenser* dis) {
    if (!dis) return false;
    if (!dis->motor) {
        printf("[FSM] No motor attached when restoring.\n");
        return false;
    }
    simple_state_t s;
    if (load_state(&s) != 0) {
        printf("[FSM] No valid EEPROM state (load_state failed).\n");
        return false;
    }
    if ((uint8_t)~s.not_state != s.state ||
        (uint8_t)~s.not_pills_left != s.pills_left) {
        printf("[FSM] EEPROM state integrity check failed.\n");
        return false;
    }
    dis->state = (DispenserState)s.state;
    dis->pills_left = s.pills_left;
    dis->slot_done = s.slot_done;
    if (dis->motor) {
        dis->motor->current_steps_slot = s.current_steps_slot;
        dis->motor->in_motion = (s.in_motion != 0);
        dis->motor->calibrated = (s.calibrated != 0);
        dis->motor->step_index = s.step_index;
        dis->motor->slot_offset_steps = SLOT_OFFSET_STEPS;
    }
    printf(
        "[FSM] Restored from EEPROM: state=%u, pills_left=%u, steps=%u, in_motion=%u,calibrate=%u,step_index=%u,slot_done=%u\n",
        s.state, s.pills_left, s.current_steps_slot, s.in_motion, s.calibrated, s.step_index, s.slot_done);
    return true;
}

// Format current RTC time as "YYYY-MM-DD HH:MM:SS"
static void format_timestamp(char* buf, size_t len) {
    datetime_t t;
    rtc_get_datetime(&t);

    // Example: "2025-12-07 13:42:05"
    snprintf(buf, len,
             "%04d-%02d-%02d %02d:%02d:%02d",
             t.year, t.month, t.day,
             t.hour, t.min, t.sec);
}

// Log + LoRa helper: add timestamp + (optional) day index
static void log_event(Dispenser* dis, const char* event) {
    char ts[20]; // "YYYY-MM-DD HH:MM:SS" -> 19 + '\0'
    char line[LOG_STRING_MAX_LEN]; // final log string to store in EEPROM

    format_timestamp(ts, sizeof(ts));

    if (dis) {
        // If pills are dispensed once per day: day index = PILL_NUMS - pills_left + 1
        uint8_t day = 0;
        if (PILL_NUMS >= dis->pills_left) {
            day = (uint8_t)(PILL_NUMS - dis->pills_left + 1);
        }

        // Example: "2025-12-07 13:42:05 Day 3 DISPENSE_OK"
        snprintf(line, sizeof(line), "%s Day %u %s", ts, day, event);
    }
    else {
        // No dispenser context (e.g. boot before state restored)
        snprintf(line, sizeof(line), "%s %s", ts, event);
    }

    // 1) Store in EEPROM log
    write_log(line);

    // 2) Send over LoRaWAN if connected
    if (dis && dis->is_lorawan_connected) {
        send_status_to_lorawan(dis, line);
    }
}

// Half-step offset between optical index and the first pill slot.
// You measured that one slot ≈ 144 half-steps.
void statemachine_init(Dispenser* dis,
                       Stepper* motor,
                       pillSensorState* sensor,
                       uint8_t pills_to_dispense,
                       uint32_t interval_ms) {
    // Use existing helper to configure button / LED / piezo pins
    dispenser_init(dis, SW_0,SW_2, LED_PIN, PIEZO_PIN);

    //set initial stage here!
    dis->state = ST_BOOT;

    // Attach modules
    dis->motor = motor;
    dis->sensor = sensor;

    // High-level logic parameters
    dis->pills_left = pills_to_dispense;
    dis->interval_ms = interval_ms;

    // Statistics
    dis->total_dispense_count = 0;
    dis->failed_dispense_count = 0;

    // First target time for dispensing
    // dis->next_dispense_time = make_timeout_time_ms(interval_ms);
}

void statemachine_step(Dispenser* dis) {
    switch (dis->state) {
    case ST_BOOT: {
        printf("[FSM] Booting system...\n");
        printf("[FSM] Debug pause: plug USB & open serial now...\n");
        sleep_ms(3000);

        // ALWAYS go to LoRa connect after boot
        printf("[FSM] Boot complete -> ST_LORA_CONNECT\n");
        dis->state = ST_LORA_CONNECT;
        break;
    }

    case ST_LORA_CONNECT:
        printf("[FSM] Connecting to LoRaWAN...\n");
        lorawan_init();

        bool lora_connected = handle_lorawan();

        if (lora_connected) {
            printf("[FSM] LORA connection is done!!!\n");
            lorawan_send_message("Group 8 LoraWan Connected!");
            dis->is_lorawan_connected = true;
            log_event(dis, "BOOT DONE LORA OK");
        }
        else {
            printf("Can't connect to LoRaWan. Continue to run without!\n");
            dis->is_lorawan_connected = false;
            log_event(dis, "BOOT DONE LORA FAIL");
        }

        //send_status_to_lorawan(dis, "BOOT_DONE & LORAWAN_CONNECTED!");

        // Wait here until SW_0 (START button) is pressed
        //         while (gpio_get(SW_0)) {
        // // Optionally print some debug info every 200 ms if needed
        //          sleep_ms(200);
        //         }

        // NOW CHECK EEPROM
        bool ok = restore_from_eeprom(dis);

        if (!ok) {
            // No valid EEPROM => fresh boot: go to calibration
            printf("[FSM] No valid EEPROM data -> fresh boot.\n");
            log_event(dis, "NO VALID EEPROM DATA");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        // EEPROM restore succeeded - check what we need to do
        printf("[FSM] EEPROM restored. state=%d, pills_left=%u, steps=%u\n",
               dis->state, dis->pills_left,
               dis->motor ? dis->motor->current_steps_slot : 0);

        // 1) Check if we lost power mid-slot
        if (dis->motor &&
            dis->motor->in_motion &&
            dis->motor->current_steps_slot > 0) {
            printf("[FSM] Mid-slot power loss -> ST_RECOVERY\n");
            log_event(dis, "POWER LOSS DURING TURNING");
            dis->state = ST_RECOVERY;
            break;
            }

        // 2) Not mid-slot: check calibration
        if (!dis->motor || !dis->motor->calibrated) {
            printf("[FSM] Motor not calibrated -> ST_WAIT_CALIBRATION\n");
            log_event(dis, "MOTOR NOT CALIBRATED");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        // 3) Motor is calibrated - check if we were dispensing
        if (dis->state == ST_DISPENSING) {
            printf("[FSM] Power loss during dispensing -> resume ST_DISPENSING\n");
            log_event(dis, "POWER LOSS RESUME DISPENSING");
            dis->state = ST_DISPENSING;
            break;
        }

        // 4) Calibrated and idle (or was in WAIT states)
        printf("[FSM] Calibrated and idle -> ST_WAIT_DISPENSING\n");
        dis->state = ST_WAIT_DISPENSING;

        break;


    case ST_WAIT_CALIBRATION:
        // First button press -> go to calibration state
        send_status_to_lorawan(dis, "WAIT FOR CALIBRATION!");
        wait_calib_button_handler(dis);
        break;

    case ST_CALIBRATION:
        if (dis->motor) {
            printf("[FSM] Calibrating motor...\n");
            stepper_calibrate(dis->motor, dis);
            dis->motor->slot_offset_steps = SLOT_OFFSET_STEPS;
            stepper_apply_slot_offset(dis->motor);

            if (!dis->motor->calibrated) {
                printf("[FSM] Calibration failed. Back to WAIT_CALIBRATION.\n");
                log_event(dis, "CALIBRATED FAIL");
                dis->state = ST_WAIT_CALIBRATION;
                break;
            }

            dis->motor->current_steps_slot = 0;
            dis->motor->in_motion = false;
            dis->motor->calibrated = true;


            save_sm_state(dis);
            log_event(dis, "CALIBRATION DONE");
        }
        dis->state = ST_WAIT_DISPENSING;
        break;

    case ST_WAIT_DISPENSING:
        // Second button press -> start dispensing loop
        send_status_to_lorawan(dis, "WAIT FOR DISPENSING!");
        wait_dispensing_button_handler(dis);
        break;

    case ST_DISPENSING: {
        if (dis->pills_left == 0) {
            printf("[FSM] Dispensing Finish.\n");
            log_event(dis, "DISPENSING FINISH");
            dis->state = ST_FINISHED;
            break;
        }

        // Time to dispense one pill
        if (time_reached(dis->next_dispense_time)) {
            printf("[FSM] Dispensing one slot... pills_left=%u\n",
                   dis->pills_left);

            // 1) Rotate wheel by one slot
            if (dis->motor) {
                stepper_step_one_slot(dis->motor, dis);
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
                log_event(dis, "DISPENSE OK");
                dis->slot_done = (dis->slot_done + 1) % PILL_NUMS;
                save_sm_state(dis);
            }
            else {
                // No hit within the window: count as a failed dispense
                dis->failed_dispense_count++;
                printf("[FSM] NO PILL detected. failed=%lu\n",
                       (unsigned long)dis->failed_dispense_count);
                log_event(dis, "DISPENSE FAIL NO PILLS");
                dis->pills_left--;
                dis->slot_done = (dis->slot_done + 1) % PILL_NUMS;
                save_sm_state(dis);
                led_blink(dis, 5);
            }

            // Schedule next dispensing time
            dis->next_dispense_time = delayed_by_ms(dis->next_dispense_time, dis->interval_ms);
        }
        break;
    }

    case ST_RECOVERY: {
        printf("[FSM] Recovery state...\n");

        if (!dis->motor || !dis->motor->calibrated) {
            printf("[FSM] No motor or not calibrated, go to calibration.\n");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        printf("current steps %u", dis->motor->current_steps_slot);
        // 1) rewind partial slot and recalibrate (inside stepper_recovery)
        stepper_recovery(dis->motor, dis);
        // 2) After recovery, DO NOT dispense pills, DO NOT check pill_sensor.
        //    Just decide where to go next.

        if (dis->pills_left > 0) {
            // We still have pills -> go back to "ready to start dispensing".
            dis->next_dispense_time = make_timeout_time_ms(dis->interval_ms);
            printf("[FSM] Recovery done, back to WAIT_DISPENSING.\n");
            log_event(dis, "RECOVERY DONE");
            dis->state = ST_DISPENSING;
        }
        else {
            // No pills left -> finished.
            printf("[FSM] Recovery done, all pills dispensed.\n");
            dis->state = ST_FINISHED;
        }

        break;
    }

    case ST_FINISHED:
        // Simple LED blink pattern to indicate the cycle is finished,
        // then return to the initial wait state.
        led_blink(dis, 3);
        log_event(dis, "CYCLE FINISHED RESET");
        dis->state = ST_WAIT_CALIBRATION;
        dis->pills_left = PILL_NUMS; // or reset to default pills_to_dispense if you want
        break;
    }
}
