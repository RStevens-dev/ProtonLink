#include "telemetry.h"
#include "options.h"
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include "json_input.h"

static bool read_enum(cJSON *root, const char *key, const char *a, const char *b, bool *value)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(v)) return false;
    if (!strcmp(v->valuestring, a)) { *value = true; return true; }
    if (!strcmp(v->valuestring, b)) { *value = false; return true; }
    return false;
}
static bool read_volume(cJSON *root, const char *key, int *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < 0 || v->valuedouble > 100 || v->valuedouble != v->valueint) return false;
    *out = v->valueint;
    return true;
}
static void read_text(cJSON *root, const char *key, char *out, size_t size)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    const char *s = cJSON_IsString(v) ? v->valuestring : "Unknown";
    if (strlen(s) >= size) s = "Unknown";
    strcpy(out, s);
}
bool protonlink_decode(const char *json, protonlink_status_t *out)
{
    if (!json || !out || !pl_json_depth_ok(json)) return false;
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    protonlink_status_t s = {0};
    bool valid = cJSON_IsObject(root) &&
        read_enum(root, "pack", "Powered", "Idle", &s.pack_on) &&
        read_enum(root, "firing", "Firing", "Idle", &s.firing) &&
        read_volume(root, "volMaster", &s.master) &&
        read_volume(root, "volEffects", &s.effects) &&
        read_volume(root, "volMusic", &s.music);
    if (valid) {
        static const char *const option_fields[]={"volMuted","musicLooping","musicShuffled","smoke","vibration","direction"};
        for(int i=0;i<=PL_OPT_DIRECTION;i++) {
            const cJSON *field=cJSON_GetObjectItemCaseSensitive(root,option_fields[i]);
            if(cJSON_IsBool(field)) {
                s.known_options|=1u<<i;
                if(cJSON_IsTrue(field)) s.option_values|=1u<<i;
            }
        }
        s.vg_mode=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"vgMode"));
        s.stream_change_reported=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"canChangeStream"));
        const cJSON *power=cJSON_GetObjectItemCaseSensitive(root,"power");
        if(cJSON_IsString(power) && power->valuestring[0]>='1' && power->valuestring[0]<='5' && !power->valuestring[1])
            s.power_level=power->valuestring[0]-'0';
        if(cJSON_IsNumber(power) && power->valuedouble>=1 && power->valuedouble<=5 && power->valuedouble==power->valueint)
            s.power_level=power->valueint;
        read_text(root, "mode", s.mode, sizeof(s.mode));
        read_text(root, "theme", s.theme, sizeof(s.theme));
        read_text(root, "wandMode", s.stream, sizeof(s.stream));
        read_text(root, "wand", s.wand, sizeof(s.wand));
        read_text(root, "wandPower", s.wand_power, sizeof(s.wand_power));
        read_text(root, "cable", s.cable, sizeof(s.cable));
        bool extended;
        strcpy(s.barrel, read_enum(root, "safety", "Safety Off", "Safety On", &extended)
               ? (extended ? "Extended" : "Retracted") : "Unknown");
        const cJSON *lid = cJSON_GetObjectItemCaseSensitive(root, "cyclotronLid");
        s.lid_known = cJSON_IsBool(lid);
        s.lid_fitted = cJSON_IsTrue(lid);
        read_text(root, "temperature", s.temperature, sizeof(s.temperature));
        read_text(root, "cyclotron", s.cyclotron, sizeof(s.cyclotron));
        const cJSON *playing = cJSON_GetObjectItemCaseSensitive(root, "musicPlaying");
        const cJSON *paused = cJSON_GetObjectItemCaseSensitive(root, "musicPaused");
        s.music_known = cJSON_IsBool(playing) && cJSON_IsBool(paused);
        s.playing = cJSON_IsTrue(playing);
        s.paused = cJSON_IsTrue(paused);
        const cJSON *track = cJSON_GetObjectItemCaseSensitive(root, "musicCurrent");
        s.track = cJSON_IsNumber(track) && track->valuedouble >= 0 && track->valuedouble <= 65535 && track->valuedouble == track->valueint ? track->valueint : -1;
        const cJSON *first = cJSON_GetObjectItemCaseSensitive(root, "musicStart");
        const cJSON *last = cJSON_GetObjectItemCaseSensitive(root, "musicEnd");
        s.track_first = s.track_last = -1;
        if (cJSON_IsNumber(first) && cJSON_IsNumber(last) &&
            first->valuedouble >= 500 && last->valuedouble <= 4595 &&
            first->valuedouble == first->valueint && last->valuedouble == last->valueint &&
            first->valueint <= last->valueint) {
            s.track_first = first->valueint; s.track_last = last->valueint;
        }
        const cJSON *ramp = cJSON_GetObjectItemCaseSensitive(root, "ramping");
        s.transition_known = cJSON_IsBool(ramp);
        s.ramping = cJSON_IsTrue(ramp);
        s.lockout = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "lockout"));
        const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
        s.legacy_superhero = cJSON_IsString(mode) && !strcmp(mode->valuestring, "Super Hero");
        s.online = true;
        *out = s;
    }
    cJSON_Delete(root);
    return valid;
}
