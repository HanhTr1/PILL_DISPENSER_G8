#include "dispenser_initialize.h"

void dispenser_init(Dispenser* dis, uint button,uint button2,uint led, uint piezo) {
    dis->state = ST_WAIT_CALIBRATION;
    dis->button_pin = button;
    dis->button_pin2 = button2;
    dis->led_pin = led;
    dis->piezo_pin = piezo;

    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);

    gpio_init(button2);
    gpio_set_dir(button2, GPIO_IN);

    gpio_pull_up(button2);
    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, 0);

    gpio_init(piezo);
    gpio_set_dir(piezo, GPIO_IN);
    gpio_pull_up(piezo);
}