
#ifndef PILL_DISPENSER_LORAWAN_H
#define PILL_DISPENSER_LORAWAN_H

#include "board_config.h"


#include <stdbool.h>

void lorawan_init(void);
bool uart_readable_timeout(int uart_nr, char* buffer, int max_len, uint32_t timeout_ms);
bool lorawan_send_command(const char *command, const char *expect, uint32_t timeout_ms);
bool lorawan_join();
bool lorawan_send(const char *message);
bool handle_lorawan(void);


#endif //PILL_DISPENSER_LORAWAN_H