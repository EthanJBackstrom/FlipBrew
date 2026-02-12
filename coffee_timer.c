#include "coffee_timer.h"
#include <furi_hal.h>
#include <string.h>
#include <coffee_timer_icons.h>

// ============================================================
// LED / sound sequences
// ============================================================
static const NotificationSequence seq_led_green = {
    &message_green_255, &message_delay_100, &message_green_0, NULL,
};
static const NotificationSequence seq_led_orange = {
    &message_red_255, &message_green_255, &message_delay_100,
    &message_red_0, &message_green_0, NULL,
};
static const NotificationSequence seq_led_red_blink = {
    &message_red_255, &message_delay_50, &message_red_0,
    &message_delay_50, &message_red_255, &message_delay_50, &message_red_0, NULL,
};
static const NotificationSequence seq_beep = {
    &message_note_c7, &message_delay_50, &message_sound_off, NULL,
};
static const NotificationSequence seq_beep_done = {
    &message_note_c7, &message_delay_100, &message_sound_off,
    &message_delay_50,
    &message_note_e7, &message_delay_100, &message_sound_off, NULL,
};

// ============================================================
// Helpers
// ============================================================
static void fmt_time(uint32_t sec, char* b, size_t n) {
    snprintf(b, n, "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

static const char* step_badge(StepType t) {
    switch(t) {
    case StepAdd:   return "ADD";
    case StepStir:  return "STIR";
    case StepWait:  return "WAIT";
    case StepPress: return "PRESS";
    case StepFlip:  return "FLIP";
    case StepPrep:  return "PREP";
    case StepPour:  return "POUR";
    case StepSwirl: return "SWIRL";
    default:        return "?";
    }
}

// ============================================================
// Unified recipe accessors
// ============================================================
static const char* get_rname(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].name :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].name;
}
static const char* get_rgrind(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].grind :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].grind;
}
static uint8_t get_rcoffee(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].coffee_grams :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].coffee_grams;
}
static uint16_t get_rwater(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].water_ml :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].water_ml;
}
static uint16_t get_rtemp(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].water_temp_c :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].water_temp_c;
}
static uint8_t get_scount(CoffeeApp* a) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].step_count :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].step_count;
}
static const char* get_sinst(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].steps[i].instruction :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i].instruction;
}
static const char* get_sdetail(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].steps[i].detail :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i].detail;
}
static uint16_t get_sdur(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].steps[i].duration_sec :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i].duration_sec;
}
static StepType get_stype(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].steps[i].type :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i].type;
}
static uint8_t get_sweight(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ? a->custom[a->s.cur_recipe].steps[i].weight_grams :
        methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i].weight_grams;
}
static uint16_t get_swml(CoffeeApp* a, uint8_t i) {
    return a->s.using_custom ?
        custom_step_water_ml(&a->custom[a->s.cur_recipe].steps[i]) :
        step_water_ml(&methods[a->s.cur_method].recipes[a->s.cur_recipe].steps[i]);
}

// ============================================================
// Notifications
// ============================================================
static void nfy_step_done(CoffeeApp* a) {
    notification_message(a->notif, &sequence_single_vibro);
    if(a->settings.sound_on) notification_message(a->notif, &seq_beep);
    if(a->settings.led_on) notification_message(a->notif, &seq_led_red_blink);
}
static void nfy_brew_done(CoffeeApp* a) {
    notification_message(a->notif, &sequence_double_vibro);
    if(a->settings.sound_on) notification_message(a->notif, &seq_beep_done);
    if(a->settings.led_on) notification_message(a->notif, &seq_led_red_blink);
}
static void nfy_step_chg(CoffeeApp* a) {
    notification_message(a->notif, &sequence_single_vibro);
    if(a->settings.led_on) notification_message(a->notif, &seq_led_green);
}

// ============================================================
// Draw: Method menu
// ============================================================
static void draw_method_menu(Canvas* c, AppState* s, CoffeeApp* app) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "Coffee Timer");
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    uint8_t total = method_count + 1;
    uint8_t vs = 0;
    if(s->method_sel > 2) vs = s->method_sel - 2;
    if(total > 4 && vs + 4 > total) vs = total - 4;

    char lb[36];
    for(uint8_t i = 0; i < 4 && (vs + i) < total; i++) {
        uint8_t idx = vs + i;
        uint8_t y = 17 + (i * 10);
        if(idx == s->method_sel) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
        }
        if(idx < method_count)
            snprintf(lb, sizeof(lb), "%s (%d)", methods[idx].name, methods[idx].recipe_count);
        else
            snprintf(lb, sizeof(lb), "Custom (%d) [>]Edit", app->custom_count);
        canvas_draw_str(c, 4, y + 7, lb);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[OK]Select [<]Exit");
}

