#include "lorawan.h"

#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "iuart.h"


void lorawan_init(void) {
    iuart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);
}
bool lorawan_send_command(const char *command, const char *expect, uint32_t timeout_ms) {

    char response[LORA_RESPONSE_LEN];
    printf("[LORA] >> %s", command);
    iuart_send(UART_NR, command);
    uint32_t start = time_us_32();
    while (time_us_32() - start < timeout_ms * 1000) {
        if (uart_readable_timeout(UART_NR, response, sizeof(response), 2000)) {
            printf("[LORA] << %s\n", response);
            if (strstr(response, expect)) {
                return true;
            }
        }
    }
    return false;
}

bool lorawan_join(void) {

    if (!lorawan_send_command("AT\r\n", "OK", 500)) {
        printf("[LORA] ERROR: No response to AT.\n");
        return false;
    }

    if (!lorawan_send_command("AT+MODE=LWOTAA\r\n", "+MODE:", 500)) {
        return false;
    }

    printf("[LORA] Setting APPKEY......\n");
    if (!lorawan_send_command("AT+KEY=APPKEY,\"9c3ccbe1a7b0844775a045933be85009\"\r\n", "+KEY:", 500)) {
        return false;
    }

    printf("[LORA] Setting CLASS A......\n");
    if (!lorawan_send_command("AT+CLASS=A\r\n", "+CLASS:", 500)) {
        return false;
    }

    printf("[LORA] Setting PORT......\n");
    if (!lorawan_send_command("AT+PORT=8\r\n", "+PORT:", 500)) {
        return false;
    }
    /*
    1. Join
    eg: AT+JOIN // Send JOIN request
    3. Returns
    a) Join successfully
    +JOIN: Starting
    +JOIN: NORMAL
    +JOIN: NetID 000024 DevAddr 48:00:00:01
    +JOIN: Done
    b) Join failed
    +JOIN: Join failed
    c) Join process is ongoing
    +JOIN: LoRaWAN modem is busy
    */

    printf("[LORA] Joining network......\n");
    if (!lorawan_send_command("AT+JOIN\r\n", "+JOIN: Done", 17000)) {   //longest!!!
        printf("[LORA] JOIN timed out or failed.\n");
        return false;
    }

    printf("[LORA] JOIN OK.\n");
    return true;

}
bool lorawan_send_message(const char *message) {
    char cmd[LORA_SEND_MESSAGE_BUFFER];
    snprintf(cmd, sizeof(cmd), "AT+MSG=\"%s\"\r\n", message);

    printf("[LORA] Attempting to send message...\n");
    /*
    *Format:
    AT+MSG="Data to send"
    Return: (Full return message)
    +MSG: Start
    +MSG: FPENDING
    +MSG: Link 20, 1
    +MSG: ACK Received
    +MSG: MULTICAST
    +MSG: PORT: 8; RX: "12345678"
    +MSG: RXWIN214, RSSI -106, SNR 4
    +MSG: Done
    */
    if (lorawan_send_command(cmd, "+MSG: Done", 7000)) { //7s
        return true;
    }

    printf("[LORA] Send failed or timed out\n");
    return false;
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

    for (int attempt = 0; attempt < LORA_JOIN_MAX_ATTEMPTS; attempt++) {
        printf("[LORA] LoRa join attempt %d/%d\n", attempt, LORA_JOIN_MAX_ATTEMPTS);

        if (lorawan_join()) {
            printf("[LORA] JOIN SUCCESS\n");
            return true;
        }

        printf("[LORA] JOIN FAILED\n");
    }

    printf("[LORA] Max join attempts reached.\n");
    return false;
}

void send_status_to_lorawan(Dispenser *dis, const char *status) {
    if (dis->is_lorawan_connected) {
        lorawan_send_message(status);
    }
}