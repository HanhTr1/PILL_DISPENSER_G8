
#ifndef PILL_DISPENSER_LORAWAN_H
#define PILL_DISPENSER_LORAWAN_H

#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BAUD_RATE 9600

//lora
#define LORA_JOIN_INTERVAL_MS 5000
#define LORA_JOIN_MAX_ATTEMPTS 5
#define  LORA_RESPONSE_LEN 128
#define LORA_SEND_MESSAGE_BUFFER 256


#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "pico/stdlib.h"
#include "iuart.h"

void lorawan_init(void);
bool uart_readable_timeout(int uart_nr, char* buffer, int max_len, uint32_t timeout_ms);
bool lorawan_send_command(const char *command, const char *expect, uint32_t timeout_ms);
bool lorawan_join();
bool lorawan_send_message(const char *message);
bool handle_lorawan(void);
void send_status_to_lorawan(Dispenser *dis, const char *status);
void report_event(Dispenser *dis, const char *event);


#endif //PILL_DISPENSER_LORAWAN_H