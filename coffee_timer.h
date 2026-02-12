#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define COFFEE_TIMER_TAG "CoffeeTimer"
#define SAVE_PATH APP_DATA_PATH("settings.bin")
#define CUSTOM_DIR APP_DATA_PATH("recipes")
#define MAX_STEPS 10
#define MAX_CUSTOM_RECIPES 4
#define NAME_LEN 20
#define DETAIL_LEN 28

// ============================================================
// Step types
// ============================================================
typedef enum {
    StepAdd,
    StepStir,
    StepWait,
    StepPress,
    StepFlip,
    StepPrep,
    StepPour,
    StepSwirl,
    StepTypeCount,
} StepType;

// ============================================================
// Built-in recipe structs (const, in flash)
// ============================================================
typedef struct {
    const char* instruction;
    const char* detail;
    uint16_t duration_sec;
    uint8_t weight_grams;
    uint8_t water_ml_div10;
    StepType type;
} BrewStep;

typedef struct {
    const char* name;
    const char* grind;
    const BrewStep* steps;
    uint16_t water_ml;
    uint16_t water_temp_c;
    uint8_t coffee_grams;
    uint8_t step_count;
} Recipe;

typedef struct {
    const char* name;
    const Recipe* recipes;
    uint8_t recipe_count;
} BrewMethod;

// ============================================================
// Custom recipe structs (mutable, in RAM, saved to SD)
// ============================================================
typedef struct {
    char instruction[NAME_LEN];
    char detail[DETAIL_LEN];
    uint16_t duration_sec;
    uint8_t weight_grams;
    uint8_t water_ml_div10;
    StepType type;
} CustomStep;

typedef struct {
    char name[NAME_LEN];
    char grind[16];
    CustomStep steps[MAX_STEPS];
    uint16_t water_ml;
    uint16_t water_temp_c;
    uint8_t coffee_grams;
    uint8_t step_count;
    char filename[32];
    bool loaded;
} CustomRecipe;

// ============================================================
// Grind presets
// ============================================================
#define GRIND_COUNT 6
extern const char* grind_names[GRIND_COUNT];

// ============================================================
// Screens
// ============================================================
typedef enum {
    ScreenMethodMenu,
    ScreenRecipeMenu,
    ScreenRecipeInfo,
    ScreenBrewing,
    ScreenComplete,
    ScreenConfirmAbort,
    ScreenEditMenu,
    ScreenEditRecipe,
    ScreenEditSteps,
    ScreenEditStep,
    ScreenConfirmDelete,
} Screen;

typedef enum {
    TimerStopped,
    TimerRunning,
    TimerPaused,
} TimerState;

// ============================================================
// Editor state
// ============================================================
typedef enum {
    EditFieldName,
    EditFieldGrind,
    EditFieldCoffee,
    EditFieldWater,
    EditFieldTemp,
    EditFieldSave,
    EditFieldDelete,
    EditFieldCount,
} EditField;

typedef enum {
    StepFieldType,
    StepFieldDuration,
    StepFieldWater,
    StepFieldWeight,
    StepFieldCount,
} StepField;

typedef struct {
    uint8_t recipe_idx;
    uint8_t step_idx;
    uint8_t sel;            // menu selection in edit screens
    EditField field;
    StepField step_field;
    uint8_t name_cursor;
    bool editing;           // actively changing a value
} EditorState;

// ============================================================
// Settings
// ============================================================
typedef struct {
    uint8_t last_method;
    uint8_t last_recipe;
    bool auto_advance;
    bool sound_on;
    bool led_on;
    uint8_t favourites[8];
    uint8_t fav_count;
} Settings;

// ============================================================
// App state
// ============================================================
typedef struct {
    uint32_t step_elapsed_ms;
    uint32_t total_elapsed_ms;
    uint16_t cumulative_water_ml;
    uint8_t method_sel;
    uint8_t recipe_sel;
    uint8_t cur_method;
    uint8_t cur_recipe;
    uint8_t cur_step;
    int8_t ratio_adjust;
    Screen screen;
    TimerState timer_state;
    bool step_complete;
    bool running;
    bool show_upcoming;
    bool using_custom;
} AppState;

// ============================================================
// App context
// ============================================================
typedef struct {
    AppState s;
    Settings settings;
    EditorState editor;
    CustomRecipe custom[MAX_CUSTOM_RECIPES];
    uint8_t custom_count;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    NotificationApp* notif;
} CoffeeApp;

// ============================================================
// Built-in data
// ============================================================
extern const BrewMethod methods[];
extern const uint8_t method_count;

// ============================================================
// Inline helpers
// ============================================================
static inline uint16_t step_water_ml(const BrewStep* st) {
    return (uint16_t)st->water_ml_div10 * 10;
}
static inline uint16_t custom_step_water_ml(const CustomStep* st) {
    return (uint16_t)st->water_ml_div10 * 10;
}

// ============================================================
// Settings (settings.c)
// ============================================================
void settings_load(CoffeeApp* app);
void settings_save(CoffeeApp* app);
bool settings_is_favourite(Settings* set, uint8_t method, uint8_t recipe);
void settings_toggle_favourite(Settings* set, uint8_t method, uint8_t recipe);
uint8_t adjusted_coffee(uint8_t base, int8_t adj);
uint16_t adjusted_water(uint16_t base_ml, uint8_t base_coffee, int8_t adj);

// ============================================================
// Custom recipes (custom.c)
// ============================================================
void custom_recipes_load(CoffeeApp* app);
bool custom_recipe_save(CoffeeApp* app, uint8_t idx);
bool custom_recipe_delete(CoffeeApp* app, uint8_t idx);
void custom_recipe_init_new(CustomRecipe* cr);
const char* step_type_name(StepType t);
void step_auto_detail(CustomStep* st);
