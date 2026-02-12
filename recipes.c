#include "coffee_timer.h"

// ================================================================
// AEROPRESS
// ================================================================
static const BrewStep ap_standard_steps[] = {
    {"Setup",       "Insert filter, rinse, on mug", 0,  0,  0, StepPrep},
    {"Add coffee",  "15g medium-fine grind",        0, 15,  0, StepAdd},
    {"Pour water",  "Add 200ml at 85C",            10,  0, 20, StepPour},
    {"Stir",        "Stir gently 3 times",          5,  0,  0, StepStir},
    {"Steep",       "Wait for extraction",         60,  0,  0, StepWait},
    {"Press",       "Slow steady press ~30s",      30,  0,  0, StepPress},
};
static const BrewStep ap_inverted_steps[] = {
    {"Invert",      "Plunger on bottom, no filter", 0,  0,  0, StepPrep},
    {"Add coffee",  "17g medium grind",             0, 17,  0, StepAdd},
    {"Pour water",  "Add 220ml at 90C",            10,  0, 22, StepPour},
    {"Stir",        "Stir vigorously 5 times",     10,  0,  0, StepStir},
    {"Steep",       "Full immersion brew",         90,  0,  0, StepWait},
    {"Flip",        "Attach cap, flip onto mug",    0,  0,  0, StepFlip},
    {"Press",       "Slow steady press ~30s",      30,  0,  0, StepPress},
};
static const BrewStep ap_hoffmann_steps[] = {
    {"Setup",       "Filter in cap, no rinse",      0,  0,  0, StepPrep},
    {"Add coffee",  "11g fine grind",               0, 11,  0, StepAdd},
    {"Pour water",  "Add 200ml boiling",           10,  0, 20, StepPour},
    {"Insert plgr", "Create seal, don't press",     0,  0,  0, StepWait},
    {"Wait",        "Let it steep",               120,  0,  0, StepWait},
    {"Swirl+wait",  "Gentle swirl, wait",          60,  0,  0, StepSwirl},
    {"Press",       "Very gentle press to hiss",   30,  0,  0, StepPress},
};
static const BrewStep ap_iced_steps[] = {
    {"Prep ice",    "Fill mug with 100g ice",       0,  0,  0, StepPrep},
    {"Add coffee",  "20g fine grind",               0, 20,  0, StepAdd},
    {"Pour water",  "120ml at 95C",                10,  0, 12, StepPour},
    {"Stir",        "Stir well to saturate",       10,  0,  0, StepStir},
    {"Steep",       "Short steep for strength",    45,  0,  0, StepWait},
    {"Press",       "Press directly onto ice",     20,  0,  0, StepPress},
};
static const BrewStep ap_comp_steps[] = {
    {"Invert",      "Plunger at 1, inverted",       0,  0,  0, StepPrep},
    {"Add coffee",  "30g coarse grind",             0, 30,  0, StepAdd},
    {"Bloom",       "Add 60ml, wet all grounds",   10,  0,  6, StepPour},
    {"Bloom wait",  "Let CO2 escape",              30,  0,  0, StepWait},
    {"Pour rest",   "Add 200ml more",              15,  0, 20, StepPour},
    {"Stir",        "Gentle back-and-forth x3",     5,  0,  0, StepStir},
    {"Flip",        "Attach cap, flip",             0,  0,  0, StepFlip},
    {"Press",       "Slow 45s press",              45,  0,  0, StepPress},
};
static const Recipe aeropress_recipes[] = {
    {"Standard",    "Medium-Fine", ap_standard_steps, 200,  85, 15, 6},
    {"Inverted",    "Medium",      ap_inverted_steps, 220,  90, 17, 7},
    {"Hoffmann",    "Fine",        ap_hoffmann_steps, 200, 100, 11, 7},
    {"Iced Coffee", "Fine",        ap_iced_steps,     120,  95, 20, 6},
    {"Competition", "Coarse",      ap_comp_steps,     260,  82, 30, 8},
};

