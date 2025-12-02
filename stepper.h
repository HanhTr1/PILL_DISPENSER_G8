//
// Created by HaoKun Tong on 2025/12/2.
//

#ifndef BLINK_STEPPER_H
#define BLINK_STEPPER_H


#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint32_t pins[4];
    uint32_t sensor_pin;
    int  step_index;
    int  steps_per_rev;
    bool calibrated;
    volatile bool index_hit;
    int  slot_offset_steps;
} Stepper;


void stepper_init(Stepper *ptr);


void stepper_calibrate(Stepper *ptr);


void stepper_step_one_slot(Stepper *ptr);

void stepper_apply_slot_offset(Stepper *ptr);
#endif //BLINK_STEPPER_H