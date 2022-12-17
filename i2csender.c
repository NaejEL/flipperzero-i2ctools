#include "i2csender.h"

void i2c_send(i2cSender* i2c_sender) {
    uint8_t adress = i2c_sender->scanner->addresses[i2c_sender->address_idx] << 1;
    if(!i2c_sender->write) {
        adress &= 0x01;
    }
    start_interrupts(i2c_sender->sniffer);
    furi_hal_i2c_acquire(I2C_BUS);
    i2c_sender->error = furi_hal_i2c_trx(
        I2C_BUS,
        adress,
        i2c_sender->value,
        i2c_sender->tosend_size,
        i2c_sender->recv,
        sizeof(i2c_sender->recv),
        I2C_TIMEOUT);
    furi_hal_i2c_release(I2C_BUS);
    i2c_sender->must_send = false;
    i2c_sender->sended = true;
    stop_interrupts(i2c_sender->sniffer);
}

i2cSender* i2c_sender_alloc() {
    i2cSender* i2c_sender = malloc(sizeof(i2cSender));
    i2c_sender->must_send = false;
    i2c_sender->sended = false;
    i2c_sender->tosend_size = 1;
    i2c_sender->menu_index = 0;
    i2c_sender->current_frame = 0;
    i2c_sender->write = false;
    return i2c_sender;
}

void i2c_sender_free(i2cSender* i2c_sender) {
    furi_assert(i2c_sender);
    free(i2c_sender);
}