// ================================================================
// POUR OVER
// ================================================================
static const BrewStep v60_classic_steps[] = {
    {"Setup",       "Rinse filter with hot water",  0,  0,  0, StepPrep},
    {"Add coffee",  "15g medium grind",             0, 15,  0, StepAdd},
    {"Bloom",       "Pour 30ml, wet all grounds",   5,  0,  3, StepPour},
    {"Bloom wait",  "Let grounds degas",           30,  0,  0, StepWait},
    {"First pour",  "Pour to 150ml in circles",    30,  0, 12, StepPour},
    {"Second pour", "Pour to 250ml in circles",    30,  0, 10, StepPour},
    {"Drawdown",    "Wait for full drawdown",      60,  0,  0, StepWait},
};
static const BrewStep v60_hoffmann_steps[] = {
    {"Setup",       "Rinse filter thoroughly",      0,  0,  0, StepPrep},
    {"Add coffee",  "15g med-fine, dig a well",     0, 15,  0, StepAdd},
    {"Bloom",       "Pour 50ml into the well",     10,  0,  5, StepPour},
    {"Swirl",       "Swirl V60 to mix slurry",      0,  0,  0, StepSwirl},
    {"Bloom wait",  "Let it degas",                35,  0,  0, StepWait},
    {"Main pour",   "Pour to 250ml steadily",      30,  0, 20, StepPour},
    {"Swirl",       "Gentle swirl, flatten bed",    0,  0,  0, StepSwirl},
    {"Drawdown",    "Wait ~3:30 total",            90,  0,  0, StepWait},
};
static const BrewStep chemex_steps[] = {
    {"Setup",       "Fold filter, rinse, discard",  0,  0,  0, StepPrep},
    {"Add coffee",  "25g medium-coarse grind",      0, 25,  0, StepAdd},
    {"Bloom",       "Pour 50ml gently",            10,  0,  5, StepPour},
    {"Bloom wait",  "Let grounds degas",           30,  0,  0, StepWait},
    {"First pour",  "Pour to 200ml in circles",    30,  0, 15, StepPour},
    {"Pause",       "Let level drop slightly",     15,  0,  0, StepWait},
    {"Final pour",  "Pour to 400ml in circles",    30,  0, 20, StepPour},
    {"Drawdown",    "Wait for full drawdown",      90,  0,  0, StepWait},
};
static const BrewStep iced_v60_steps[] = {
    {"Prep ice",    "Put 100g ice in server",       0,  0,  0, StepPrep},
    {"Setup",       "Rinse filter with hot water",  0,  0,  0, StepPrep},
    {"Add coffee",  "20g medium-fine grind",        0, 20,  0, StepAdd},
    {"Bloom",       "Pour 40ml at 95C",             5,  0,  4, StepPour},
    {"Bloom wait",  "Let grounds degas",           30,  0,  0, StepWait},
    {"First pour",  "Pour to 100ml slowly",        25,  0,  6, StepPour},
    {"Final pour",  "Pour to 150ml",               25,  0,  5, StepPour},
    {"Drawdown",    "Wait, then swirl with ice",   60,  0,  0, StepWait},
};
static const Recipe pourover_recipes[] = {
    {"V60 Classic",  "Medium",      v60_classic_steps,  250, 93, 15, 7},
    {"V60 Hoffmann", "Medium-Fine", v60_hoffmann_steps, 250, 95, 15, 8},
    {"Chemex",       "Med-Coarse",  chemex_steps,       400, 93, 25, 8},
    {"Iced V60",     "Medium-Fine", iced_v60_steps,     150, 95, 20, 8},
};

