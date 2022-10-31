#include "save_view.h"

i2cToolsSaver* i2c_saver_alloc() {
    i2cToolsSaver* i2c_save;
    i2c_save->text_input = text_input_alloc();
    i2c_save->file_path = furi_string_alloc();
    i2c_save->saved = false;
    i2c_save->saving = false;
    return i2c_save;
}

void i2c_saver_free(i2cToolsSaver* i2c_save) {
    furi_assert(i2c_save);
    free(i2c_save);
}

static void i2csniffer_save_name_text_input_callback(void* context) {
    UNUSED(context);
    printf("save");
}

void i2csniffer_save_name_on_enter(void* context) {
    i2cToolsSaver* i2c_save = context;
    TextInput* text_input = i2c_save->text_input;

    FuriString* file_name;
    file_name = furi_string_alloc();
    if(furi_string_end_with(i2c_save->file_path, I2CTOOLS_APP_EXTENSION)) {
        path_extract_filename(i2c_save->file_path, file_name, true);
    }

    const bool file_name_is_empty = furi_string_empty(file_name);
    if(file_name_is_empty) {
        set_random_name(i2c_save->text_store, I2CTOOLS_TEXT_STORE_SIZE);
    } else {
        sprintf(i2c_save->text_store, "%s", file_name);
    }

    text_input_set_header_text(text_input, "Name the record");
    text_input_set_result_callback(
        text_input,
        i2csniffer_save_name_text_input_callback,
        i2c_save,
        i2c_save->text_store,
        I2CTOOLS_FILE_NAME_SIZE,
        file_name_is_empty);

    FuriString* folder_path;
    folder_path = furi_string_alloc();

    path_extract_dirname(furi_string_get_cstr(i2c_save->file_path), folder_path);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path),
        I2CTOOLS_APP_EXTENSION,
        furi_string_get_cstr(file_name));
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    furi_string_free(file_name);
    furi_string_free(folder_path);
}