// ============================================================
// Draw: Recipe menu
// ============================================================
static void draw_recipe_menu(Canvas* c, AppState* s, CoffeeApp* app) {
    bool is_cust = (s->method_sel >= method_count);
    const char* title = is_cust ? "Custom" : methods[s->method_sel].name;
    uint8_t count = is_cust ? app->custom_count : methods[s->method_sel].recipe_count;

    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, title);
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    if(count == 0) {
        canvas_draw_str_aligned(c, 64, 35, AlignCenter, AlignBottom, "No recipes yet");
        canvas_draw_line(c, 0, 56, 127, 56);
        canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[<] Back");
        return;
    }

    uint8_t vs = 0;
    if(s->recipe_sel > 2) vs = s->recipe_sel - 2;
    if(count > 4 && vs + 4 > count) vs = count - 4;

    char lb[36];
    for(uint8_t i = 0; i < 4 && (vs + i) < count; i++) {
        uint8_t idx = vs + i;
        uint8_t y = 17 + (i * 10);
        if(idx == s->recipe_sel) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
        }
        if(is_cust) {
            snprintf(lb, sizeof(lb), "  %s %dg", app->custom[idx].name, app->custom[idx].coffee_grams);
        } else {
            const Recipe* r = &methods[s->method_sel].recipes[idx];
            bool fav = settings_is_favourite(&app->settings, s->method_sel, idx);
            snprintf(lb, sizeof(lb), "%s %s %dg/%dml",
                fav ? "*" : " ", r->name, r->coffee_grams, r->water_ml);
        }
        canvas_draw_str(c, 2, y + 7, lb);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom,
        is_cust ? "[OK]Brew [<]Back" : "[OK]Sel [>]Fav [<]Back");
}

// ============================================================
// Draw: Recipe info
// ============================================================
static void draw_info(Canvas* c, CoffeeApp* app) {
    AppState* s = &app->s;
    char b[36];
    uint8_t cof = adjusted_coffee(get_rcoffee(app), s->ratio_adjust);
    uint16_t wat = adjusted_water(get_rwater(app), get_rcoffee(app), s->ratio_adjust);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, get_rname(app));
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    snprintf(b, sizeof(b), "%s%s%s",
        app->settings.auto_advance ? "Auto " : "",
        app->settings.sound_on ? "Snd " : "",
        app->settings.led_on ? "LED" : "");
    canvas_draw_str_aligned(c, 126, 15, AlignRight, AlignTop, b);

    snprintf(b, sizeof(b), "Coffee: %dg (%s)", cof, get_rgrind(app));
    canvas_draw_str(c, 2, 26, b);
    snprintf(b, sizeof(b), "Water: %dml at %dC", wat, get_rtemp(app));
    canvas_draw_str(c, 2, 36, b);
    if(s->ratio_adjust != 0)
        snprintf(b, sizeof(b), "Adjusted: %+d dose", s->ratio_adjust);
    else
        snprintf(b, sizeof(b), "Steps: %d  [</>]dose", get_scount(app));
    canvas_draw_str(c, 2, 46, b);

    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[OK]Start [Up]Opts [<]Back");
}

