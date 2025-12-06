#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "eeprom.h"
#include "board_config.h"   // BUTTON_PIN, LED_PIN, OPTO_FORK_PIN, PIEZO_PIN
#include "stepper.h"
#include "pill_sensor.h"
#include "statemachine.h"

// Global module instances
static Stepper         g_stepper;
static pillSensorState g_sensor;
static Dispenser       g_dispenser;

// Single global GPIO IRQ callback for RP2040
static void global_gpio_irq(uint gpio, uint32_t events) {
    // Stepper index sensor (optical fork)
    if (gpio == g_stepper.sensor_pin && (events & GPIO_IRQ_EDGE_FALL)) {
        g_stepper.index_hit = true;
    }

    // Pill hit sensor (piezo)
    pill_sensor_handle_irq(&g_sensor, gpio, events);
}

int main(void) {
    stdio_init_all();
    setup_i2c();
    //---for debug---
    // erase_log();
    // uint8_t zero=0;
    // eeprom_write(STATE_ADDR,&zero,1);
    // -------- Stepper initialization --------
    g_stepper.pins[0]    = 2;
    g_stepper.pins[1]    = 3;
    g_stepper.pins[2]    = 6;
    g_stepper.pins[3]    = 13;
    g_stepper.sensor_pin = OPTO_FORK_PIN;

    stepper_init(&g_stepper);

    // -------- Pill sensor initialization --------
    pill_sensor_init(&g_sensor);
    printf("Pill sensor initialized. Detection window = %u ms\n",
           g_sensor.pill_fall_time);

    // -------- GPIO IRQ registration (one global callback) --------
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

    // -------- State machine initialization --------
    // Example: dispense 7 pills, one pill every 30 seconds
    statemachine_init(&g_dispenser,
                      &g_stepper,
                      &g_sensor,
                      PILL_NUMS,        // pills_to_dispense
                      PILL_TIME);   // interval_ms

    printf("System ready. Press button to start.\n");

    // -------- Main loop --------
    while (true) {
        // Drive high-level state machine
        statemachine_step(&g_dispenser);
    }

    return 0;
}
