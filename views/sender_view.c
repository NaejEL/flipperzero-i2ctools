#include "sender_view.h"

void draw_sender_view(Canvas* canvas, i2cSender* i2c_sender) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);

    if(!i2c_sender->scanner->scanned) {
        scan_i2c_bus(i2c_sender->scanner);
    }

    canvas_set_font(canvas, FontSecondary);
    if(i2c_sender->scanner->nb_found <= 0) {
        canvas_draw_str_aligned(canvas, 20, 5, AlignLeft, AlignTop, "No peripherals found");
        return;
    }
    // Send Button
    canvas_draw_rbox(canvas, 45, 48, 45, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 50, 50, &I_Ok_btn_9x9);
    canvas_draw_str_aligned(canvas, 62, 51, AlignLeft, AlignTop, "Send");
    // Addr
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, 3, 5, AlignLeft, AlignTop, "Addr: ");
    char addr_text[8];
    snprintf(
        addr_text,
        sizeof(addr_text),
        "0x%02x",
        (int)i2c_sender->scanner->addresses[i2c_sender->address_idx]);
    canvas_draw_str_aligned(canvas, 43, 5, AlignLeft, AlignTop, addr_text);
    if(i2c_sender->menu_index == 0) {
        canvas_draw_icon(canvas, 33, 7, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 68, 7, &I_ButtonDown_7x4);
    }
    // Read/Write
    if(i2c_sender->write) {
        canvas_draw_str_aligned(canvas, 78, 5, AlignLeft, AlignTop, "W");
    } else {
        canvas_draw_str_aligned(canvas, 78, 5, AlignLeft, AlignTop, "R");
    }
    if(i2c_sender->menu_index == 1) {
        canvas_draw_icon(canvas, 68, 5, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 89, 5, &I_ButtonDown_7x4);
    }

    // Value
    canvas_draw_str_aligned(canvas, 3, 15, AlignLeft, AlignTop, "Value: ");
    snprintf(
        addr_text, sizeof(addr_text), "0x%02x", (int)i2c_sender->value[i2c_sender->current_frame]);
    canvas_draw_str_aligned(canvas, 43, 15, AlignLeft, AlignTop, addr_text);
    // Frame Index
    snprintf(
        addr_text,
        sizeof(addr_text),
        "%d/%d",
        (int)i2c_sender->current_frame + 1,
        (int)i2c_sender->tosend_size);
    canvas_draw_str_aligned(canvas, 78, 15, AlignLeft, AlignTop, addr_text);
    if(i2c_sender->menu_index == 2) {
        canvas_draw_icon(canvas, 33, 17, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 68, 17, &I_ButtonDown_7x4);
    } else if(i2c_sender->menu_index == 3) {
        canvas_draw_icon(canvas, 68, 17, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 107, 17, &I_ButtonDown_7x4);
    }

    if(i2c_sender->must_send) {
        i2c_send(i2c_sender);
    }
    // Result
    canvas_draw_str_aligned(canvas, 3, 25, AlignLeft, AlignTop, "Result: ");
    if(i2c_sender->error) {
        canvas_draw_str_aligned(canvas, 35, 25, AlignLeft, AlignTop, "Error");
        return;
    }
    if(i2c_sender->sended) {
        uint8_t row = 1;
        uint8_t column = 1;
        const uint8_t x_min = 8;
        const uint8_t y_min = 25;
        uint8_t x_pos = 0;
        uint8_t y_pos = 0;
        for(uint8_t i = 0; i < sizeof(i2c_sender->recv); i++) {
            x_pos = x_min + (column - 1) * 35;
            if(row == 1) {
                x_pos += 35;
            }
            y_pos = y_min + (row - 1) * 10;
            snprintf(addr_text, sizeof(addr_text), "0x%02x", (int)i2c_sender->recv[i]);
            canvas_draw_str_aligned(canvas, x_pos, y_pos, AlignLeft, AlignTop, addr_text);
            column++;
            if((row > 1 && column > 3) || (row == 1 && column > 2)) {
                column = 1;
                row++;
            }
            if(row > 2) {
                break;
            }
        }
    }
}