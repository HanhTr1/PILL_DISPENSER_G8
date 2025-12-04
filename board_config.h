#ifndef PILL_DISPENSER_BOARD_CONFIG_H
#define PILL_DISPENSER_BOARD_CONFIG_H
#include<pico/types.h>
#include"pill_sensor.h"

//pin
#define BUTTON_PIN 7
#define SW_0 9
#define SW_2 7

#define LED_PIN 20
#define OPTO_FORK_PIN 28
#define PIEZO_PIN 27

//delay
#define LED_BLINK_US 500000
#define BUTTON_DEBOUNCE_MS 20
#define SLOT_OFFSET_STEPS 144

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
    int  steps_per_rev;
    bool calibrated;
    volatile bool index_hit;
    int  slot_offset_steps;
} Stepper;

typedef struct {
    DispenserState state;
    uint button_pin;
    uint button_pin2;
    uint led_pin;
    uint piezo_pin;
    Stepper         *motor;
    pillSensorState *sensor;
    uint pills_left;
    uint interval_ms;
    uint total_dispense_count;
    uint failed_dispense_count;
    absolute_time_t next_dispense_time;

} Dispenser;


#endif //PILL_DISPENSER_BOARD_CONFIG_H