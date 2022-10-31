#include "../i2csniffer.h"
#include <lib/toolbox/random_name.h>
#include <toolbox/path.h>
#include <dolphin/dolphin.h>

#include <gui/modules/text_input.h>

#define I2CTOOLS_FILE_NAME_SIZE 100
#define I2CTOOLS_TEXT_STORE_SIZE 128

#define I2CTOOLS_APP_FOLDER ANY_PATH("i2ctools")
#define I2CTOOLS_APP_EXTENSION ".i2c"
#define I2CTOOLS_APP_FILE_TYPE "Flipper i2c record"

typedef struct {
    FuriString* file_path;
    TextInput* text_input;
    char text_store[I2CTOOLS_TEXT_STORE_SIZE + 1];
    i2cSniffer* i2c_data;
    bool saving;
    bool saved;
} i2cToolsSaver;

i2cToolsSaver* i2c_saver_alloc();
void i2c_saver_free(i2cToolsSaver* i2c_save);
void i2csniffer_save_name_on_enter(void* context);