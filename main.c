#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "stepper.h"
#include "pill_sensor.h"

// Global states
static Stepper        g_stepper;
static pillSensorState g_sensor;

// Global GPIO IRQ callback (only ONE for RP2040)
static void global_gpio_irq(uint gpio, uint32_t events) {
    // 1) stepper optical index
    if (gpio == g_stepper.sensor_pin && (events & GPIO_IRQ_EDGE_FALL)) {
        g_stepper.index_hit = true;
    }

    // 2) pill sensor hit
    pill_sensor_handle_irq(&g_sensor, gpio, events);
}

int main(void) {
    stdio_init_all();

    // ---- init stepper ----
    g_stepper.pins[0]   = 2;
    g_stepper.pins[1]   = 3;
    g_stepper.pins[2]   = 6;
    g_stepper.pins[3]   = 13;
    g_stepper.sensor_pin = 28;   // OPTO fork pin

    stepper_init(&g_stepper);

    // ---- init pill sensor ----
    pill_sensor_init(&g_sensor);
    printf("Pill sensor init done.\n");
    printf("Computed detect window = %u ms\n", g_sensor.pill_fall_time);

    // ---- register ONE global GPIO IRQ callback ----
    gpio_set_irq_enabled_with_callback(
        g_stepper.sensor_pin,
        GPIO_IRQ_EDGE_FALL,
        true,
        global_gpio_irq
    );
    gpio_set_irq_enabled(
        PILL_SENSOR_PIN,
        GPIO_IRQ_EDGE_FALL,
        true
    );

    // ---- calibrate stepper once ----
    stepper_calibrate(&g_stepper);
    g_stepper.slot_offset_steps = 144;
    stepper_apply_slot_offset(&g_stepper);

    while (true) {
        printf("\n=== New dispense simulation ===\n");
        printf("Rotate 1 slot and wait for pill hit within %u ms...\n",
               g_sensor.pill_fall_time);

        stepper_step_one_slot(&g_stepper);

        bool hit = pill_sensor_is_ready(&g_sensor);

        if (hit) {
            printf("PILL DETECTED! edge_count = %u\n", g_sensor.last_edge_count);
        } else {
            printf("NO PILL detected in window.\n");
        }

        sleep_ms(1000);
    }

    return 0;
}