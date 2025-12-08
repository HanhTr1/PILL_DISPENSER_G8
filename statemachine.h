// statemachine.h
#ifndef PILL_DISPENSER_STATEMACHINE_H
#define PILL_DISPENSER_STATEMACHINE_H

#include <stdint.h>
#include <stdbool.h>

#include "board_config.h"   // provides Dispenser / DispenserState definitions
#include "pill_sensor.h"    // pill sensor structure and API

bool restore_from_eeprom(Dispenser *dis);
// Initialize the finite-state machine.
// Connects the stepper motor, pill sensor, and dispenser logic.
void statemachine_init(Dispenser *dis,
                       Stepper *motor,
                       pillSensorState *sensor,
                       uint8_t pills_to_dispense,
                       uint32_t interval_ms);

// Run one FSM cycle; call repeatedly inside while(1)
void statemachine_step(Dispenser *dis);



#endif // PILL_DISPENSER_STATEMACHINE_H