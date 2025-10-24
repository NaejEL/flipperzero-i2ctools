#include "i2ctools_i.h"

#include <dialogs/dialogs.h>

static void i2ctools_show_dialog_message(const char* text) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "I2C Tools", 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);
}

void i2ctools_draw_callback(Canvas* canvas, void* ctx) {
    i2cTools* i2ctools = ctx;
    if(furi_mutex_acquire(i2ctools->mutex, 200) != FuriStatusOk) {
        return;
    }

    switch(i2ctools->main_view->current_view) {
    case MAIN_VIEW:
        draw_main_view(canvas, i2ctools->main_view);
        break;

    case SCAN_VIEW:
        draw_scanner_view(canvas, i2ctools->scanner);
        break;

    case SNIFF_VIEW:
        draw_sniffer_view(canvas, i2ctools->sniffer);
        break;

    case SEND_VIEW:
        draw_sender_view(canvas, i2ctools->sender);
        break;

    case INFOS_VIEW:
        draw_infos_view(canvas);
        break;

    default:
        break;
    }
    furi_mutex_release(i2ctools->mutex);
}

void i2ctools_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t i2ctools_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Alloc i2ctools
    i2cTools* i2ctools = malloc(sizeof(i2cTools));
    i2ctools->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Alloc viewport
    i2ctools->view_port = view_port_alloc();
    view_port_draw_callback_set(i2ctools->view_port, i2ctools_draw_callback, i2ctools);
    view_port_input_callback_set(i2ctools->view_port, i2ctools_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, i2ctools->view_port, GuiLayerFullscreen);

    InputEvent event;

    i2ctools->main_view = i2c_main_view_alloc();

    i2ctools->sniffer = i2c_sniffer_alloc();
    i2ctools->sniffer->menu_index = 0;

    i2ctools->scanner = i2c_scanner_alloc();

    i2ctools->sender = i2c_sender_alloc();
    // Share scanner with sender
    i2ctools->sender->scanner = i2ctools->scanner;

    while(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        // Back
        if(event.key == InputKeyBack && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == MAIN_VIEW) {
                break;
            } else {
                if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                    stop_interrupts();
                    i2c_sniffer_stop_logging(i2ctools->sniffer);
                    i2ctools->sniffer->started = false;
                    i2ctools->sniffer->state = I2C_BUS_FREE;
                }
                i2ctools->main_view->current_view = MAIN_VIEW;
            }
        }
        // Up
        else if(event.key == InputKeyUp && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == MAIN_VIEW) {
                if((i2ctools->main_view->menu_index > SCAN_VIEW)) {
                    i2ctools->main_view->menu_index--;
                }
            } else if(i2ctools->main_view->current_view == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index > 0) {
                    i2ctools->scanner->menu_index--;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if(i2ctools->sniffer->row_index > 0) {
                    i2ctools->sniffer->row_index--;
                }
            } else if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->value < 0xFF) {
                    i2ctools->sender->value++;
                    i2ctools->sender->sended = false;
                }
            }
        }
        // Long Up
        else if(
            event.key == InputKeyUp &&
            (event.type == InputTypeLong || event.type == InputTypeRepeat)) {
            if(i2ctools->main_view->current_view == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index > 5) {
                    i2ctools->scanner->menu_index -= 5;
                }
            } else if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->value < 0xF9) {
                    i2ctools->sender->value += 5;
                    i2ctools->sender->sended = false;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if(i2ctools->sniffer->row_index > 5) {
                    i2ctools->sniffer->row_index -= 5;
                } else {
                    i2ctools->sniffer->row_index = 0;
                }
            }
        }
        // Down
        else if(event.key == InputKeyDown && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == MAIN_VIEW) {
                if(i2ctools->main_view->menu_index < MENU_SIZE - 1) {
                    i2ctools->main_view->menu_index++;
                }
            } else if(i2ctools->main_view->current_view == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index < ((int)i2ctools->scanner->nb_found / 3)) {
                    i2ctools->scanner->menu_index++;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if((i2ctools->sniffer->row_index + 3) <
                   (int)i2ctools->sniffer->frames[i2ctools->sniffer->menu_index].data_index) {
                    i2ctools->sniffer->row_index++;
                }
            } else if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->value > 0x00) {
                    i2ctools->sender->value--;
                    i2ctools->sender->sended = false;
                }
            }
        }
        // Long Down
        else if(
            event.key == InputKeyDown &&
            (event.type == InputTypeLong || event.type == InputTypeRepeat)) {
            if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->value > 0x05) {
                    i2ctools->sender->value -= 5;
                    i2ctools->sender->sended = false;
                } else {
                    i2ctools->sender->value = 0;
                    i2ctools->sender->sended = false;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if((i2ctools->sniffer->row_index + 8) <
                   (int)i2ctools->sniffer->frames[i2ctools->sniffer->menu_index].data_index) {
                    i2ctools->sniffer->row_index += 5;
                }
            }

        } else if(event.key == InputKeyOk && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == MAIN_VIEW) {
                i2ctools->main_view->current_view = i2ctools->main_view->menu_index;
            } else if(i2ctools->main_view->current_view == SCAN_VIEW) {
                scan_i2c_bus(i2ctools->scanner);
            } else if(i2ctools->main_view->current_view == SEND_VIEW) {
                i2ctools->sender->must_send = true;
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if(i2ctools->sniffer->started) {
                    stop_interrupts();
                    i2c_sniffer_stop_logging(i2ctools->sniffer);
                    i2ctools->sniffer->started = false;
                    i2ctools->sniffer->state = I2C_BUS_FREE;
                } else {
                    if(!i2c_sniffer_start_logging(i2ctools->sniffer)) {
                        i2ctools_show_dialog_message("Storage unavailable");
                    }
                    clear_sniffer_buffers(i2ctools->sniffer);
                    start_interrupts(i2ctools->sniffer);
                    i2ctools->sniffer->started = true;
                    i2ctools->sniffer->state = I2C_BUS_FREE;
                }
            }
        } else if(event.key == InputKeyRight && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->address_idx < (i2ctools->scanner->nb_found - 1)) {
                    i2ctools->sender->address_idx++;
                    i2ctools->sender->sended = false;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if(i2ctools->sniffer->menu_index < i2ctools->sniffer->frame_index) {
                    i2ctools->sniffer->menu_index++;
                    i2ctools->sniffer->row_index = 0;
                }
            }
        } else if(event.key == InputKeyLeft && event.type == InputTypeRelease) {
            if(i2ctools->main_view->current_view == SEND_VIEW) {
                if(i2ctools->sender->address_idx > 0) {
                    i2ctools->sender->address_idx--;
                    i2ctools->sender->sended = false;
                }
            } else if(i2ctools->main_view->current_view == SNIFF_VIEW) {
                if(i2ctools->sniffer->menu_index > 0) {
                    i2ctools->sniffer->menu_index--;
                    i2ctools->sniffer->row_index = 0;
                }
            }
        }
        view_port_update(i2ctools->view_port);
        if(i2ctools->sniffer->log_error_pending) {
            const char* toast_message =
                i2ctools->sniffer->log_error_message[0] != '\0'
                    ? i2ctools->sniffer->log_error_message
                    : "Log write failed";
            i2ctools_show_dialog_message(toast_message);
            i2ctools->sniffer->log_error_pending = false;
            i2ctools->sniffer->log_error_message[0] = '\0';
        }
    }
    gui_remove_view_port(gui, i2ctools->view_port);
    view_port_free(i2ctools->view_port);
    furi_message_queue_free(event_queue);
    i2c_sniffer_free(i2ctools->sniffer);
    i2c_scanner_free(i2ctools->scanner);
    i2c_sender_free(i2ctools->sender);
    i2c_main_view_free(i2ctools->main_view);
    free(i2ctools);
    furi_record_close(RECORD_GUI);
    return 0;
}