// ============================================================
// Draw: Brewing
// ============================================================
static void draw_brewing(Canvas* c, CoffeeApp* app) {
    AppState* s = &app->s;
    uint8_t sc = get_scount(app);
    char b[36];

    canvas_set_font(c, FontSecondary);
    snprintf(b, sizeof(b), "%d/%d", s->cur_step + 1, sc);
    canvas_draw_str(c, 2, 8, b);

    const char* bg = step_badge(get_stype(app, s->cur_step));
    uint8_t bw = canvas_string_width(c, bg) + 6;
    canvas_draw_rframe(c, 128 - bw - 2, 0, bw, 11, 2);
    canvas_draw_str_aligned(c, 128 - bw / 2 - 2, 8, AlignCenter, AlignBottom, bg);
    canvas_draw_str_aligned(c, 64, 8, AlignCenter, AlignBottom, get_rname(app));
    canvas_draw_line(c, 0, 10, 127, 10);

    if(s->show_upcoming) {
        canvas_draw_str_aligned(c, 64, 13, AlignCenter, AlignTop, "Upcoming:");
        for(uint8_t i = 0; i < 3; i++) {
            uint8_t si = s->cur_step + 1 + i;
            if(si >= sc) break;
            snprintf(b, sizeof(b), "%d. %s", si + 1, get_sinst(app, si));
            canvas_draw_str(c, 4, 24 + (i * 10), b);
        }
        snprintf(b, sizeof(b), "Water: %dml", s->cumulative_water_ml);
        canvas_draw_str(c, 2, 54, b);
    } else {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignBottom, get_sinst(app, s->cur_step));
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignBottom, get_sdetail(app, s->cur_step));

        uint16_t wml = get_swml(app, s->cur_step);
        uint8_t wg = get_sweight(app, s->cur_step);
        if(wg > 0 || wml > 0) {
            char ex[24];
            if(wg > 0 && wml > 0) snprintf(ex, sizeof(ex), "%dg / %dml", wg, wml);
            else if(wg > 0) snprintf(ex, sizeof(ex), "%dg", wg);
            else snprintf(ex, sizeof(ex), "%dml", wml);
            canvas_draw_str_aligned(c, 64, 40, AlignCenter, AlignBottom, ex);
        }
        canvas_draw_line(c, 0, 42, 127, 42);

        uint16_t dur = get_sdur(app, s->cur_step);
        if(dur > 0) {
            uint32_t el = s->step_elapsed_ms / 1000;
            canvas_set_font(c, FontBigNumbers);
            char ts[12];
            if(el < dur) fmt_time(dur - el, ts, sizeof(ts));
            else fmt_time(el - dur, ts, sizeof(ts));
            canvas_draw_str_aligned(c, 45, 44, AlignCenter, AlignTop, ts);
            if(el >= dur) { canvas_set_font(c, FontSecondary); canvas_draw_str(c, 14, 48, "+"); }

            canvas_draw_rframe(c, 70, 45, 54, 8, 2);
            float prog = (float)el / (float)dur;
            if(prog > 1.0f) prog = 1.0f;
            uint8_t fw = (uint8_t)(prog * 52.0f);
            if(fw > 0) canvas_draw_box(c, 71, 46, fw, 6);

            canvas_set_font(c, FontSecondary);
            if(s->timer_state == TimerPaused)
                canvas_draw_str_aligned(c, 97, 62, AlignCenter, AlignBottom, "PAUSED");
            else if(s->step_complete)
                canvas_draw_str_aligned(c, 97, 62, AlignCenter, AlignBottom, "DONE>");
        } else {
            canvas_set_font(c, FontSecondary);
            canvas_draw_str_aligned(c, 64, 52, AlignCenter, AlignBottom, "[OK] when ready");
        }
    }

    canvas_set_font(c, FontSecondary);
    char tt[12]; fmt_time(s->total_elapsed_ms / 1000, tt, sizeof(tt));
    snprintf(b, sizeof(b), "T:%s", tt);
    canvas_draw_str(c, 2, 62, b);
}

static void draw_complete(Canvas* c, CoffeeApp* app) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignBottom, "Brew Complete!");
    canvas_set_font(c, FontSecondary);
    char tb[12]; fmt_time(app->s.total_elapsed_ms / 1000, tb, sizeof(tb));
    char b[32]; snprintf(b, sizeof(b), "Total: %s", tb);
    canvas_draw_str_aligned(c, 64, 28, AlignCenter, AlignBottom, b);
    canvas_draw_str_aligned(c, 64, 40, AlignCenter, AlignBottom, get_rname(app));
    canvas_draw_str_aligned(c, 64, 50, AlignCenter, AlignBottom, "Enjoy your coffee!");
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[OK] Menu  [<] Exit");
}

static void draw_confirm_box(Canvas* c, const char* msg) {
    canvas_draw_rframe(c, 14, 16, 100, 32, 4);
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 15, 17, 98, 30);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 28, AlignCenter, AlignBottom, msg);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 42, AlignCenter, AlignBottom, "[OK] Yes  [<] No");
}

// ============================================================
// Editor draw functions
// ============================================================
static void draw_edit_menu(Canvas* c, CoffeeApp* app) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "Custom Recipes");
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    uint8_t total = app->custom_count + 1;
    uint8_t sel = app->editor.sel;
    uint8_t vs = 0;
    if(sel > 2) vs = sel - 2;
    if(total > 4 && vs + 4 > total) vs = total - 4;

    char lb[36];
    for(uint8_t i = 0; i < 4 && (vs + i) < total; i++) {
        uint8_t idx = vs + i;
        uint8_t y = 17 + (i * 10);
        if(idx == sel) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else { canvas_set_color(c, ColorBlack); }
        snprintf(lb, sizeof(lb), "%s",
            idx < app->custom_count ? app->custom[idx].name : "+ New Recipe");
        canvas_draw_str(c, 4, y + 7, lb);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[OK]Edit [<]Back");
}

