#ifndef PILL_DISPENSER_BUTTON_HANDLER_H
#define PILL_DISPENSER_BUTTON_HANDLER_H

#include  "board_config.h"

void wait_calib_button_handler(Dispenser* dis);

void wait_dispensing_button_handler(Dispenser* dis);

void led_blink(Dispenser*dis,int time);
#endif //PILL_DISPENSER_BUTTON_HANDLER_H