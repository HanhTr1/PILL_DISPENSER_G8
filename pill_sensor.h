//
// Created by HaoKun Tong on 2025/12/1.
//

#ifndef BLINK_PILL_SENSOR_H
#define BLINK_PILL_SENSOR_H


#define PILL_SENSOR_PIN 27
#define PILL_FALL_DISTANCE 0.035f
#define GRAVITY 9.8f
#define PILL_FALLTIME_MARGIN 0.5f
#define MOTOR_STOP_EXTRA_MS    80

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float fall_distance ;
    float gravity;
    float pill_fall_margin;
    uint32_t motor_stop_extra_ms;
    uint32_t pill_fall_time ;
    volatile bool hit_flag;
    bool last_hit;
    uint32_t last_edge_count;
}pillSensorState;

void pill_sensor_init(pillSensorState*ptr);

void pill_sensor_update(pillSensorState*ptr);

bool pill_sensor_is_ready(pillSensorState*ptr);

void pill_sensor_handle_irq(pillSensorState*ptr,uint gpio, uint32_t events);

void pill_sensor_reset(pillSensorState*ptr);






#endif //BLINK_PILL_SENSOR_H
