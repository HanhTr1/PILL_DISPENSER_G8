
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include "eeprom.h"
#include "board_config.h"
#include "hardware/gpio.h"

void init_default_state(device_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = 32;
    s->version = 1;
    s->calibrated = 0;
    s->current_slot = 0;
    s->pills_left = 7;
    s->in_progress = 0;
    s->disp_state =ST_BOOT;
    save_state(s);
}

int eeprom_write(uint16_t addr, uint8_t *data, size_t len) {
    if (len > LOG_ENTRY_SIZE) {
        return -1;
    }
    uint8_t tx[2 + len];
    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)addr & 0xFF;
    for (int i = 0; i < len; i++) {
        tx[2 + i] = data[i];
    }
    int write=i2c_write_blocking(I2C_PORT, EEPROM_I2C_ADDR,
                              tx, 2 + len, false);
    if (write !=2+len) {
        return -1; //error
    }
    sleep_ms(5);

    return 0;
}
int eeprom_read(uint16_t addr, uint8_t *data, size_t len) {
    if (len > LOG_ENTRY_SIZE) {
        return -1;
    }
    uint8_t tx[2];
    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t) (addr& 0xFF);

    i2c_write_blocking(I2C_PORT, EEPROM_I2C_ADDR, tx, 2, true);
    int read= i2c_read_blocking(I2C_PORT, EEPROM_I2C_ADDR, data, len, false);
    if (read != (int)len) {
        return -1;
    }
    return 0;
}
void setup_i2c(void) {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}
uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint16_t crc = 0xFFFF;
    while (length--) {
        uint8_t x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^((uint16_t)(x << 5))^ ((uint16_t)x);
    }
    return crc;
}
bool eeprom_available() {
    uint8_t test =0;
    return eeprom_read(EEPROM_STORE_ADDR, &test, 1) ==0;
}
int find_log() {
    uint8_t first_byte;
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        uint16_t addr =LOG_START_ADDR +i*LOG_ENTRY_SIZE;
        if (eeprom_read(addr, &first_byte, 1) !=0) {
            return -1;
        }
        if (first_byte ==0) {
            return i; //log empty
        }
    }

    return -1; //full
}

void erase_log() {
    uint8_t zero =0;
    for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
        uint16_t addr = i*LOG_ENTRY_SIZE;
        eeprom_write(addr,&zero,1);
        sleep_ms(5);
    }
    printf("Log is erase\n");
}

void write_log(char *msg) {
    if (!eeprom_available()) {
        printf("EEPROM not available\n");
        return;
    }
    int find =find_log();
    if (find == -1) {
        erase_log();
        find =0;
    }
    uint16_t addr = LOG_START_ADDR + find * LOG_ENTRY_SIZE;
    uint8_t entry[LOG_ENTRY_SIZE] = {0};

    snprintf((char*)entry, LOG_ENTRY_SIZE -2, "%s", msg); //61+null
    int str_len = strlen((char*)entry)+1;

    uint16_t check_crc = crc16(entry, str_len);   // include \0
    entry[str_len ] = (uint8_t)(check_crc >> 8);
    entry[str_len + 1] = (uint8_t)(check_crc);

    eeprom_write(addr, entry, LOG_ENTRY_SIZE);
    printf("Log [%d] %s\n",find, msg);
}
//read command
void read_log() {
    if (!eeprom_available()) {
        printf("EEPROM not available\n");
        return;
    }
    uint8_t entry[LOG_ENTRY_SIZE];
    for (int i = 0; i < LOG_MAX_ENTRIES ; i++) {
        uint16_t addr = LOG_START_ADDR + i * LOG_ENTRY_SIZE;
        if (eeprom_read(addr, entry, LOG_ENTRY_SIZE) !=0) {
            printf("EEPROM READ ERROR\n");
            return;
        }
        if (entry[0] ==0) {
            return;
        }

        int len = -1;

        for (int j = 0; j < LOG_STRING_MAX_LEN+1; j++) { //find \0 at idx 62
            if (entry[j] == 0) {
                len = j;
                break;
            }
        }
        if (len ==-1) {
            printf("Invalid entry at index %d\n", i);
            return;
        }
        uint16_t cal_crc = crc16(entry, len+1+2);
       if (cal_crc != 0) {
           printf("CRC ERROR\n");
           return;
       }
        printf("Log %d: %.*s\n\n", i, len, (char*)entry);
    }

}
void validate_command( char *command, int *command_idx) {
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (ch == '\n' || ch == '\r') {
            command[*command_idx] = 0;

            for (int i = 0; i < *command_idx; i++)
                command[i] = (char)tolower((unsigned char)command[i]);
            *command_idx = 0;

            if (strcmp(command, "read") == 0 && eeprom_available()) {
                read_log();
            } else if (strcmp(command, "erase") == 0 && eeprom_available()) {
                erase_log();
            } else if (command[0] != 0) {
                printf("Unknown command\n");
            }
        } else if (*command_idx < COMMAND_SIZE - 1) {
            command[(*command_idx)++] = (char)ch;
        }
    }
}
int save_state(device_state_t *s)
{
    device_state_t buf;
    memcpy(&buf, s, sizeof(buf));

    buf.crc16 = 0;
    uint16_t c = crc16((uint8_t*)&buf, sizeof(buf) - 2);
    buf.crc16 = c;

    return eeprom_write(STATE_ADDR, (uint8_t*)&buf, sizeof(buf));
}

int load_state(device_state_t *s)
{
    device_state_t buf;

    if (eeprom_read(STATE_ADDR, (uint8_t*)&buf, sizeof(buf)) != 0)
        return -1;

    if (buf.magic != 32 || buf.version != 1)
        return -2;

    uint16_t saved_crc = buf.crc16;
    buf.crc16 = 0;
    uint16_t c = crc16((uint8_t*)&buf, sizeof(buf) - 2);

    if (c != saved_crc)
        return -3;

    memcpy(s, &buf, sizeof(buf));
    return 0;
}
void mark_turn_start(device_state_t *s)
{
    s->in_progress = 1;
    save_state(s);
}
void mark_turn_complete(device_state_t *s, bool pill_ok)
{
    s->in_progress = 0;

    if (pill_ok && s->pills_left > 0)
        s->pills_left--;

    if (s->current_slot >= 7) {
        s->current_slot = 0;
        s->dispense_state = ST_FINISHED;
    } else {
        s->current_slot++;
    }

    save_state(s);
}

void auto_recalibrate(device_state_t *s)
{
    write_log("Auto recalibrate...");

    while (gpio_get(OPTO_FORK_PIN) != 0) {
        //stepper_step_once_slow();   // 1 step
        sleep_ms(8);                // tránh block dài
    }

    s->current_slot = 0;
    s->calibrated = 1;
    s->dispense_state = ST_WAIT_DISPENSING;
}







