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

//==============================================================================================
// HELPER FUNCTIONS
//==============================================================================================


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

// Log + LoRa helper: add timestamp + (opt) day index
static void log_event(Dispenser* dis, const char* event) {
    char ts[20];
    char line[LOG_STRING_MAX_LEN];

    format_timestamp(ts, sizeof(ts));

    if (dis) {
        // Only show "Day X" AFTER dispensing has started
        bool day_started =
            (dis->state == ST_DISPENSING) ||
            (dis->state == ST_RECOVERY) ||
            (dis->state == ST_FINISHED);

        if (day_started && dis->slot_done > 0) {
            uint8_t day = dis->slot_done;
            if (day > PILL_NUMS) day = PILL_NUMS; // Cap at max
            snprintf(line, sizeof(line), "%s Day %u %s", ts, day, event);
        }
        else {
            snprintf(line, sizeof(line), "%s %s", ts, event);
        }

        // 1) Store in EEPROM log
        write_log(line);

        // 2) Send over LoRaWAN if connected
        if (dis->is_lorawan_connected) {
            send_status_to_lorawan(dis, line);
        }
    }
}

//==============================================================================================
// INITIALIZATION
//==============================================================================================

// move one slot ~ 144 half-steps.
void statemachine_init(Dispenser* dis,
                       Stepper* motor,
                       pillSensorState* sensor,
                       uint8_t pills_to_dispense,
                       uint32_t interval_ms) {
    dispenser_init(dis, SW_0,SW_2, LED_PIN, PIEZO_PIN);

    dis->state = ST_BOOT;
    dis->motor = motor;
    dis->sensor = sensor;
    dis->pills_left = pills_to_dispense;
    dis->interval_ms = interval_ms;

    dis->total_dispense_count = 0;
    dis->failed_dispense_count = 0;
    dis->slot_done = 0;

    // First target time for dispensing
    // dis->next_dispense_time = make_timeout_time_ms(interval_ms);
}

//==============================================================================================
// STATE MACHINE
//==============================================================================================

