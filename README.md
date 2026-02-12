## Controls

### Recipe Menu
- **Up/Down**: Browse recipes
- **OK**: Select recipe
- **Back**: Exit app

### Recipe Info
- **OK**: Start brewing
- **Back**: Return to menu

### Brewing
- **OK**: Pause/Resume timer (timed steps) or Advance (manual/completed steps)
- **Right**: Skip to next step
- **Left**: Go back one step
- **Back**: Abort brew, return to menu

### Brew Complete
- **OK**: Return to menu
- **Back**: Exit app

## Features

- Step-by-step guided brewing with clear instructions
- Automatic countdown timers with progress bars
- Manual advance for prep steps (no arbitrary timing)
- Vibration alerts when steps complete
- Total brew time tracking
- Pause/resume support
- Step navigation (skip ahead or go back)

## Project Structure

```
coffee_timer/
├── application.fam      # App manifest
├── coffee_timer.h       # Types, structs, recipe definitions
├── coffee_timer.c       # Main app logic, UI, input handling
├── recipes.c            # Recipe data
├── coffee_timer.png     # 10x10 FAP icon
├── images/              # In-app image assets
│   └── CoffeeCup_20x20.png
└── README.md
```

## Adding Recipes

Edit `recipes.c` to add new recipes. Each recipe has a name, description, parameters (coffee, water, temp, grind), and an array of steps. Steps can be:

- `StepTypeAdd` - Add coffee or water
- `StepTypeStir` - Stir
- `StepTypeWait` - Wait/steep
- `StepTypePress` - Press plunger
- `StepTypeFlip` - Flip inverted AeroPress
- `StepTypePreheat` - Setup/preheat

Steps with `duration_sec = 0` are manual advance (press OK). Steps with a duration auto-start a countdown timer.

## License

MIT
