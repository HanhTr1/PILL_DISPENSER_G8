#include "button_handler.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"


void wait_calib_button_handler(Dispenser* dis) {

    while (dis->state == ST_WAIT_CALIBRATION) {
        static uint64_t last_blink = 0;
        static bool led_state = false;

        uint64_t now = time_us_64();
        if (now - last_blink > LED_BLINK_US) {
            led_state = !led_state;
            gpio_put(dis->led_pin, led_state);
            last_blink = now;
        }
        if (gpio_get(dis->button_pin) == 0) {
            printf("Button pressed. Start calibration...\n");

            gpio_put(dis->led_pin, 0);
            dis->state = ST_CALIBRATION;

            while (gpio_get(dis->button_pin) == 0) {
                sleep_ms(BUTTON_DEBOUNCE_MS);
            }
        }
    }
}

void wait_dispensing_button_handler(Dispenser* dis) {

    while (dis->state == ST_WAIT_DISPENSING) {
        gpio_put(dis->led_pin, 1);

        if (gpio_get(dis->button_pin2) == 0) {
            printf("Button pressed. Start dispensing...\n");
            dis->next_dispense_time = make_timeout_time_ms(dis->interval_ms);
            dis->state = ST_DISPENSING;
            printf("[FSM] START pressed -> enter ST_DISPENSING, first after %u ms\n",
                   dis->interval_ms);
            gpio_put(dis->led_pin, 0);
            dis->state = ST_DISPENSING;

            while (gpio_get(dis->button_pin2) == 0) {
                sleep_ms(BUTTON_DEBOUNCE_MS);
            }
        }
    }
}

void led_blink(Dispenser*dis,int time) {
    for (int i=0;i<=time;i++) {
        gpio_put(dis->led_pin, 1);
        sleep_ms(150);
        gpio_put(dis->led_pin, 0);
        sleep_ms(150);
    }
}