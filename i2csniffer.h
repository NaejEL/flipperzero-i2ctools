#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef struct Storage Storage;
typedef struct File File;

// I2C Pins
#define pinSCL &gpio_ext_pc0
#define pinSDA &gpio_ext_pc1

// Bus States
typedef enum { I2C_BUS_FREE, I2C_BUS_STARTED } i2cBusStates;

// Max read size of i2c frame by message
// Arbitraly defined
// They're not real limit to maximum frames send
#define MAX_MESSAGE_SIZE 128

// Nb of records
#define MAX_RECORDS 128

#define I2C_SNIFFER_LOG_MESSAGE_SIZE 64

/// @brief Struct used to store our reads
typedef struct {
    uint8_t data[MAX_MESSAGE_SIZE];
    bool ack[MAX_MESSAGE_SIZE];
    uint8_t bit_index;
    uint8_t data_index;
    bool logged;
} i2cFrame;

typedef struct {
    bool started;
    bool first;
    i2cBusStates state;
    i2cFrame frames[MAX_RECORDS];
    uint8_t frame_index;
    uint8_t menu_index;
    uint8_t row_index;
    bool logging_enabled;
    bool log_error_pending;
    char log_error_message[I2C_SNIFFER_LOG_MESSAGE_SIZE];
    Storage* storage;
    File* log_file;
} i2cSniffer;

void clear_sniffer_buffers(i2cSniffer* i2c_sniffer);
void start_interrupts(i2cSniffer* i2c_sniffer);
void stop_interrupts();
void SDAcallback(void* _i2c_sniffer);
void SCLcallback(void* _i2c_sniffer);

i2cSniffer* i2c_sniffer_alloc();
void i2c_sniffer_free(i2cSniffer* i2c_sniffer);

bool i2c_sniffer_start_logging(i2cSniffer* i2c_sniffer);
void i2c_sniffer_stop_logging(i2cSniffer* i2c_sniffer);
void i2c_sniffer_finalize_current_frame(i2cSniffer* i2c_sniffer);