// ================================================================
// FRENCH PRESS
// ================================================================
static const BrewStep fp_classic_steps[] = {
    {"Preheat",     "Fill press with hot water",    0,  0,  0, StepPrep},
    {"Discard",     "Empty preheat water",          0,  0,  0, StepPrep},
    {"Add coffee",  "30g coarse grind",             0, 30,  0, StepAdd},
    {"Pour water",  "Add 500ml at 93C",            15,  0, 50, StepPour},
    {"Stir",        "Stir gently to saturate",      5,  0,  0, StepStir},
    {"Steep",       "Wait, do not touch",         240,  0,  0, StepWait},
    {"Press",       "Slow steady press",           20,  0,  0, StepPress},
};
static const BrewStep fp_hoffmann_steps[] = {
    {"Preheat",     "Fill press with hot water",    0,  0,  0, StepPrep},
    {"Discard",     "Empty preheat water",          0,  0,  0, StepPrep},
    {"Add coffee",  "30g medium grind",             0, 30,  0, StepAdd},
    {"Pour water",  "Add 500ml boiling",           15,  0, 50, StepPour},
    {"Steep",       "Do NOT stir, just wait",     240,  0,  0, StepWait},
    {"Break crust", "Stir top crust, scoop foam",  30,  0,  0, StepStir},
    {"Wait more",   "Let fines settle",           300,  0,  0, StepWait},
    {"Pour gently", "Pour without pressing",        0,  0,  0, StepPress},
};
static const BrewStep fp_strong_steps[] = {
    {"Preheat",     "Fill press with hot water",    0,  0,  0, StepPrep},
    {"Discard",     "Empty preheat water",          0,  0,  0, StepPrep},
    {"Add coffee",  "36g coarse grind",             0, 36,  0, StepAdd},
    {"Bloom",       "Add 70ml, wet grounds",       10,  0,  7, StepPour},
    {"Bloom wait",  "Let CO2 escape",              30,  0,  0, StepWait},
    {"Pour rest",   "Add 430ml at 96C",            15,  0, 43, StepPour},
    {"Stir",        "Stir vigorously 5 times",      5,  0,  0, StepStir},
    {"Steep",       "Full extraction",            300,  0,  0, StepWait},
    {"Press",       "Slow steady press",           20,  0,  0, StepPress},
};
static const Recipe frenchpress_recipes[] = {
    {"Classic",     "Coarse",  fp_classic_steps,  500, 93, 30, 7},
    {"Hoffmann",    "Medium",  fp_hoffmann_steps, 500, 100, 30, 8},
    {"Strong",      "Coarse",  fp_strong_steps,   500, 96, 36, 9},
};

// ================================================================
// MOKA POT
// ================================================================
static const BrewStep moka_classic_steps[] = {
    {"Boil water",  "Pre-boil water in kettle",     0,  0,  0, StepPrep},
    {"Fill base",   "Hot water to valve line",       0,  0, 20, StepAdd},
    {"Add coffee",  "Fill basket, level off",        0, 15,  0, StepAdd},
    {"Assemble",    "Screw top tight (use towel)",   0,  0,  0, StepPrep},
    {"Heat",        "Medium heat, lid open",        60,  0,  0, StepWait},
    {"Watch",       "Coffee starts flowing",        60,  0,  0, StepWait},
    {"Remove",      "When hissing/blonding starts",  0,  0,  0, StepPrep},
    {"Cool base",   "Run base under cold water",     0,  0,  0, StepPrep},
};
static const BrewStep moka_hoffmann_steps[] = {
    {"Boil water",  "Pre-boil water in kettle",     0,  0,  0, StepPrep},
    {"Fill base",   "Hot water below valve",         0,  0, 20, StepAdd},
    {"Add coffee",  "Fill basket, don't tamp",       0, 15,  0, StepAdd},
    {"Assemble",    "Screw on (towel for heat)",     0,  0,  0, StepPrep},
    {"Low heat",    "Lowest heat setting",          45,  0,  0, StepWait},
    {"Watch flow",  "Should flow like warm honey",  60,  0,  0, StepWait},
    {"Lid open",    "Watch color, wait for blonde",  0,  0,  0, StepWait},
    {"Remove+cool", "Cold towel on base to stop",    0,  0,  0, StepPrep},
    {"Stir+serve",  "Stir in pot then pour",         0,  0,  0, StepStir},
};
static const BrewStep moka_iced_steps[] = {
    {"Prep ice",    "Fill glass with ice",           0,  0,  0, StepPrep},
    {"Boil water",  "Pre-boil water in kettle",     0,  0,  0, StepPrep},
    {"Fill base",   "Hot water to valve line",       0,  0, 20, StepAdd},
    {"Add coffee",  "Fill basket, level off",        0, 18,  0, StepAdd},
    {"Assemble",    "Screw top on tight",            0,  0,  0, StepPrep},
    {"Heat",        "Medium heat, lid open",        60,  0,  0, StepWait},
    {"Watch",       "Remove when hissing starts",   60,  0,  0, StepWait},
    {"Pour on ice", "Pour directly over ice",        0,  0,  0, StepPour},
};
static const Recipe moka_recipes[] = {
    {"Classic",     "Fine",    moka_classic_steps,  200, 100, 15, 8},
    {"Hoffmann",    "Fine",    moka_hoffmann_steps, 200, 100, 15, 9},
    {"Iced Moka",   "Fine",    moka_iced_steps,     200, 100, 18, 8},
};

