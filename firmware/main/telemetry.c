#include "telemetry.h"
#include <string.h>
#include <math.h>
#include "cJSON.h"

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
    if (!json || !out) return false;
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    protonlink_status_t s = {0};
    bool valid = cJSON_IsObject(root) &&
        read_enum(root, "pack", "Powered", "Idle", &s.pack_on) &&
        read_enum(root, "firing", "Firing", "Idle", &s.firing) &&
        read_volume(root, "volMaster", &s.master) &&
        read_volume(root, "volEffects", &s.effects) &&
        read_volume(root, "volMusic", &s.music);
    if (valid) {
        read_text(root, "temperature", s.temperature, sizeof(s.temperature));
        read_text(root, "cyclotron", s.cyclotron, sizeof(s.cyclotron));
        s.online = true;
        *out = s;
    }
    cJSON_Delete(root);
    return valid;
}
