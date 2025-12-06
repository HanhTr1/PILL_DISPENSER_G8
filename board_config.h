#ifndef PILL_DISPENSER_BOARD_CONFIG_H
#define PILL_DISPENSER_BOARD_CONFIG_H
#include<pico/types.h>
#include"pill_sensor.h"
#include "pico/stdlib.h"

//pin
#define SW_0 9
#define SW_2 7

#define LED_PIN 20
#define OPTO_FORK_PIN 28
#define PIEZO_PIN 27

//delay
#define LED_BLINK_US 500000
#define BUTTON_DEBOUNCE_MS 20
#define SLOT_OFFSET_STEPS 144
#define HALF_STEPS 512

//pill
#define PILL_TIME 30000
#define PILL_NUMS 7
//uart
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BAUD_RATE 9600

//lora
#define LORA_JOIN_INTERVAL_MS 5000
#define LORA_JOIN_MAX_ATTEMPTS 5
#define  LORA_RESPONSE_LEN 128


typedef enum {
    ST_BOOT,
    ST_LORA_CONNECT,
    ST_RECOVERY,
    ST_WAIT_CALIBRATION,         //wait button, blink led
    ST_CALIBRATION,             //calib
    ST_WAIT_DISPENSING,          //wait button, led stays on
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
    //for recovery
    bool in_motion;
    uint16_t current_steps_slot;
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

    bool is_lorawan_connected;
} Dispenser;

#endif //PILL_DISPENSER_BOARD_CONFIG_H