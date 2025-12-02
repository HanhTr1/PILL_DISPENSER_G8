#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "board_config.h"
#include "button_handler.h"
#include "dispenser_initialize.h"

int main() {
    stdio_init_all();

    Stepper motor = {
        .pins       = {2, 3, 6, 13},
        .sensor_pin = OPTO_FORK_PIN,
        .step_index = 0,
        .steps_per_rev = 0,
        .calibrated = false,
        .index_hit  = false
    };

    Dispenser dispenser;
    dispenser_init(&dispenser, BUTTON_PIN, LED_PIN, PIEZO_PIN);

    printf("Press button to calibrate!\n");

    while (true) {
        switch (dispenser.state) {
        case ST_WAIT_FOR_BUTTON:
            wait_button_handler(&dispenser);
            break;
        case ST_CALIBRATION:
            break;
        case ST_WAIT_FOR_START:
            wait_start_handler(&dispenser);
            break;
        case ST_DISPENSING:
            break;
        case ST_FINISHED:
            break;

        }
    }

}