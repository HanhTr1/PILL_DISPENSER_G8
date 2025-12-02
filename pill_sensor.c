//
// Created by HaoKun Tong on 2025/12/1.
//

#include"pico/stdlib.h"
#include "pico/time.h"
#include"pill_sensor.h"
#include<math.h>
#include <time.h>

void pill_sensor_handle_irq(pillSensorState *ptr, uint gpio, uint32_t events) {
    if (!ptr) return;

    if (gpio == PILL_SENSOR_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        ptr->hit_flag = true;
        ptr->last_edge_count++;
    }
}

void pill_sensor_init(pillSensorState*ptr) {
        gpio_init(PILL_SENSOR_PIN);
        gpio_set_dir(PILL_SENSOR_PIN,false);
        gpio_pull_up(PILL_SENSOR_PIN);
        ptr->fall_distance=PILL_FALL_DISTANCE;
        ptr->gravity=GRAVITY;
        ptr->pill_fall_margin=PILL_FALLTIME_MARGIN;
        ptr->motor_stop_extra_ms=MOTOR_STOP_EXTRA_MS;

        ptr->pill_fall_time=0;
        ptr->last_hit=false;
        ptr->last_edge_count=0;
        pill_sensor_update(ptr);
}


void pill_sensor_update(pillSensorState*ptr) {
        float h=ptr->fall_distance;
        float g=ptr->gravity;
        float t0=sqrtf((2.0f*h)/g);
        float t1=t0*(1.0f+ptr->pill_fall_margin);

        float total=t1+(ptr->motor_stop_extra_ms/1000.0f);
        uint32_t window_ms = (uint32_t)(total * 1000.0f + 0.5f);

        if (window_ms < 150) {
            window_ms = 150;
        }

        ptr->pill_fall_time = window_ms;
}


bool pill_sensor_is_ready(pillSensorState*ptr) {
    ptr->hit_flag=false;
    ptr->last_hit=false;
    ptr->last_edge_count=0;
    sleep_ms(ptr->pill_fall_time);
    ptr->last_hit=ptr->hit_flag;
    return ptr->last_hit;
}
