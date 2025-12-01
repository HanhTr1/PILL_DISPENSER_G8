#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

//=======================================
//DEF
//=======================================


//pin
#define BUTTON_PIN 7
#define LED_PIN 20
#define OPTO_FORK_PIN 28
#define PIEZO_PIN 27

//delay
#define LED_BLINK_US 500000
#define BUTTON_DEBOUNCE_MS 20

typedef enum {
    ST_WAIT_FOR_BUTTON,         //wait button, blink led
    ST_CALIBRATION,             //calib
    ST_WAIT_FOR_START,          //wait button, led stays on
    ST_DISPENSING,              //dispense pills every 30s
    ST_FINISHED                 //finished
} DispenserState;

typedef struct {
    uint pins[4];
    uint sensor_pin;
    int  step_index;
    int  steps_per_rev;            // Half-steps per full revolution
    bool calibrated;
    volatile bool index_hit;       // Set by IRQ when index gap is detected
} Stepper;

typedef struct {
    DispenserState state;
    uint button_pin;
    uint led_pin;
    uint piezo_pin;
} Dispenser;

void dispenser_init(Dispenser* dis, uint button, uint led, uint piezo) {
    dis->state = ST_WAIT_FOR_BUTTON;
    dis->button_pin = button;
    dis->led_pin = led;
    dis->piezo_pin = piezo;

    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);

    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, 0);

    gpio_init(piezo);
    gpio_set_dir(piezo, GPIO_IN);
    gpio_pull_up(piezo);
}

void wait_button_handler(Dispenser* dis) {
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

void wait_start_handler(Dispenser* dis) {
    gpio_put(dis->led_pin, 0);

    if (gpio_get(dis->button_pin) == 0) {
        printf("Button pressed. Start dispensing...\n");

        gpio_put(dis->led_pin, 0);
        dis->state = ST_DISPENSING;

        while (gpio_get(dis->button_pin) == 0) {
            sleep_ms(BUTTON_DEBOUNCE_MS);
        }
    }

}

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