void statemachine_step(Dispenser* dis) {
    switch (dis->state) {

    //------------------------------------------------------------------------------------------
    // BOOT: Initial system startup
    //------------------------------------------------------------------------------------------

    case ST_BOOT: {
        printf("[FSM] Booting system...\n");
        sleep_ms(3000); //usb enumeration delay
        dis->state = ST_LORA_CONNECT;
        break;
    }

    //------------------------------------------------------------------------------------------
    // LORA_CONNECT: Try to connect to LoraWan
    //------------------------------------------------------------------------------------------

    case ST_LORA_CONNECT: {
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

        //?should we have a st_check_eeprom here? !!!!!!!!!!!
        bool ok = restore_from_eeprom(dis);

        if (!ok) {
            //no valid EEPROM => fresh boot: go to wait for calib
            printf("[FSM] No valid EEPROM data -> fresh boot.\n");
            log_event(dis, "FRESH BOOT");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        // EEPROM restore succeeded
        printf(
            "[FSM] EEPROM restored. state=%d, pills_left=%u,in_motion=%d, calibrated=%d,step_index=%u,slot_done=%u\n",
            dis->state,
            dis->pills_left,
            dis->motor ? dis->motor->in_motion : 0,
            dis->motor ? dis->motor->calibrated : 0, dis->motor->step_index, dis->slot_done);

        bool need_recovery = false;

        // 1. check if we lost power in the middle of a slot
        if (dis->motor && dis->motor->in_motion) {
            // Motor was moving when power lost
            need_recovery = true;
            printf("[FSM] Detected: motor was in motion\n");
        }

        if (need_recovery) {
            // motor was moving & pill hasn't fallen yet
            //we need to re-attempt this slot
            printf("[FSM] -> ST_RECOVERY (will retry current slot)\n");
            log_event(dis, "POWER LOSS DURING MOVEMENT");
            dis->state = ST_RECOVERY;
            break;
        }
        // 2. no recovery needed: check calibration status
        if (!dis->motor || !dis->motor->calibrated) {
            printf("[FSM] Motor not calibrated -> ST_WAIT_CALIBRATION\n");
            log_event(dis, "MOTOR NOT CALIBRATED");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }
        // 3. Motor calibrated and no interrupted motion: ready to wait for dispensing
        if (dis->state == ST_DISPENSING && dis->pills_left > 0) {
            // Resume dispensing - set next dispense time
            dis->next_dispense_time = make_timeout_time_ms(dis->interval_ms);
            printf("[FSM] Resuming dispensing, pills_left=%u\n", dis->pills_left);
            log_event(dis, "RESUME DISPENSING");
            dis->state = ST_DISPENSING;
        }
        else {
            //was waiting or finished
            printf("[FSM] System ready -> ST_WAIT_DISPENSING\n");
            dis->state = ST_WAIT_DISPENSING;
        }
        break;
    }

    //------------------------------------------------------------------------------------------
    // WAIT_CALIBRATION: wait SW_0 pressed while blinking LED
    //------------------------------------------------------------------------------------------

    case ST_WAIT_CALIBRATION:
        send_status_to_lorawan(dis, "WAIT FOR CALIBRATION!");
        wait_calib_button_handler(dis);
        break;

    //------------------------------------------------------------------------------------------
    // CALIBRATION: perform calibration
    //------------------------------------------------------------------------------------------

    case ST_CALIBRATION:
        if (dis->motor) {
            printf("[FSM] Calibrating motor...\n");
            stepper_calibrate(dis->motor, dis);
            dis->motor->slot_offset_steps = SLOT_OFFSET_STEPS;
            stepper_apply_slot_offset(dis->motor);

            if (!dis->motor->calibrated) {
                printf("[FSM] Calibration failed.Back to WAIT_CALIBRATION.\n");
                log_event(dis, "CALIBRATED FAIL");
                dis->state = ST_WAIT_CALIBRATION;
                break;
            }
            dis->motor->in_motion = false;
            dis->motor->calibrated = true;


            save_sm_state(dis);
            log_event(dis, "CALIBRATION DONE");
        }
        dis->state = ST_WAIT_DISPENSING;
        break;

    //------------------------------------------------------------------------------------------
    // WAIT_DISPENSING: wait SW_2 pressed, LED stays on
    //------------------------------------------------------------------------------------------

    case ST_WAIT_DISPENSING:
        send_status_to_lorawan(dis, "WAIT FOR DISPENSING!");
        wait_dispensing_button_handler(dis);
        break;


    //------------------------------------------------------------------------------------------
    // DISPENSING: Main dispensing
    //------------------------------------------------------------------------------------------

    case ST_DISPENSING: {
        if (dis->pills_left == 0) {
            printf("[FSM] Dispensing Finish.\n");
            log_event(dis, "DISPENSING FINISH");
            dis->state = ST_FINISHED;
            break;
        }

        // Time to dispense one pill
        if (time_reached(dis->next_dispense_time)) {
            uint8_t current_slot_attempt = dis->slot_done + 1;

            printf("[FSM] Attempting slot %u (completed=%u, pills_left=%u)\n",
                   current_slot_attempt, dis->slot_done, dis->pills_left);

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
                dis->pills_left--;

                dis->slot_done = current_slot_attempt;

                printf("[FSM] PILL DETECTED. completed_slots=%u, total_pills=%lu, left=%u\n",
                       dis->slot_done, (unsigned long)dis->total_dispense_count,
                       dis->pills_left);
                log_event(dis, "DISPENSE OK");
                //dis->slot_done = dis->total_dispense_count;
            }
            else {
                // No hit within the window: count as a failed dispense
                dis->failed_dispense_count++;
                dis->pills_left--;

                dis->slot_done = current_slot_attempt;

                printf("[FSM] NO PILL. completed_slots=%u, failed=%lu, left=%u\n",
                       dis->slot_done, (unsigned long)dis->failed_dispense_count,
                       dis->pills_left);
                log_event(dis, "DISPENSE FAIL NO PILLS");
                //dis->slot_done = (dis->slot_done + 1) % PILL_NUMS;
                led_blink(dis, 5);
            }
            save_sm_state(dis);
            // Schedule next dispensing time
            dis->next_dispense_time = delayed_by_ms(dis->next_dispense_time, dis->interval_ms);
        }
        break;
    }
    //------------------------------------------------------------------------------------------
    // RECOVERY: recover from power loss
    //------------------------------------------------------------------------------------------

    case ST_RECOVERY: {
        printf("[FSM] Recovery state...\n");

        if (!dis->motor) {
            printf("[FSM] No motor -> ST_WAIT_CALIBRATION\n");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        // If motor was never calibrated, we can't trust the position -> go calibrate.
        if (!dis->motor->calibrated) {
            printf("[FSM] Motor not calibrated, skip recovery.\n");
            dis->state = ST_WAIT_CALIBRATION;
            break;
        }

        printf("[FSM] Recovering: %u slots completed, will retry slot %u\n",
               dis->slot_done, dis->slot_done + 1);
        // rewind partial slot and recalibrate (inside stepper_recovery)
        stepper_recovery(dis->motor, dis);

        printf("[FSM] Recovery done. At end of slot %u, will retry slot %u\n",
               dis->slot_done, dis->slot_done + 1);
        log_event(dis, "RECOVERY DONE");

        if (dis->pills_left > 0) {
            // Resume dispensing from current position
            dis->next_dispense_time = make_timeout_time_ms(dis->interval_ms);
            dis->state = ST_DISPENSING;
            printf("[FSM] Resuming dispensing...\n");
        }
        else {
            // No pills left
            dis->state = ST_FINISHED;
        }
        break;
    }
    //------------------------------------------------------------------------------------------
    // FINISHED: blink LED 5 times, reset for next cycle
    //------------------------------------------------------------------------------------------

    case ST_FINISHED:
        led_blink(dis, 3);
        log_event(dis, "CYCLE COMPLETE");


        // Reset for next cycle
        dis->motor->calibrated = false;
        dis->slot_done = 0; //reset slot counter
        dis->pills_left = PILL_NUMS;
        dis->total_dispense_count = 0;
        dis->failed_dispense_count = 0;

        save_sm_state(dis);
        dis->state = ST_WAIT_CALIBRATION;
        break;
    }
}
