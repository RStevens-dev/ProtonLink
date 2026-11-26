#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    bool online;
    bool pack_on;
    bool firing;
    bool music_known;
    bool playing;
    bool paused;
    bool transition_known;
    bool ramping;
    bool lockout;
    bool legacy_superhero;
    int track;
    int track_first;
    int track_last;
    int master;
    int effects;
    int music;
    char temperature[24];
    char cyclotron[24];
} protonlink_status_t;
bool protonlink_decode(const char *json, protonlink_status_t *out);
void protonlink_network_start(void);
bool protonlink_snapshot(protonlink_status_t *out);
