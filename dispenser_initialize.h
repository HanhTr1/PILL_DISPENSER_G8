#ifndef PILL_DISPENSER_DISPENSER_INITIALIZE_H
#define PILL_DISPENSER_DISPENSER_INITIALIZE_H

#include  "board_config.h"
#include "pico/stdlib.h"

void dispenser_init(Dispenser* dis, uint button, uint led, uint piezo);

#endif //PILL_DISPENSER_DISPENSER_INITIALIZE_H