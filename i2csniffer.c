#include "i2csniffer.h"

#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define I2C_TOOLS_APP_DATA_DIR "/ext/apps_data/i2ctools"
#define I2C_TOOLS_LOG_FILE_PATH I2C_TOOLS_APP_DATA_DIR "/sniffer.log"

static void i2c_sniffer_reset_frame(i2cFrame* frame) {
    for(uint8_t j = 0; j < MAX_MESSAGE_SIZE; j++) {
        frame->ack[j] = false;
        frame->data[j] = 0;
    }
    frame->bit_index = 0;
    frame->data_index = 0;
    frame->logged = false;
}

static void i2c_sniffer_abort_logging(i2cSniffer* i2c_sniffer) {
    if(!i2c_sniffer) {
        return;
    }
    if(i2c_sniffer->log_file) {
        storage_file_close(i2c_sniffer->log_file);
        storage_file_free(i2c_sniffer->log_file);
        i2c_sniffer->log_file = NULL;
    }
    if(i2c_sniffer->storage) {
        furi_record_close(RECORD_STORAGE);
        i2c_sniffer->storage = NULL;
    }
    i2c_sniffer->logging_enabled = false;
}

static void i2c_sniffer_handle_log_error(i2cSniffer* i2c_sniffer, const char* message) {
    if(!i2c_sniffer) {
        return;
    }
    i2c_sniffer_abort_logging(i2c_sniffer);
    if(message) {
        snprintf(
            i2c_sniffer->log_error_message,
            I2C_SNIFFER_LOG_MESSAGE_SIZE,
            "%s",
            message);
    } else {
        i2c_sniffer->log_error_message[0] = '\0';
    }
    if(!i2c_sniffer->log_error_pending) {
        i2c_sniffer->log_error_pending = true;
    }
}

static bool i2c_sniffer_write_log(i2cSniffer* i2c_sniffer, const char* data, size_t size) {
    if(!i2c_sniffer || !i2c_sniffer->logging_enabled || !i2c_sniffer->log_file) {
        return false;
    }
    if(!storage_file_write(i2c_sniffer->log_file, data, size)) {
        i2c_sniffer_handle_log_error(i2c_sniffer, "Log write failed");
        return false;
    }
    if(!storage_file_sync(i2c_sniffer->log_file)) {
        i2c_sniffer_handle_log_error(i2c_sniffer, "Log sync failed");
        return false;
    }
    return true;
}

static void i2c_sniffer_log_frame(i2cSniffer* i2c_sniffer, const i2cFrame* frame) {
    if(!i2c_sniffer || !frame || frame->data_index == 0) {
        return;
    }
    if(!i2c_sniffer->logging_enabled || !i2c_sniffer->log_file) {
        return;
    }

    char line[512];
    size_t offset = 0;
    uint8_t address_byte = frame->data[0];
    uint8_t address = address_byte >> 1;
    bool read = address_byte & 0x01;
    int result = snprintf(line, sizeof(line), "ADDR=0x%02X %c", address, read ? 'R' : 'W');
    if(result < 0) {
        return;
    }
    offset = (size_t)result;
    if(offset >= sizeof(line)) {
        offset = sizeof(line) - 1;
    }

    uint8_t data_start = 1;
    if(!read && frame->data_index > 1) {
        result = snprintf(line + offset, sizeof(line) - offset, " CMD=0x%02X", frame->data[1]);
        if(result < 0) {
            return;
        }
        offset += (size_t)result;
        if(offset >= sizeof(line)) {
            offset = sizeof(line) - 1;
        }
        data_start = 2;
    } else {
        result = snprintf(line + offset, sizeof(line) - offset, " CMD=--");
        if(result < 0) {
            return;
        }
        offset += (size_t)result;
        if(offset >= sizeof(line)) {
            offset = sizeof(line) - 1;
        }
    }

    if(frame->data_index > data_start) {
        result = snprintf(line + offset, sizeof(line) - offset, " DATA=");
        if(result < 0) {
            return;
        }
        offset += (size_t)result;
        if(offset >= sizeof(line)) {
            offset = sizeof(line) - 1;
        }
        for(uint8_t idx = data_start; idx < frame->data_index; idx++) {
            result = snprintf(line + offset, sizeof(line) - offset, "%s0x%02X", idx == data_start ? "" : " ", frame->data[idx]);
            if(result < 0) {
                return;
            }
            offset += (size_t)result;
            if(offset >= sizeof(line)) {
                offset = sizeof(line) - 1;
                break;
            }
        }
    }

    if(offset < sizeof(line) - 1) {
        line[offset++] = '\n';
    }
    line[offset < sizeof(line) ? offset : sizeof(line) - 1] = '\0';

    i2c_sniffer_write_log(i2c_sniffer, line, strlen(line));
}

