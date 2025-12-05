//
// Created by truon on 05/12/2025.
//

#ifndef PILL_DISPENSER_5_EEPROM_H
#define PILL_DISPENSER_5_EEPROM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "hardware/i2c.h"
#include "board_config.h"

#define I2C_PORT i2c0
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17

#define I2C_BAUDRATE 100000
#define EEPROM_I2C_ADDR 0x50
#define EEPROM_STORE_ADDR (0x7fff-2)    // highest address avoid overflow
#define EEPROM_TOTAL_BYTES 32768u

#define LOG_START_ADDR 0
#define LOG_ENTRY_SIZE 64
#define LOG_AREA_SIZE 2048
#define LOG_MAX_ENTRIES 32
#define COMMAND_SIZE 8
#define LOG_STRING_MAX_LEN 61

#define STATE_ADDR 0X0800

typedef struct {
    uint8_t state;       // FSM state
    uint8_t not_state;   // ~state
    uint8_t pills_left;
    uint8_t not_pills_left;// ~pills_left
    uint16_t current_steps_slot;
    uint8_t in_motion;
    uint8_t calibrated;
    uint8_t not_calibrated;
} simple_state_t;


int eeprom_write(uint16_t addr, uint8_t *data, size_t len);

int eeprom_read(uint16_t addr, uint8_t *data, size_t len);
void setup_i2c(void);

uint16_t crc16(const uint8_t *data_p, size_t length);
bool eeprom_available();
int find_log();
void erase_log() ;
void write_log( char *msg);
void read_log();
int save_state(simple_state_t *s);
int load_state(simple_state_t *s);
void save_sm_state(Dispenser *dis);
#endif //PILL_DISPENSER_5_EEPROM_H