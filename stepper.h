
#ifndef BLINK_STEPPER_H
#define BLINK_STEPPER_H
#include"board_config.h"

void stepper_init(Stepper *ptr);

void stepper_calibrate(Stepper *ptr,Dispenser *dis);

void stepper_step_one_slot(Stepper *ptr,Dispenser *dis);

void stepper_apply_slot_offset(Stepper *ptr);

void stepper_recovery(Stepper *ptr,Dispenser *dis);
#endif //BLINK_STEPPER_H
