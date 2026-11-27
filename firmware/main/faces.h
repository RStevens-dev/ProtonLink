#pragma once
#include "lvgl.h"
#include "telemetry.h"

typedef enum {
    PL_FACE_PKE, PL_FACE_CYCLOTRON, PL_FACE_CRT, PL_FACE_BLUEPRINT, PL_FACE_ANALOG,
    PL_FACE_CASSETTE, PL_FACE_CONTAINMENT, PL_FACE_CELL, PL_FACE_MARQUEE, PL_FACE_MINIMAL,
    PL_FACE_MISSION, PL_FACE_COUNT, PL_ART_DIAGNOSTICS=PL_FACE_COUNT, PL_ART_TRANSPORT
} pl_face_t;

const char *pl_face_name(pl_face_t face);
lv_obj_t *pl_face_create(lv_obj_t *parent, int y, int height);
void pl_face_options(bool contact_labels, uint32_t marquee_color);
void pl_face_update(lv_obj_t *face, pl_face_t style, const protonlink_status_t *status,
                    const char *clock, const char *date, bool still);
