#pragma once
#include "telemetry.h"
typedef enum { PL_START, PL_STOP, PL_VENT, PL_MASTER_UP, PL_MASTER_DOWN,
 PL_EFFECTS_UP, PL_EFFECTS_DOWN, PL_MUSIC_UP, PL_MUSIC_DOWN,
 PL_PLAY_STOP, PL_PAUSE_RESUME, PL_PREVIOUS, PL_NEXT } pl_action_t;
const char *pl_command_path(pl_action_t action, const protonlink_status_t *s);
bool pl_request(pl_action_t action);
void pl_set_unlocked(bool unlocked);
bool pl_is_unlocked(void);

// Channel order: master, effects, music.
int pl_volume_value(const protonlink_status_t *s, int channel);
const char *pl_volume_step(const protonlink_status_t *s, int channel, int target);
bool pl_request_volume(int channel, int target);
bool pl_busy(void);
