#include "lorawan.h"

#include <stdio.h>
#include <string.h>

#include "iuart.h"


void lorawan_init(void) {
    iuart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);
}
bool lorawan_send_command(const char *command, const char *expect, uint32_t timeout_ms) {

    char response[LORA_RESPONSE_LEN];
    //printf("[LORA] >> %s", command);
    iuart_send(UART_NR, command);

    if (uart_readable_timeout(UART_NR, response, sizeof(response), timeout_ms)) {
        //printf("[LORA] << %s\n", response);

        if (strstr(response, expect) != NULL) {
            return true;
        }
    }

    return false;
}

bool lorawan_join(void) {

    if (!lorawan_send_command("AT\r\n", "OK", 500)) {
        printf("[LORA] ERROR: No response to AT.\n");
        return false;
    }
    if (!lorawan_send_command("AT+MODE=LWOTAA\r\n", "+MODE:", 1000)) {
        return false;
    }

    printf("[LORA] Setting APPKEY...\n");
    if (!lorawan_send_command("AT+KEY=APPKEY,\"c695805d0cf7cd4ee24b11be3055659e\"\r\n", "+KEY:", 1000)) {
        return false;
    }

    printf("[LORA] Setting CLASS A...\n");
    if (!lorawan_send_command("AT+CLASS=A\r\n", "+CLASS:", 1000)) {
        return false;
    }

    printf("[LORA] Setting PORT...\n");
    if (!lorawan_send_command("AT+PORT=8\r\n", "+PORT:", 1000)) {
        return false;
    }

    printf("[LORA] Joining network...\n");
    if (!lorawan_send_command("AT+JOIN\r\n", "+JOIN", 20000)) {   //longest!!!
        printf("[LORA] JOIN timed out or failed.\n");
        return false;
    }

    printf("[LORA] JOIN OK.\n");
    return true;

}
bool lorawan_send(const char *message) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+MSG=\"%s\"\r\n", message);

    return lorawan_send_command(cmd, "+MSG", 5000);
}

bool uart_readable_timeout(int uart_nr, char* buffer, int max_len, uint32_t timeout_ms) {
    int pos = 0;
    uint32_t start_time = time_us_32();
    uint32_t timeout_us = (timeout_ms * 1000);

    buffer[0] = '\0';

    while (time_us_32() - start_time < timeout_us) {
        uint8_t c;
        int bytes_read = iuart_read(uart_nr, &c, 1);

        if (bytes_read > 0) {
            //have some data, reset timeout
            start_time = time_us_32();

            //end of line
            if (c == '\n') {
                buffer[pos] = '\0';
                return true;
            }
            else if (c != '\r' && pos < max_len - 1) {
                buffer[pos++] = c;
            }
        }
    }
    return false;
}

bool handle_lorawan(void) {

    for (int attempt = 1; attempt <= LORA_JOIN_MAX_ATTEMPTS; attempt++) {
        printf("[LORA] LoRa join attempt %d/%d\n", attempt, LORA_JOIN_MAX_ATTEMPTS);

        if (lorawan_join()) {
            printf("[LORA] JOIN SUCCESS\n");
            return true;
        }

        printf("[LORA] JOIN FAILED\n");

        if (attempt < LORA_JOIN_MAX_ATTEMPTS) {
            printf("[LORA] Waiting 2s before retry...\n");
            sleep_ms(2000);  //wait a bit?
        }
    }

    printf("[LORA] Max join attempts reached.\n");
    return false;

}