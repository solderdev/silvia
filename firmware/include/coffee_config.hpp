#pragma once

// timing
#define SHOT_T_INITWATER  400  // ms
#define SHOT_T_RAMP       8000  // ms
#define SHOT_T_PAUSE      0  // ms

#define SHOT_RAMP_MIN  10  // %
#define SHOT_RAMP_MAX  100  // %

#define PREHEAT_T_WATER  1000  // ms
#define PREHEAT_T_FILL   300  // ms
#define PREHEAT_T_PAUSE  10000  // ms

#define PUMP_OVERRIDE_MS  800  // ms


// temperatures
#define BREW_TEMP      94.0f   // deg-C
#define STEAM_TEMP     116.0f  // deg-C
#define BREWHEAD_TEMP  81      // deg-C

// PID config
#define PID_P_POS     32
#define PID_P_NEG     90
#define PID_I         1.2f
#define PID_D         -20
#define PID_TS        1000  // ms

#define PID_MIN_OUTPUT  10  // if 0 < output <= MIN_OUTPUT --> apply MIN_OUTPUT

#define PID_OVERRIDE_STARTSHOT  100.0f
#define PID_OVERRIDE_COUNT      3

#define PID_OVERRIDE_TEMP       100.0f
#define PID_OVERRIDE_TEMP_ERR   10


// hardware config
#define ADC_VREF_MEASURED  1141  // mV

#define POWEROFF_MINUTES  50  // min
