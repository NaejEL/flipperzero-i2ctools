#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "i2cscanner.h"
#include "i2csniffer.h"

#define MAX_DATA_TO_SEND 255

typedef struct {
    uint8_t address_idx;
    uint8_t value[MAX_DATA_TO_SEND];
    uint8_t tosend_size;
    uint8_t recv[5];
    bool must_send;
    bool sended;
    bool error;
    uint8_t menu_index;
    uint8_t current_frame;
    bool write;

    i2cScanner* scanner;
    i2cSniffer* sniffer;
} i2cSender;

void i2c_send(i2cSender* i2c_sender);

i2cSender* i2c_sender_alloc();
void i2c_sender_free(i2cSender* i2c_sender);