// ================================================================
// COLD BREW
// ================================================================
static const BrewStep cb_standard_steps[] = {
    {"Add coffee",  "100g coarse grind to jar",      0,100,  0, StepAdd},
    {"Add water",   "Add 1000ml room temp water",    0,  0,100, StepPour},
    {"Stir",        "Stir to fully saturate",       15,  0,  0, StepStir},
    {"Cover",       "Seal jar, into fridge",         0,  0,  0, StepPrep},
    {"Steep",       "12-24hrs in fridge",             0,  0,  0, StepWait},
    {"Filter",      "Strain through fine filter",    0,  0,  0, StepPrep},
};
static const BrewStep cb_concentrate_steps[] = {
    {"Add coffee",  "150g coarse grind to jar",      0,150,  0, StepAdd},
    {"Add water",   "Add 750ml room temp water",     0,  0, 75, StepPour},
    {"Stir",        "Stir to fully wet grounds",    15,  0,  0, StepStir},
    {"Cover",       "Seal jar, into fridge",         0,  0,  0, StepPrep},
    {"Steep",       "16-24hrs in fridge",             0,  0,  0, StepWait},
    {"Filter",      "Strain through fine filter",    0,  0,  0, StepPrep},
    {"Dilute",      "Mix 1:1 with water or milk",    0,  0,  0, StepPrep},
};
static const BrewStep cb_japanese_steps[] = {
    {"Add coffee",  "50g medium-fine to dripper",    0, 50,  0, StepAdd},
    {"Prep ice",    "300g ice in server below",      0,  0,  0, StepPrep},
    {"First pour",  "100ml room temp water",        10,  0, 10, StepPour},
    {"Wait",        "Let it drip slowly",           60,  0,  0, StepWait},
    {"Second pour", "100ml more water",             10,  0, 10, StepPour},
    {"Wait",        "Let it drip through",          60,  0,  0, StepWait},
    {"Final pour",  "100ml more water",             10,  0, 10, StepPour},
    {"Drawdown",    "Wait for full drip",           90,  0,  0, StepWait},
    {"Swirl",       "Swirl to melt ice, serve",      0,  0,  0, StepSwirl},
};
static const Recipe coldbrew_recipes[] = {
    {"Standard",      "Coarse",      cb_standard_steps,    1000, 20, 100, 6},
    {"Concentrate",   "Coarse",      cb_concentrate_steps,  750, 20, 150, 7},
    {"Japanese Iced", "Medium-Fine", cb_japanese_steps,     300, 20,  50, 9},
};

// ================================================================
// METHODS
// ================================================================
const BrewMethod methods[] = {
    {"AeroPress",    aeropress_recipes,   5},
    {"Pour Over",    pourover_recipes,    4},
    {"French Press", frenchpress_recipes, 3},
    {"Moka Pot",     moka_recipes,        3},
    {"Cold Brew",    coldbrew_recipes,    3},
};
const uint8_t method_count = sizeof(methods) / sizeof(methods[0]);