void clear_sniffer_buffers(i2cSniffer* i2c_sniffer) {
    furi_assert(i2c_sniffer);
    for(uint8_t i = 0; i < MAX_RECORDS; i++) {
        i2c_sniffer_reset_frame(&i2c_sniffer->frames[i]);
    }
    i2c_sniffer->frame_index = 0;
    i2c_sniffer->state = I2C_BUS_FREE;
    i2c_sniffer->first = true;
}

void start_interrupts(i2cSniffer* i2c_sniffer) {
    furi_assert(i2c_sniffer);
    furi_hal_gpio_init(pinSCL, GpioModeInterruptRise, GpioPullNo, GpioSpeedHigh);
    furi_hal_gpio_add_int_callback(pinSCL, SCLcallback, i2c_sniffer);

    // Add Rise and Fall Interrupt on SDA pin
    furi_hal_gpio_init(pinSDA, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedHigh);
    furi_hal_gpio_add_int_callback(pinSDA, SDAcallback, i2c_sniffer);
}

void stop_interrupts() {
    furi_hal_gpio_remove_int_callback(pinSCL);
    furi_hal_gpio_remove_int_callback(pinSDA);
    // Reset GPIO pins to default state
    furi_hal_gpio_init(pinSCL, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(pinSDA, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

// Called on Fallin/Rising SDA
// Used to monitor i2c bus state
void SDAcallback(void* _i2c_sniffer) {
    i2cSniffer* i2c_sniffer = _i2c_sniffer;
    // SCL is low maybe cclock strecching
    if(furi_hal_gpio_read(pinSCL) == false) {
        return;
    }
    // Check for stop condition: SDA rising while SCL is High
    if(i2c_sniffer->state == I2C_BUS_STARTED) {
        if(furi_hal_gpio_read(pinSDA) == true) {
            i2c_sniffer_finalize_current_frame(i2c_sniffer);
            i2c_sniffer->state = I2C_BUS_FREE;
        }
    }
    // Check for start condition: SDA falling while SCL is high
    else if(furi_hal_gpio_read(pinSDA) == false) {
        i2c_sniffer_finalize_current_frame(i2c_sniffer);
        i2c_sniffer->state = I2C_BUS_STARTED;
        if(i2c_sniffer->first) {
            i2c_sniffer->first = false;
            i2c_sniffer_reset_frame(&i2c_sniffer->frames[i2c_sniffer->frame_index]);
            return;
        }
        if(i2c_sniffer->frame_index < (MAX_RECORDS - 1)) {
            i2c_sniffer->frame_index++;
            i2c_sniffer_reset_frame(&i2c_sniffer->frames[i2c_sniffer->frame_index]);
        } else {
            clear_sniffer_buffers(i2c_sniffer);
            i2c_sniffer->first = false;
            i2c_sniffer->state = I2C_BUS_STARTED;
            i2c_sniffer_reset_frame(&i2c_sniffer->frames[i2c_sniffer->frame_index]);
        }
    }
    return;
}

// Called on Rising SCL
// Used to read bus datas
void SCLcallback(void* _i2c_sniffer) {
    i2cSniffer* i2c_sniffer = _i2c_sniffer;
    if(i2c_sniffer->state == I2C_BUS_FREE) {
        return;
    }
    uint8_t frame = i2c_sniffer->frame_index;
    uint8_t bit = i2c_sniffer->frames[frame].bit_index;
    uint8_t data_idx = i2c_sniffer->frames[frame].data_index;
    if(bit < 8) {
        i2c_sniffer->frames[frame].data[data_idx] <<= 1;
        i2c_sniffer->frames[frame].data[data_idx] |= (int)furi_hal_gpio_read(pinSDA);
        i2c_sniffer->frames[frame].bit_index++;
    } else {
        i2c_sniffer->frames[frame].ack[data_idx] = !furi_hal_gpio_read(pinSDA);
        i2c_sniffer->frames[frame].data_index++;
        i2c_sniffer->frames[frame].bit_index = 0;
    }
}

i2cSniffer* i2c_sniffer_alloc() {
    i2cSniffer* i2c_sniffer = malloc(sizeof(i2cSniffer));
    i2c_sniffer->started = false;
    i2c_sniffer->row_index = 0;
    i2c_sniffer->menu_index = 0;
    i2c_sniffer->logging_enabled = false;
    i2c_sniffer->log_error_pending = false;
    i2c_sniffer->log_error_message[0] = '\0';
    i2c_sniffer->storage = NULL;
    i2c_sniffer->log_file = NULL;
    clear_sniffer_buffers(i2c_sniffer);
    return i2c_sniffer;
}

void i2c_sniffer_finalize_current_frame(i2cSniffer* i2c_sniffer) {
    furi_assert(i2c_sniffer);
    if(i2c_sniffer->frame_index >= MAX_RECORDS) {
        return;
    }
    i2cFrame* frame = &i2c_sniffer->frames[i2c_sniffer->frame_index];
    if(frame->logged || frame->data_index == 0) {
        return;
    }
    frame->logged = true;
    i2c_sniffer_log_frame(i2c_sniffer, frame);
}

bool i2c_sniffer_start_logging(i2cSniffer* i2c_sniffer) {
    furi_assert(i2c_sniffer);
    if(i2c_sniffer->logging_enabled) {
        return true;
    }
    i2c_sniffer->log_error_pending = false;
    i2c_sniffer->log_error_message[0] = '\0';
    i2c_sniffer->storage = furi_record_open(RECORD_STORAGE);
    if(!i2c_sniffer->storage) {
        return false;
    }
    i2c_sniffer->log_file = storage_file_alloc(i2c_sniffer->storage);
    if(!i2c_sniffer->log_file) {
        furi_record_close(RECORD_STORAGE);
        i2c_sniffer->storage = NULL;
        return false;
    }
    storage_common_mkdir(i2c_sniffer->storage, I2C_TOOLS_APP_DATA_DIR);
    if(!storage_file_open(
           i2c_sniffer->log_file,
           I2C_TOOLS_LOG_FILE_PATH,
           FSAM_WRITE,
           FSOM_CREATE_ALWAYS)) {
        storage_common_mkdir(i2c_sniffer->storage, I2C_TOOLS_APP_DATA_DIR);
        if(!storage_file_open(
               i2c_sniffer->log_file,
               I2C_TOOLS_LOG_FILE_PATH,
               FSAM_WRITE,
               FSOM_CREATE_ALWAYS)) {
            storage_file_free(i2c_sniffer->log_file);
            i2c_sniffer->log_file = NULL;
            furi_record_close(RECORD_STORAGE);
            i2c_sniffer->storage = NULL;
            return false;
        }
    }
    i2c_sniffer->logging_enabled = true;
    return true;
}

void i2c_sniffer_stop_logging(i2cSniffer* i2c_sniffer) {
    if(!i2c_sniffer) {
        return;
    }
    i2c_sniffer_finalize_current_frame(i2c_sniffer);
    if(i2c_sniffer->logging_enabled && i2c_sniffer->log_file) {
        if(!storage_file_sync(i2c_sniffer->log_file)) {
            i2c_sniffer_handle_log_error(i2c_sniffer, "Log sync failed");
            return;
        }
    }
    i2c_sniffer_abort_logging(i2c_sniffer);
}

void i2c_sniffer_free(i2cSniffer* i2c_sniffer) {
    furi_assert(i2c_sniffer);
    if(i2c_sniffer->started) {
        stop_interrupts();
    }
    i2c_sniffer_stop_logging(i2c_sniffer);
    free(i2c_sniffer);
}
