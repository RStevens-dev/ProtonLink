#pragma once
#include "telemetry.h"

typedef enum {
    PL_OPT_MUTE, PL_OPT_REPEAT, PL_OPT_SHUFFLE, PL_OPT_SMOKE, PL_OPT_VIBRATION, PL_OPT_DIRECTION,
    PL_OPT_1984, PL_OPT_1989, PL_OPT_2021, PL_OPT_2024,
    PL_OPT_PROTON, PL_OPT_STASIS, PL_OPT_SLIME, PL_OPT_MESON, PL_OPT_SPECTRAL,
    PL_OPT_HALLOWEEN, PL_OPT_CHRISTMAS, PL_OPT_CUSTOM, PL_OPT_COUNT
} pl_option_t;

const char *pl_option_candidate(pl_option_t option, bool second);
const char *pl_option_path(pl_option_t option, const protonlink_status_t *status);
const char *pl_option_label(pl_option_t option, const protonlink_status_t *status);
bool pl_request_option(pl_option_t option);
