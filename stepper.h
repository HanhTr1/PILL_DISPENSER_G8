//
// Created by HaoKun Tong on 2025/12/2.
//

#ifndef BLINK_STEPPER_H
#define BLINK_STEPPER_H
#include"board_config.h"

void stepper_init(Stepper *ptr);


void stepper_calibrate(Stepper *ptr);


void stepper_step_one_slot(Stepper *ptr);

void stepper_apply_slot_offset(Stepper *ptr);
#endif //BLINK_STEPPER_H