#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include <string.h>

#define BUTTON_PIN 7
#define LED_PIN 20
#define OPTO_FORK_PIN 28
#define PIEZO_PIN 27
#define IN1 2
#define IN2 3
#define IN3 6
#define IN4 13

#define LED2_PIN 20
#define LED_BLINK_US 500000
#define BUTTON_DEBOUNCE_MS 20
#define DELAY_TIME_MS 2
#define STEPS 8
#define CALIB_TIMES 3
#define RUN_DEFAULT 8
#define TIME_CYCLE 300000
typedef enum States {
    ST_WAIT_FOR_BUTTON,
    ST_CALIBRATION,
    ST_WAIT_FOR_START,
    ST_DISPENSING,
    ST_FINISHED,
}DispenserState;
typedef struct {
    uint pins[4];
    uint opto_fork_pin;
    uint piezo_sensor;
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

const int driving_sequence[STEPS][4] = {
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
    {1,0,0,1}
};

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

void calibrate(Stepper* motor) {
    printf("Calibrating...\n");
    motor->step_index = 0;

    while (!motor->index_hit) {
        for (int i = 0; i < 4; i++) {
            gpio_put(motor->pins[i], driving_sequence[motor->step_index][i]);
        }
        motor->step_index++;
        if (motor->step_index >= STEPS) {
            motor->step_index = 0;
        }

        sleep_ms(DELAY_TIME_MS);

        // check opto fork
        if (gpio_get(motor->opto_fork_pin) == 0) {
            motor->index_hit = true;
            printf("Index detected. Calibrate done!\n");
        }
    }

    for (int i = 0; i < 4; i++) gpio_put(motor->pins[i], 0); //turn off
    motor->calibrated = true;
}
/*
bool button_pressed(uint gpio) {
    return gpio_get(gpio) == 0;
}
bool ledOn(uint gpio) {
    return gpio_get(gpio) == 1;
}
void blink() {
    gpio_put(LED2_PIN, true);
    sleep_ms(1000);
    gpio_put(LED2_PIN, false);
    sleep_ms(1000);

}
*/

void dispense( Stepper* motor, Dispenser* dis) {
    //every 30s

}
int main() {
    stdio_init_all();

    Stepper motor = {
        .pins       = {2, 3, 6, 13},
        .opto_fork_pin = OPTO_FORK_PIN,
        .piezo_sensor = PIEZO_PIN,
        .step_index = 0,
        .steps_per_rev = 0,
        .calibrated = false,
        .index_hit  = false
    };
    Dispenser dispenser;
    dispenser_init(&dispenser, BUTTON_PIN, LED_PIN, PIEZO_PIN);

    printf("Press button to calibrate!\n");
    DispenserState current_state = ST_WAIT_FOR_BUTTON;

    while (true) {
        switch (dispenser.state) {
            case ST_WAIT_FOR_BUTTON:
                wait_button_handler(&dispenser);
                break;
            case ST_CALIBRATION:
                calibrate(&motor);
                dispenser.state = ST_WAIT_FOR_START;
                break;
            case ST_WAIT_FOR_START:
                wait_start_handler(&dispenser);
                break;
            case ST_DISPENSING:
                dispense(&motor, &dispenser);
                sleep_ms(TIME_CYCLE);
                break;
            case ST_FINISHED:
                printf("All pills dispensed. Restarting...\n");
                motor.calibrated = false;
                motor.index_hit = false;
                dispenser.state = ST_WAIT_FOR_BUTTON;
                break;
        }
    }



}