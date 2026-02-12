#include "coffee_timer.h"
#include <stdlib.h>
#include <string.h>

// Portable case-insensitive compare
static int ci_cmp(const char* a, const char* b) {
    while(*a && *b) {
        char ca = *a, cb = *b;
        if(ca >= 'A' && ca <= 'Z') ca += 32;
        if(cb >= 'A' && cb <= 'Z') cb += 32;
        if(ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

const char* grind_names[GRIND_COUNT] = {
    "Extra Fine", "Fine", "Medium-Fine", "Medium", "Med-Coarse", "Coarse"
};

const char* step_type_name(StepType t) {
    switch(t) {
    case StepAdd:   return "Add";
    case StepStir:  return "Stir";
    case StepWait:  return "Wait";
    case StepPress: return "Press";
    case StepFlip:  return "Flip";
    case StepPrep:  return "Prep";
    case StepPour:  return "Pour";
    case StepSwirl: return "Swirl";
    default:        return "?";
    }
}

void step_auto_detail(CustomStep* st) {
    uint16_t wml = custom_step_water_ml(st);
    if(st->weight_grams > 0 && wml > 0)
        snprintf(st->detail, DETAIL_LEN, "%dg, %dml", st->weight_grams, wml);
    else if(st->weight_grams > 0)
        snprintf(st->detail, DETAIL_LEN, "%dg", st->weight_grams);
    else if(wml > 0)
        snprintf(st->detail, DETAIL_LEN, "%dml", wml);
    else if(st->duration_sec > 0)
        snprintf(st->detail, DETAIL_LEN, "%ds", st->duration_sec);
    else
        snprintf(st->detail, DETAIL_LEN, "Manual step");
}

void custom_recipe_init_new(CustomRecipe* cr) {
    memset(cr, 0, sizeof(CustomRecipe));
    snprintf(cr->name, NAME_LEN, "My Recipe");
    snprintf(cr->grind, sizeof(cr->grind), "Medium");
    cr->coffee_grams = 15;
    cr->water_ml = 250;
    cr->water_temp_c = 93;
    cr->step_count = 0;
    cr->loaded = true;
    cr->filename[0] = 0;
}

// ============================================================
// Parse step type from string
// ============================================================
static StepType parse_step_type(const char* s) {
    if(ci_cmp(s, "ADD") == 0) return StepAdd;
    if(ci_cmp(s, "STIR") == 0) return StepStir;
    if(ci_cmp(s, "WAIT") == 0) return StepWait;
    if(ci_cmp(s, "PRESS") == 0) return StepPress;
    if(ci_cmp(s, "FLIP") == 0) return StepFlip;
    if(ci_cmp(s, "PREP") == 0) return StepPrep;
    if(ci_cmp(s, "POUR") == 0) return StepPour;
    if(ci_cmp(s, "SWIRL") == 0) return StepSwirl;
    return StepPrep;
}

// ============================================================
// Parse a single .brew file into a CustomRecipe
// ============================================================
static bool parse_brew_file(Storage* storage, const char* path, CustomRecipe* cr) {
    memset(cr, 0, sizeof(CustomRecipe));

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    // Heap-allocate read buffer to avoid stack overflow
    const uint16_t buf_size = 1024;
    char* buf = malloc(buf_size);
    if(!buf) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    uint16_t read = storage_file_read(file, buf, (uint16_t)(buf_size - 1));
    buf[read] = 0;
    storage_file_close(file);
    storage_file_free(file);

    bool in_steps = false;
    char* line = buf;

    while(line && *line) {
        // Find end of line
        char* nl = strchr(line, '\n');
        if(nl) *nl = 0;

        // Trim trailing \r
        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\r') line[len - 1] = 0;

        if(strlen(line) == 0) {
            line = nl ? nl + 1 : NULL;
            continue;
        }

        if(strncmp(line, "---", 3) == 0) {
            in_steps = true;
            line = nl ? nl + 1 : NULL;
            continue;
        }

        if(!in_steps) {
            // Parse header: key=value
            char* eq = strchr(line, '=');
            if(eq) {
                *eq = 0;
                char* val = eq + 1;
                if(ci_cmp(line, "name") == 0) {
                    strncpy(cr->name, val, NAME_LEN - 1);
                    cr->name[NAME_LEN - 1] = 0;
                } else if(ci_cmp(line, "grind") == 0) {
                    strncpy(cr->grind, val, sizeof(cr->grind) - 1);
                    cr->grind[sizeof(cr->grind) - 1] = 0;
                } else if(ci_cmp(line, "coffee") == 0)
                    cr->coffee_grams = (uint8_t)atoi(val);
                else if(ci_cmp(line, "water") == 0)
                    cr->water_ml = (uint16_t)atoi(val);
                else if(ci_cmp(line, "temp") == 0)
                    cr->water_temp_c = (uint16_t)atoi(val);
            }
        } else {
            // Parse step: TYPE|instruction|detail|duration|weight|water_ml
            if(cr->step_count >= MAX_STEPS) {
                line = nl ? nl + 1 : NULL;
                continue;
            }
            CustomStep* st = &cr->steps[cr->step_count];

            char* tok = line;
            char* pipe;
            int field = 0;

            while(tok && field < 6) {
                pipe = strchr(tok, '|');
                if(pipe) *pipe = 0;

                switch(field) {
                case 0: st->type = parse_step_type(tok); break;
                case 1:
                    strncpy(st->instruction, tok, NAME_LEN - 1);
                    st->instruction[NAME_LEN - 1] = 0;
                    break;
                case 2:
                    strncpy(st->detail, tok, DETAIL_LEN - 1);
                    st->detail[DETAIL_LEN - 1] = 0;
                    break;
                case 3: st->duration_sec = (uint16_t)atoi(tok); break;
                case 4: st->weight_grams = (uint8_t)atoi(tok); break;
                case 5: {
                    int ml = atoi(tok);
                    st->water_ml_div10 = (uint8_t)(ml / 10);
                    break;
                }
                }

                tok = pipe ? pipe + 1 : NULL;
                field++;
            }

            cr->step_count++;
        }

        line = nl ? nl + 1 : NULL;
    }

    cr->loaded = (cr->step_count > 0 && cr->name[0] != 0);
    free(buf);
    return cr->loaded;
}

// ============================================================
// Load all .brew files from custom dir
// ============================================================
void custom_recipes_load(CoffeeApp* app) {
    app->custom_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, CUSTOM_DIR);

    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, CUSTOM_DIR)) {
        storage_dir_close(dir);
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo info;
    char name[64];

    while(app->custom_count < MAX_CUSTOM_RECIPES &&
          storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
        // Check for .brew extension
        size_t nlen = strlen(name);
        if(nlen < 6) continue;
        if(ci_cmp(name + nlen - 5, ".brew") != 0) continue;

        char path[128];
        snprintf(path, sizeof(path), "%s/%s", CUSTOM_DIR, name);

        CustomRecipe* cr = &app->custom[app->custom_count];
        if(parse_brew_file(storage, path, cr)) {
            strncpy(cr->filename, name, sizeof(cr->filename) - 1);
            cr->filename[sizeof(cr->filename) - 1] = 0;
            app->custom_count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

// ============================================================
// Save a custom recipe to .brew file
// ============================================================
bool custom_recipe_save(CoffeeApp* app, uint8_t idx) {
    if(idx >= MAX_CUSTOM_RECIPES) return false;
    CustomRecipe* cr = &app->custom[idx];

    // Generate filename from name if not set
    if(cr->filename[0] == 0) {
        // Simple: use index
        snprintf(cr->filename, sizeof(cr->filename), "custom_%d.brew", idx);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, CUSTOM_DIR);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", CUSTOM_DIR, cr->filename);

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[128];
        uint16_t len;

        // Header
        len = (uint16_t)snprintf(line, sizeof(line), "name=%s\n", cr->name);
        storage_file_write(file, line, (uint16_t)len);
        len = (uint16_t)snprintf(line, sizeof(line), "grind=%s\n", cr->grind);
        storage_file_write(file, line, (uint16_t)len);
        len = (uint16_t)snprintf(line, sizeof(line), "coffee=%d\n", cr->coffee_grams);
        storage_file_write(file, line, (uint16_t)len);
        len = (uint16_t)snprintf(line, sizeof(line), "water=%d\n", cr->water_ml);
        storage_file_write(file, line, (uint16_t)len);
        len = (uint16_t)snprintf(line, sizeof(line), "temp=%d\n", cr->water_temp_c);
        storage_file_write(file, line, (uint16_t)len);

        // Separator
        storage_file_write(file, "---\n", 4);

        // Steps
        for(uint8_t i = 0; i < cr->step_count; i++) {
            CustomStep* st = &cr->steps[i];
            const char* tname;
            switch(st->type) {
            case StepAdd:   tname = "ADD"; break;
            case StepStir:  tname = "STIR"; break;
            case StepWait:  tname = "WAIT"; break;
            case StepPress: tname = "PRESS"; break;
            case StepFlip:  tname = "FLIP"; break;
            case StepPrep:  tname = "PREP"; break;
            case StepPour:  tname = "POUR"; break;
            case StepSwirl: tname = "SWIRL"; break;
            default:        tname = "PREP"; break;
            }
            uint16_t wml = custom_step_water_ml(st);
            len = (uint16_t)snprintf(line, sizeof(line), "%s|%s|%s|%d|%d|%d\n",
                tname, st->instruction, st->detail,
                st->duration_sec, st->weight_grams, wml);
            storage_file_write(file, line, (uint16_t)len);
        }

        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ============================================================
// Delete a custom recipe
// ============================================================
bool custom_recipe_delete(CoffeeApp* app, uint8_t idx) {
    if(idx >= app->custom_count) return false;
    CustomRecipe* cr = &app->custom[idx];

    if(cr->filename[0] != 0) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", CUSTOM_DIR, cr->filename);
        storage_simply_remove(storage, path);
        furi_record_close(RECORD_STORAGE);
    }

    // Shift remaining
    for(uint8_t i = idx; i < app->custom_count - 1; i++) {
        memcpy(&app->custom[i], &app->custom[i + 1], sizeof(CustomRecipe));
    }
    app->custom_count--;
    return true;
}