static void draw_edit_recipe(Canvas* c, CoffeeApp* app) {
    EditorState* ed = &app->editor;
    CustomRecipe* cr = &app->custom[ed->recipe_idx];
    char b[36];

    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "Edit Recipe");
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    const char* labels[] = {"Name:", "Grind:", "Coffee:", "Water:", "Temp:", ">> SAVE", ">> DELETE"};

    uint8_t vs = 0;
    if((uint8_t)ed->field > 3) vs = (uint8_t)ed->field - 3;

    for(uint8_t i = 0; i < 4 && (vs + i) < EditFieldCount; i++) {
        uint8_t fi = vs + i;
        uint8_t y = 17 + (i * 10);
        if(fi == (uint8_t)ed->field) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else { canvas_set_color(c, ColorBlack); }

        switch(fi) {
        case EditFieldName:
            snprintf(b, sizeof(b), ed->editing ? "%s %s_" : "%s %s", labels[fi], cr->name);
            break;
        case EditFieldGrind: snprintf(b, sizeof(b), "%s %s", labels[fi], cr->grind); break;
        case EditFieldCoffee: snprintf(b, sizeof(b), "%s %dg", labels[fi], cr->coffee_grams); break;
        case EditFieldWater: snprintf(b, sizeof(b), "%s %dml", labels[fi], cr->water_ml); break;
        case EditFieldTemp: snprintf(b, sizeof(b), "%s %dC", labels[fi], cr->water_temp_c); break;
        default: snprintf(b, sizeof(b), "%s", labels[fi]); break;
        }
        canvas_draw_str(c, 4, y + 7, b);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom,
        ed->editing ? "[</>]Adj [OK]Done" : "[OK]Edit [>]Steps [<]Back");
}

static void draw_edit_steps(Canvas* c, CoffeeApp* app) {
    EditorState* ed = &app->editor;
    CustomRecipe* cr = &app->custom[ed->recipe_idx];
    char b[36];

    canvas_set_font(c, FontPrimary);
    snprintf(b, sizeof(b), "Steps (%d/%d)", cr->step_count, MAX_STEPS);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, b);
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    uint8_t total = cr->step_count + (cr->step_count < MAX_STEPS ? 1 : 0);
    uint8_t sel = ed->sel;
    uint8_t vs = 0;
    if(sel > 2) vs = sel - 2;
    if(total > 4 && vs + 4 > total) vs = total - 4;

    for(uint8_t i = 0; i < 4 && (vs + i) < total; i++) {
        uint8_t idx = vs + i;
        uint8_t y = 17 + (i * 10);
        if(idx == sel) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else { canvas_set_color(c, ColorBlack); }
        if(idx < cr->step_count)
            snprintf(b, sizeof(b), "%d.%s %ds", idx + 1, step_type_name(cr->steps[idx].type), cr->steps[idx].duration_sec);
        else
            snprintf(b, sizeof(b), "+ Add Step");
        canvas_draw_str(c, 4, y + 7, b);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[OK]Edit [>]Del [<]Back");
}

static void draw_edit_step(Canvas* c, CoffeeApp* app) {
    EditorState* ed = &app->editor;
    CustomStep* st = &app->custom[ed->recipe_idx].steps[ed->step_idx];
    char b[36];

    canvas_set_font(c, FontPrimary);
    snprintf(b, sizeof(b), "Step %d", ed->step_idx + 1);
    canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, b);
    canvas_draw_line(c, 0, 13, 127, 13);
    canvas_set_font(c, FontSecondary);

    for(uint8_t i = 0; i < StepFieldCount; i++) {
        uint8_t y = 17 + (i * 10);
        if(i == (uint8_t)ed->step_field) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 1, 128, 11);
            canvas_set_color(c, ColorWhite);
        } else { canvas_set_color(c, ColorBlack); }
        switch(i) {
        case StepFieldType: snprintf(b, sizeof(b), "Type: %s", step_type_name(st->type)); break;
        case StepFieldDuration: snprintf(b, sizeof(b), "Time: %ds", st->duration_sec); break;
        case StepFieldWater: snprintf(b, sizeof(b), "Water: %dml", custom_step_water_ml(st)); break;
        case StepFieldWeight: snprintf(b, sizeof(b), "Weight: %dg", st->weight_grams); break;
        }
        canvas_draw_str(c, 4, y + 7, b);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, 56, 127, 56);
    canvas_draw_str_aligned(c, 64, 63, AlignCenter, AlignBottom, "[</>]Adj [OK/Bk]Done");
}

