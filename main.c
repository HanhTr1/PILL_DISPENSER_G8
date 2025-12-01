#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdlib.h>

#define PIN_OPTO 28
#define PIN_PIEZO 27
#define IN1 2
#define IN2 3
#define IN3 6
#define IN4 13
#define BUTTON 7

#define LED2_PIN 21

#define DELAY_TIME_MS 2
#define STEPS 8
#define CALIB_TIMES 3
#define RUN_DEFAULT 8
typedef struct {
    uint pins[4];
    uint opto_fork_pin;
    uint piezo_sensor;
    int  step_index;
    int  steps_per_rev;            // Half-steps per full revolution
    bool calibrated;
    volatile bool index_hit;       // Set by IRQ when index gap is detected
} Stepper;
enum States {
    WAITING_FOR_BUTTON,
    CALIBRATION,
    DISPENSING
};
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

void blink() {
    gpio_put(LED2_PIN, true);
    sleep_ms(1000);
    gpio_put(LED2_PIN, false);
    sleep_ms(1000);

}
void calibrate() {
    //wheel 1 full turn and stop when align

}
bool button_pressed(uint gpio) {
    return gpio_get(gpio) == 0;
}
bool ledOn(uint gpio) {
    return gpio_get(gpio) == 1;
}

void dispense() {
    //every 30s

}
int main() {
    stdio_init_all();

    gpio_init(PIN_OPTO);
    gpio_init(PIN_PIEZO);
    gpio_init(BUTTON);
    gpio_set_dir(BUTTON, GPIO_IN);
    gpio_init(LED2_PIN);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    Stepper motor = {
        .pins       = {2, 3, 6, 13},
        .opto_fork_pin = PIN_OPTO,
        .piezo_sensor = PIN_PIEZO,
        .step_index = 0,
        .steps_per_rev = 0,
        .calibrated = false,
        .index_hit  = false
    };
    enum States current_state = WAITING_FOR_BUTTON;

    while (true) {

       switch (current_state) {
           case WAITING_FOR_BUTTON:
               calibrate();
               blink();
               current_state = CALIBRATION;
               break;
           case CALIBRATION:
               calibrate();
               ledOn(LED2_PIN);
               current_state = DISPENSING;
           case DISPENSING:
               dispense();
           default: ;
       }

    }


}