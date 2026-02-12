#include "coffee_timer.h"

void settings_load(CoffeeApp* app) {
    Settings* set = &app->settings;
    set->last_method = 0;
    set->last_recipe = 0;
    set->auto_advance = false;
    set->sound_on = true;
    set->led_on = true;
    set->fav_count = 0;
    memset(set->favourites, 0, sizeof(set->favourites));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, set, sizeof(Settings));
        if(set->last_method >= method_count) set->last_method = 0;
        if(set->last_method < method_count &&
           set->last_recipe >= methods[set->last_method].recipe_count)
            set->last_recipe = 0;
        if(set->fav_count > 8) set->fav_count = 0;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_save(CoffeeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->settings, sizeof(Settings));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool settings_is_favourite(Settings* set, uint8_t method, uint8_t recipe) {
    uint8_t packed = (method << 4) | (recipe & 0x0F);
    for(uint8_t i = 0; i < set->fav_count; i++) {
        if(set->favourites[i] == packed) return true;
    }
    return false;
}

void settings_toggle_favourite(Settings* set, uint8_t method, uint8_t recipe) {
    uint8_t packed = (method << 4) | (recipe & 0x0F);
    for(uint8_t i = 0; i < set->fav_count; i++) {
        if(set->favourites[i] == packed) {
            for(uint8_t j = i; j < set->fav_count - 1; j++)
                set->favourites[j] = set->favourites[j + 1];
            set->fav_count--;
            return;
        }
    }
    if(set->fav_count < 8) {
        set->favourites[set->fav_count] = packed;
        set->fav_count++;
    }
}

uint8_t adjusted_coffee(uint8_t base, int8_t adj) {
    int16_t val = (int16_t)base + adj;
    if(val < 1) val = 1;
    if(val > 200) val = 200;
    return (uint8_t)val;
}

uint16_t adjusted_water(uint16_t base_ml, uint8_t base_coffee, int8_t adj) {
    if(base_coffee == 0) return base_ml;
    float ratio = (float)base_ml / (float)base_coffee;
    int16_t coffee = (int16_t)base_coffee + adj;
    if(coffee < 1) coffee = 1;
    int16_t water = (int16_t)(ratio * (float)coffee + 0.5f);
    if(water < 10) water = 10;
    if(water > 2000) water = 2000;
    return (uint16_t)water;
}