// ============================================================
// Main draw callback
// ============================================================
static void draw_cb(Canvas* c, void* ctx) {
    CoffeeApp* app = ctx;
    if(furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;
    canvas_clear(c);
    switch(app->s.screen) {
    case ScreenMethodMenu:    draw_method_menu(c, &app->s, app); break;
    case ScreenRecipeMenu:    draw_recipe_menu(c, &app->s, app); break;
    case ScreenRecipeInfo:    draw_info(c, app); break;
    case ScreenBrewing:       draw_brewing(c, app); break;
    case ScreenComplete:      draw_complete(c, app); break;
    case ScreenConfirmAbort:  draw_brewing(c, app); draw_confirm_box(c, "Cancel brew?"); break;
    case ScreenEditMenu:      draw_edit_menu(c, app); break;
    case ScreenEditRecipe:    draw_edit_recipe(c, app); break;
    case ScreenEditSteps:     draw_edit_steps(c, app); break;
    case ScreenEditStep:      draw_edit_step(c, app); break;
    case ScreenConfirmDelete: draw_edit_recipe(c, app); draw_confirm_box(c, "Delete recipe?"); break;
    }
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* ev, void* ctx) {
    CoffeeApp* app = ctx;
    furi_message_queue_put(app->queue, ev, FuriWaitForever);
}

static void tick_cb(void* ctx) {
    CoffeeApp* app = ctx;
    if(furi_mutex_acquire(app->mutex, 10) != FuriStatusOk) return;
    if(app->s.screen == ScreenBrewing && app->s.timer_state == TimerRunning) {
        app->s.step_elapsed_ms += 100;
        app->s.total_elapsed_ms += 100;
        uint16_t dur = get_sdur(app, app->s.cur_step);
        if(dur > 0 && app->s.step_elapsed_ms / 1000 >= dur && !app->s.step_complete) {
            app->s.step_complete = true;
            nfy_step_done(app);
        }
        if(app->s.step_elapsed_ms % 5000 == 0 && app->settings.led_on &&
           app->s.timer_state == TimerPaused)
            notification_message(app->notif, &seq_led_orange);
        view_port_update(app->view_port);
    }
    furi_mutex_release(app->mutex);
}

// ============================================================
// Step advance
// ============================================================
static void advance(CoffeeApp* app) {
    uint8_t sc = get_scount(app);
    app->s.cumulative_water_ml += get_swml(app, app->s.cur_step);

    if(app->s.cur_step + 1 >= sc) {
        app->s.screen = ScreenComplete;
        app->s.timer_state = TimerStopped;
        nfy_brew_done(app);
    } else {
        app->s.cur_step++;
        app->s.step_elapsed_ms = 0;
        app->s.step_complete = false;
        app->s.timer_state = (get_sdur(app, app->s.cur_step) > 0) ? TimerRunning : TimerStopped;
        nfy_step_chg(app);
    }
}

static void check_auto_advance(CoffeeApp* app) {
    if(!app->settings.auto_advance || !app->s.step_complete || app->s.screen != ScreenBrewing) return;
    uint8_t sc = get_scount(app);
    if(app->s.cur_step + 1 < sc && get_sdur(app, app->s.cur_step + 1) > 0) {
        advance(app);
    }
}

// ============================================================
// Input: editor screens
// ============================================================
static void handle_edit_menu(CoffeeApp* app, InputEvent* ev) {
    uint8_t total = app->custom_count + 1;
    if(ev->key == InputKeyUp) {
        app->editor.sel = (app->editor.sel == 0) ? total - 1 : app->editor.sel - 1;
    } else if(ev->key == InputKeyDown) {
        app->editor.sel = (app->editor.sel >= total - 1) ? 0 : app->editor.sel + 1;
    } else if(ev->key == InputKeyOk) {
        if(app->editor.sel >= app->custom_count) {
            // New recipe
            if(app->custom_count < MAX_CUSTOM_RECIPES) {
                custom_recipe_init_new(&app->custom[app->custom_count]);
                app->editor.recipe_idx = app->custom_count;
                app->custom_count++;
                app->editor.field = EditFieldName;
                app->editor.editing = false;
                app->s.screen = ScreenEditRecipe;
            }
        } else {
            app->editor.recipe_idx = app->editor.sel;
            app->editor.field = EditFieldName;
            app->editor.editing = false;
            app->s.screen = ScreenEditRecipe;
        }
    } else if(ev->key == InputKeyBack) {
        app->s.screen = ScreenMethodMenu;
    }
}

static void handle_edit_recipe(CoffeeApp* app, InputEvent* ev) {
    EditorState* ed = &app->editor;
    CustomRecipe* cr = &app->custom[ed->recipe_idx];

    if(ed->editing) {
        // Editing a value
        if(ev->key == InputKeyOk || ev->key == InputKeyBack) {
            ed->editing = false;
            return;
        }
        int dir = (ev->key == InputKeyRight) ? 1 : (ev->key == InputKeyLeft) ? -1 : 0;
        if(dir == 0) return;

        switch(ed->field) {
        case EditFieldName: {
            // Cycle character at cursor
            size_t len = strlen(cr->name);
            if(len == 0) { cr->name[0] = 'A'; cr->name[1] = 0; break; }
            uint8_t pos = ed->name_cursor;
            if(pos >= (uint8_t)len) pos = (uint8_t)(len - 1);
            char ch = cr->name[pos];
            ch += (char)dir;
            if(ch < ' ') ch = 'z';
            if(ch > 'z') ch = ' ';
            cr->name[pos] = ch;
            break;
        }
        case EditFieldGrind: {
            // Cycle through presets
            uint8_t gi = 0;
            for(uint8_t i = 0; i < GRIND_COUNT; i++) {
                if(strcmp(cr->grind, grind_names[i]) == 0) { gi = i; break; }
            }
            gi = (uint8_t)((int)gi + dir);
            if(gi >= GRIND_COUNT) gi = (dir > 0) ? 0 : GRIND_COUNT - 1;
            snprintf(cr->grind, sizeof(cr->grind), "%s", grind_names[gi]);
            break;
        }
        case EditFieldCoffee: {
            int16_t v = (int16_t)cr->coffee_grams + dir;
            if(v < 1) v = 1;
            if(v > 200) v = 200;
            cr->coffee_grams = (uint8_t)v;
            break;
        }
        case EditFieldWater: {
            int16_t v = (int16_t)cr->water_ml + dir * 10;
            if(v < 10) v = 10;
            if(v > 2000) v = 2000;
            cr->water_ml = (uint16_t)v;
            break;
        }
        case EditFieldTemp: {
            int16_t v = (int16_t)cr->water_temp_c + dir;
            if(v < 0) v = 0;
            if(v > 100) v = 100;
            cr->water_temp_c = (uint16_t)v;
            break;
        }
        default: break;
        }
        return;
    }

    // Not editing - navigate fields
    if(ev->key == InputKeyUp) {
        ed->field = (ed->field == 0) ? EditFieldCount - 1 : ed->field - 1;
    } else if(ev->key == InputKeyDown) {
        ed->field = (ed->field >= EditFieldCount - 1) ? 0 : ed->field + 1;
    } else if(ev->key == InputKeyOk) {
        if(ed->field == EditFieldSave) {
            // Auto-generate instructions
            for(uint8_t i = 0; i < cr->step_count; i++) {
                snprintf(cr->steps[i].instruction, NAME_LEN, "%s", step_type_name(cr->steps[i].type));
                step_auto_detail(&cr->steps[i]);
            }
            custom_recipe_save(app, ed->recipe_idx);
            app->s.screen = ScreenEditMenu;
        } else if(ed->field == EditFieldDelete) {
            app->s.screen = ScreenConfirmDelete;
        } else {
            ed->editing = true;
            if(ed->field == EditFieldName) {
                size_t nlen = strlen(cr->name);
                ed->name_cursor = (nlen > 0) ? (uint8_t)(nlen - 1) : 0;
            }
        }
    } else if(ev->key == InputKeyRight) {
        // Go to steps editor
        ed->sel = 0;
        app->s.screen = ScreenEditSteps;
    } else if(ev->key == InputKeyBack) {
        app->s.screen = ScreenEditMenu;
    }
}

static void handle_edit_steps(CoffeeApp* app, InputEvent* ev) {
    EditorState* ed = &app->editor;
    CustomRecipe* cr = &app->custom[ed->recipe_idx];
    uint8_t total = cr->step_count + (cr->step_count < MAX_STEPS ? 1 : 0);

    if(ev->key == InputKeyUp) {
        ed->sel = (ed->sel == 0) ? total - 1 : ed->sel - 1;
    } else if(ev->key == InputKeyDown) {
        ed->sel = (ed->sel >= total - 1) ? 0 : ed->sel + 1;
    } else if(ev->key == InputKeyOk) {
        if(ed->sel >= cr->step_count) {
            // Add new step
            if(cr->step_count < MAX_STEPS) {
                CustomStep* st = &cr->steps[cr->step_count];
                memset(st, 0, sizeof(CustomStep));
                st->type = StepWait;
                st->duration_sec = 30;
                snprintf(st->instruction, NAME_LEN, "Wait");
                snprintf(st->detail, DETAIL_LEN, "30s");
                ed->step_idx = cr->step_count;
                cr->step_count++;
                ed->step_field = StepFieldType;
                app->s.screen = ScreenEditStep;
            }
        } else {
            ed->step_idx = ed->sel;
            ed->step_field = StepFieldType;
            app->s.screen = ScreenEditStep;
        }
    } else if(ev->key == InputKeyRight) {
        // Delete step
        if(ed->sel < cr->step_count && cr->step_count > 0) {
            for(uint8_t i = ed->sel; i < cr->step_count - 1; i++)
                memcpy(&cr->steps[i], &cr->steps[i + 1], sizeof(CustomStep));
            cr->step_count--;
            if(ed->sel >= cr->step_count && ed->sel > 0) ed->sel--;
        }
    } else if(ev->key == InputKeyBack) {
        app->s.screen = ScreenEditRecipe;
    }
}

static void handle_edit_step(CoffeeApp* app, InputEvent* ev) {
    EditorState* ed = &app->editor;
    CustomStep* st = &app->custom[ed->recipe_idx].steps[ed->step_idx];

    if(ev->key == InputKeyUp) {
        ed->step_field = (ed->step_field == 0) ? StepFieldCount - 1 : ed->step_field - 1;
    } else if(ev->key == InputKeyDown) {
        ed->step_field = (ed->step_field >= StepFieldCount - 1) ? 0 : ed->step_field + 1;
    } else if(ev->key == InputKeyLeft || ev->key == InputKeyRight) {
        int dir = (ev->key == InputKeyRight) ? 1 : -1;
        switch(ed->step_field) {
        case StepFieldType: {
            int t = (int)st->type + dir;
            if(t < 0) t = (int)StepSwirl;
            if(t > (int)StepSwirl) t = 0;
            st->type = (StepType)t;
            snprintf(st->instruction, NAME_LEN, "%s", step_type_name(st->type));
            break;
        }
        case StepFieldDuration: {
            int16_t v = (int16_t)st->duration_sec + dir * 5;
            if(v < 0) v = 0;
            if(v > 600) v = 600;
            st->duration_sec = (uint16_t)v;
            break;
        }
        case StepFieldWater: {
            int16_t v = (int16_t)st->water_ml_div10 + dir;
            if(v < 0) v = 0;
            if(v > 200) v = 200;
            st->water_ml_div10 = (uint8_t)v;
            break;
        }
        case StepFieldWeight: {
            int16_t v = (int16_t)st->weight_grams + dir;
            if(v < 0) v = 0;
            if(v > 200) v = 200;
            st->weight_grams = (uint8_t)v;
            break;
        }
        default: break;
        }
        step_auto_detail(st);
    } else if(ev->key == InputKeyOk || ev->key == InputKeyBack) {
        step_auto_detail(st);
        app->s.screen = ScreenEditSteps;
    }
}

// ============================================================
// Main input handler
// ============================================================
static void handle_input(CoffeeApp* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    AppState* s = &app->s;

    switch(s->screen) {
    case ScreenConfirmAbort:
        if(ev->key == InputKeyOk) { s->screen = ScreenRecipeMenu; s->timer_state = TimerStopped; }
        else if(ev->key == InputKeyBack) s->screen = ScreenBrewing;
        break;

    case ScreenConfirmDelete:
        if(ev->key == InputKeyOk) {
            custom_recipe_delete(app, app->editor.recipe_idx);
            app->editor.sel = 0;
            s->screen = ScreenEditMenu;
        } else if(ev->key == InputKeyBack) {
            s->screen = ScreenEditRecipe;
        }
        break;

    case ScreenMethodMenu: {
        uint8_t total = method_count + 1;
        if(ev->key == InputKeyUp) {
            s->method_sel = (s->method_sel == 0) ? total - 1 : s->method_sel - 1;
        } else if(ev->key == InputKeyDown) {
            s->method_sel = (s->method_sel >= total - 1) ? 0 : s->method_sel + 1;
        } else if(ev->key == InputKeyOk) {
            s->recipe_sel = 0;
            s->screen = ScreenRecipeMenu;
        } else if(ev->key == InputKeyRight && s->method_sel >= method_count) {
            // Edit custom recipes
            app->editor.sel = 0;
            s->screen = ScreenEditMenu;
        } else if(ev->key == InputKeyBack) {
            s->running = false;
        }
        break;
    }

    case ScreenRecipeMenu: {
        bool is_cust = (s->method_sel >= method_count);
        uint8_t count = is_cust ? app->custom_count : methods[s->method_sel].recipe_count;
        if(count == 0) {
            if(ev->key == InputKeyBack) s->screen = ScreenMethodMenu;
            break;
        }
        if(ev->key == InputKeyUp) {
            s->recipe_sel = (s->recipe_sel == 0) ? count - 1 : s->recipe_sel - 1;
        } else if(ev->key == InputKeyDown) {
            s->recipe_sel = (s->recipe_sel >= count - 1) ? 0 : s->recipe_sel + 1;
        } else if(ev->key == InputKeyOk) {
            s->cur_method = s->method_sel;
            s->cur_recipe = s->recipe_sel;
            s->using_custom = is_cust;
            s->ratio_adjust = 0;
            s->screen = ScreenRecipeInfo;
        } else if(ev->key == InputKeyRight && !is_cust) {
            settings_toggle_favourite(&app->settings, s->method_sel, s->recipe_sel);
            settings_save(app);
        } else if(ev->key == InputKeyBack) {
            s->screen = ScreenMethodMenu;
        }
        break;
    }

    case ScreenRecipeInfo:
        if(ev->key == InputKeyOk) {
            s->screen = ScreenBrewing;
            s->cur_step = 0;
            s->step_elapsed_ms = 0;
            s->total_elapsed_ms = 0;
            s->step_complete = false;
            s->cumulative_water_ml = 0;
            s->show_upcoming = false;
            s->timer_state = (get_sdur(app, 0) > 0) ? TimerRunning : TimerStopped;
            if(!s->using_custom) {
                app->settings.last_method = s->cur_method;
                app->settings.last_recipe = s->cur_recipe;
                settings_save(app);
            }
        } else if(ev->key == InputKeyLeft) {
            if(s->ratio_adjust > -5) s->ratio_adjust--;
        } else if(ev->key == InputKeyRight) {
            if(s->ratio_adjust < 5) s->ratio_adjust++;
        } else if(ev->key == InputKeyUp) {
            if(!app->settings.auto_advance) app->settings.auto_advance = true;
            else if(app->settings.sound_on) app->settings.sound_on = false;
            else if(app->settings.led_on) app->settings.led_on = false;
            else { app->settings.auto_advance = false; app->settings.sound_on = true; app->settings.led_on = true; }
            settings_save(app);
        } else if(ev->key == InputKeyDown) {
            s->ratio_adjust = 0;
        } else if(ev->key == InputKeyBack) {
            s->screen = ScreenRecipeMenu;
        }
        break;

    case ScreenBrewing: {
        uint16_t dur = get_sdur(app, s->cur_step);
        if(ev->key == InputKeyOk) {
            if(dur == 0 || s->step_complete) advance(app);
            else if(s->timer_state == TimerRunning) s->timer_state = TimerPaused;
            else if(s->timer_state == TimerPaused) s->timer_state = TimerRunning;
        } else if(ev->key == InputKeyRight) { advance(app); }
        else if(ev->key == InputKeyLeft) {
            uint8_t sc = get_scount(app);
            s->cur_step = (s->cur_step > 0) ? s->cur_step - 1 : sc - 1;
            s->step_elapsed_ms = 0;
            s->step_complete = false;
            s->timer_state = (get_sdur(app, s->cur_step) > 0) ? TimerRunning : TimerStopped;
        } else if(ev->key == InputKeyUp) {
            s->show_upcoming = !s->show_upcoming;
        } else if(ev->key == InputKeyBack) {
            s->timer_state = TimerPaused;
            s->screen = ScreenConfirmAbort;
        }
        break;
    }

    case ScreenComplete:
        if(ev->key == InputKeyOk) s->screen = ScreenMethodMenu;
        else if(ev->key == InputKeyBack) s->running = false;
        break;

    // Editor screens
    case ScreenEditMenu:   handle_edit_menu(app, ev); break;
    case ScreenEditRecipe: handle_edit_recipe(app, ev); break;
    case ScreenEditSteps:  handle_edit_steps(app, ev); break;
    case ScreenEditStep:   handle_edit_step(app, ev); break;
    }
}

// ============================================================
// Alloc / Free / Main
// ============================================================
static CoffeeApp* app_alloc(void) {
    CoffeeApp* app = malloc(sizeof(CoffeeApp));
    memset(app, 0, sizeof(CoffeeApp));
    app->s.screen = ScreenMethodMenu;
    app->s.running = true;

    settings_load(app);
    custom_recipes_load(app);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->timer = furi_timer_alloc(tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(100));
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    return app;
}

static void app_free(CoffeeApp* app) {
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t coffee_timer_main(void* p) {
    UNUSED(p);
    CoffeeApp* app = app_alloc();
    InputEvent ev;
    while(app->s.running) {
        FuriStatus st = furi_message_queue_get(app->queue, &ev, 100);
        if(furi_mutex_acquire(app->mutex, 25) == FuriStatusOk) {
            if(st == FuriStatusOk) handle_input(app, &ev);
            check_auto_advance(app);
            furi_mutex_release(app->mutex);
        }
        view_port_update(app->view_port);
    }
    app_free(app);
    return 0;
}
