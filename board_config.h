

#ifndef PILL_DISPENSER_BOARD_CONFIG_H
#define PILL_DISPENSER_BOARD_CONFIG_H

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


#endif //PILL_DISPENSER_BOARD_CONFIG_H