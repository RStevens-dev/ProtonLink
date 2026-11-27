#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PL_TRACK_MIN 500
#define PL_TRACK_MAX 4595
#define PL_TITLE_SIZE 181
#define PL_CATALOG_LIMIT 96
#define PL_PLAYLIST_LIMIT 6
#define PL_PLAYLIST_TRACKS 48
#define PL_PROFILE_COUNT 4
#define PL_NAME_SIZE 33
#define PL_SONG_LIST_SIZE 18432

void pl_catalog_load(void);
bool pl_catalog_saved(void);
bool pl_catalog_range(int first, int last);
const char *pl_catalog_title(int id, int first, char fallback[24]);
bool pl_catalog_edit(int id, const char *title);
bool pl_catalog_import(const char *text, int first, int last, bool sync);
bool pl_catalog_favorite(int id);
void pl_catalog_star(int id);
bool pl_catalog_match(int id, int first, const char *search);
int pl_catalog_profile(void);
const char *pl_catalog_profile_name(void);
void pl_catalog_switch_profile(void);
void pl_catalog_name_profile(const char *name);
const char *pl_catalog_playlist_name(int slot);
int pl_catalog_playlist_track(int slot, int offset);
bool pl_catalog_save_playlist(int slot, const char *name, const char *ids, int first, int last);
