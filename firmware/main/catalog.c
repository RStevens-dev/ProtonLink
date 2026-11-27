#include "catalog.h"
#include "cJSON.h"
#include "json_input.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PROTONLINK_DESKTOP
#include "nvs.h"
#include "esp_attr.h"
#define PL_EXTERNAL_BSS EXT_RAM_BSS_ATTR
#else
#define PL_EXTERNAL_BSS
#endif

typedef struct { uint16_t id; char title[PL_TITLE_SIZE]; } track_name_t;
typedef struct { char name[PL_NAME_SIZE]; uint16_t ids[PL_PLAYLIST_TRACKS]; } playlist_t;
typedef struct {
    char name[PL_NAME_SIZE];
    uint8_t favorites[(PL_TRACK_MAX-PL_TRACK_MIN+8)/8];
    track_name_t titles[PL_CATALOG_LIMIT];
    playlist_t playlists[PL_PLAYLIST_LIMIT];
} profile_t;
static PL_EXTERNAL_BSS profile_t local;
#ifdef PROTONLINK_DESKTOP
static profile_t desktop_profiles[PL_PROFILE_COUNT];
#endif
static PL_EXTERNAL_BSS track_name_t synced[PL_CATALOG_LIMIT], staged[PL_CATALOG_LIMIT];
static uint8_t profile;
#ifndef PROTONLINK_DESKTOP
static PL_EXTERNAL_BSS uint8_t encoded[sizeof(profile_t)+2+PL_CATALOG_LIMIT];

static bool unpack_profile(size_t size)
{
    size_t header=sizeof(local.name)+sizeof(local.favorites)+sizeof(local.playlists);
    if(size<header+1 || encoded[0]!=1) return false;
    memcpy(local.name,encoded+1,sizeof(local.name));
    memcpy(local.favorites,encoded+1+sizeof(local.name),sizeof(local.favorites));
    memcpy(local.playlists,encoded+1+sizeof(local.name)+sizeof(local.favorites),sizeof(local.playlists));
    size_t pos=header+1;
    for(int i=0;pos<size;i++) {
        if(i>=PL_CATALOG_LIMIT || size-pos<3) return false;
        uint16_t id=(uint16_t)(encoded[pos]|(encoded[pos+1]<<8));
        size_t len=encoded[pos+2];
        pos+=3;
        if(id<PL_TRACK_MIN || id>PL_TRACK_MAX || len>=PL_TITLE_SIZE || len>size-pos) return false;
        local.titles[i].id=id;
        memcpy(local.titles[i].title,encoded+pos,len);
        pos+=len;
    }
    return true;
}

static size_t pack_profile(void)
{
    encoded[0]=1;
    size_t pos=1;
    memcpy(encoded+pos,local.name,sizeof(local.name)); pos+=sizeof(local.name);
    memcpy(encoded+pos,local.favorites,sizeof(local.favorites)); pos+=sizeof(local.favorites);
    memcpy(encoded+pos,local.playlists,sizeof(local.playlists)); pos+=sizeof(local.playlists);
    for(int i=0;i<PL_CATALOG_LIMIT;i++) {
        const track_name_t *entry=&local.titles[i];
        if(!entry->id) continue;
        size_t len=strlen(entry->title);
        encoded[pos++]=(uint8_t)entry->id;
        encoded[pos++]=(uint8_t)(entry->id>>8);
        encoded[pos++]=(uint8_t)len;
        memcpy(encoded+pos,entry->title,len);
        pos+=len;
    }
    return pos;
}
#endif
static bool catalog_saved=true;
bool pl_catalog_saved(void) { return catalog_saved; }

static bool valid_id(int id) { return id>=PL_TRACK_MIN && id<=PL_TRACK_MAX; }
bool pl_catalog_range(int first, int last) { return valid_id(first) && valid_id(last) && last>=first; }

static void load_profile(void)
{
#ifdef PROTONLINK_DESKTOP
    local=desktop_profiles[profile];
#else
    memset(&local,0,sizeof(local));
    nvs_handle_t nvs;
    if(nvs_open("pl_catalog",NVS_READONLY,&nvs)==ESP_OK) {
        char key[8];
        snprintf(key,sizeof(key),"slot%u",(unsigned)profile);
        size_t size=sizeof(encoded);
        if(nvs_get_blob(nvs,key,encoded,&size)!=ESP_OK || !unpack_profile(size)) memset(&local,0,sizeof(local));
        nvs_close(nvs);
    }
#endif
    if(!local.name[0]) {
        if(profile==0) strcpy(local.name,"Default");
        else snprintf(local.name,sizeof(local.name),"Pack %u",(unsigned)profile+1);
    }
}

void pl_catalog_load(void)
{
#ifndef PROTONLINK_DESKTOP
    nvs_handle_t nvs;
    if(nvs_open("pl_catalog",NVS_READONLY,&nvs)==ESP_OK) {
        nvs_get_u8(nvs,"profile",&profile);
        nvs_close(nvs);
    }
    if(profile>=PL_PROFILE_COUNT) profile=0;
#endif
    load_profile();
}

static void pl_catalog_save(void)
{
#ifdef PROTONLINK_DESKTOP
    desktop_profiles[profile]=local;
#else
    nvs_handle_t nvs;
    catalog_saved=false;
    if(nvs_open("pl_catalog",NVS_READWRITE,&nvs)!=ESP_OK) return;
    char key[8];
    snprintf(key,sizeof(key),"slot%u",(unsigned)profile);
    if(nvs_set_blob(nvs,key,encoded,pack_profile())==ESP_OK &&
       nvs_set_u8(nvs,"profile",profile)==ESP_OK) catalog_saved=nvs_commit(nvs)==ESP_OK;
    nvs_close(nvs);
#endif
}

int pl_catalog_profile(void) { return profile; }
const char *pl_catalog_profile_name(void) { return local.name; }

void pl_catalog_switch_profile(void)
{
    if(!catalog_saved) {
        pl_catalog_save();
        if(!catalog_saved) return;
    }
    profile=(profile+1)%PL_PROFILE_COUNT;
    load_profile();
#ifndef PROTONLINK_DESKTOP
    nvs_handle_t nvs;
    if(nvs_open("pl_catalog",NVS_READWRITE,&nvs)==ESP_OK) {
        if(nvs_set_u8(nvs,"profile",profile)==ESP_OK) nvs_commit(nvs);
        nvs_close(nvs);
    }
#endif
}

static void copy_trimmed(char *out, size_t size, const char *start, size_t length)
{
    while(length && isspace((unsigned char)*start)) { start++; length--; }
    while(length && isspace((unsigned char)start[length-1])) length--;
    if(length>=size) {
        length=size-1;
        while(length && ((unsigned char)start[length]&0xc0)==0x80) length--;
    }
    memcpy(out,start,length);
    out[length]=0;
}

void pl_catalog_name_profile(const char *name)
{
    char trimmed[PL_NAME_SIZE];
    copy_trimmed(trimmed,sizeof(trimmed),name,strlen(name));
    if(trimmed[0]) {
        strcpy(local.name,trimmed);
        pl_catalog_save();
    }
}

static const char *find_title(const track_name_t *names, int id)
{
    for(int i=0;i<PL_CATALOG_LIMIT;i++) if(names[i].id==id && names[i].title[0]) return names[i].title;
    return NULL;
}

const char *pl_catalog_title(int id, int first, char fallback[24])
{
    const char *title=find_title(local.titles,id);
    if(!title) title=find_title(synced,id);
    if(title) return title;
    snprintf(fallback,24,"Track %d",id-first+1);
    return fallback;
}

static bool put_title(track_name_t *names, int id, const char *value, size_t length)
{
    int entry=-1;
    for(int i=0;i<PL_CATALOG_LIMIT;i++) {
        if(names[i].id==id) { entry=i; break; }
        if(names[i].id==0 && entry<0) entry=i;
    }
    if(entry<0) return false;
    copy_trimmed(names[entry].title,sizeof(names[entry].title),value,length);
    names[entry].id=names[entry].title[0]?(uint16_t)id:0;
    return true;
}

bool pl_catalog_edit(int id, const char *title)
{
    if(!valid_id(id) || !put_title(local.titles,id,title,strlen(title))) return false;
    pl_catalog_save();
    return true;
}

static bool csv_title(const char *start, const char *end, char title[PL_TITLE_SIZE])
{
    if(start==end || *start!='"') {
        if(memchr(start,',',(size_t)(end-start))) return false;
        copy_trimmed(title,PL_TITLE_SIZE,start,(size_t)(end-start));
        return true;
    }
    char decoded[PL_TITLE_SIZE+4];
    size_t length=0;
    for(const char *p=start+1;p<end;p++) {
        if(*p=='"') {
            if(p+1==end || p[1]!='"') {
                while(++p<end && isspace((unsigned char)*p)) {}
                if(p!=end) return false;
                copy_trimmed(title,PL_TITLE_SIZE,decoded,length);
                return true;
            }
            p++;
        }
        if(length<sizeof(decoded)) decoded[length++]=*p;
    }
    return false;
}

static bool import_json(const char *input, int first, int last)
{
    if(!pl_json_depth_ok(input)) return false;
    cJSON *root=cJSON_ParseWithOpts(input,NULL,true);
    const cJSON *list=cJSON_IsArray(root)?root:cJSON_GetObjectItemCaseSensitive(root,"tracks");
    uint8_t seen[(PL_TRACK_MAX-PL_TRACK_MIN+8)/8]={0};
    bool valid=cJSON_IsArray(list);
    const cJSON *entry;
    cJSON_ArrayForEach(entry,list) {
        const cJSON *number=cJSON_GetObjectItemCaseSensitive(entry,"id");
        const cJSON *title=cJSON_GetObjectItemCaseSensitive(entry,"title");
        if(!valid || !cJSON_IsNumber(number) || number->valuedouble<first || number->valuedouble>last ||
           number->valuedouble!=number->valueint || !cJSON_IsString(title)) { valid=false; break; }
        unsigned index=(unsigned)(number->valueint-PL_TRACK_MIN);
        if(seen[index/8] & (1u<<(index%8)) ||
           !put_title(staged,number->valueint,title->valuestring,strlen(title->valuestring))) { valid=false; break; }
        seen[index/8]|=1u<<(index%8);
    }
    cJSON_Delete(root);
    return valid;
}

bool pl_catalog_import(const char *text, int first, int last, bool sync)
{
    if(!pl_catalog_range(first,last)) return false;
    if(sync) memset(staged,0,sizeof(staged));
    else memcpy(staged,local.titles,sizeof(staged));
    const unsigned char *start=(const unsigned char *)text;
    if(start[0]==0xef && start[1]==0xbb && start[2]==0xbf) text+=3;
    const char *nonwhite=text+strspn(text," \t\r\n");
    if(!sync && (*nonwhite=='[' || *nonwhite=='{')) {
        if(!import_json(nonwhite,first,last)) return false;
        memcpy(local.titles,staged,sizeof(staged));
        pl_catalog_save();
        return true;
    }
    int positional=first;
    bool csv=!sync && (!strncmp(text,"id,",3) || !strncmp(text,"ID,",3));
    bool m3u=!sync && !strncmp(text,"#EXTM3U",7);
    char pending[PL_TITLE_SIZE]="";
    uint8_t seen[(PL_TRACK_MAX-PL_TRACK_MIN+8)/8]={0};
    for(const char *line=text;*line;) {
        const char *end=line+strcspn(line,"\r\n");
        const char *next=end;
        if(*next=='\r') next++;
        if(*next=='\n') next++;
        const char *title=line;
        size_t length=(size_t)(end-line);
        int id=positional;
        bool explicit_id=false;
        char decoded[PL_TITLE_SIZE];
        if(csv && line==text) { line=next; continue; }
        if(m3u && (!length || *line=='#')) {
            if(length>=8 && !strncmp(line,"#EXTINF:",8)) {
                const char *comma=memchr(line,',',length);
                if(comma) copy_trimmed(pending,sizeof(pending),comma+1,(size_t)(end-comma-1));
            }
            line=next;
            continue;
        }
        if(m3u) {
            if(pending[0]) {
                strcpy(decoded,pending);
                pending[0]=0;
                title=decoded;
                length=strlen(decoded);
            } else {
                const char *dot=NULL;
                for(const char *p=line;p<end;p++) {
                    if(*p=='/' || *p=='\\') { title=p+1; dot=NULL; }
                    else if(*p=='.') dot=p;
                }
                length=(size_t)((dot?dot:end)-title);
            }
        } else if(length>=4 && isdigit((unsigned char)*line)) {
            char *after;
            long prefix=strtol(line,&after,10);
            if(after<end && after-line>=3 && after-line<=4) {
                if(!sync && *after==',') {
                    if(prefix<first || prefix>last || !csv_title(after+1,end,decoded)) return false;
                    id=(int)prefix;
                    title=decoded;
                    length=strlen(decoded);
                    explicit_id=true;
                } else if(sync && *after==' ') {
                    title=after+1;
                    length=(size_t)(end-title);
                }
            }
        }
        if(csv && !explicit_id) return false;
        if(id<first || id>last) return false;
        unsigned index=(unsigned)(id-PL_TRACK_MIN);
        if(seen[index/8] & (1u<<(index%8))) return false;
        seen[index/8]|=1u<<(index%8);
        if(!put_title(staged,id,title,length)) return false;
        if(!explicit_id) positional++;
        line=next;
    }
    memcpy(sync?synced:local.titles,staged,sizeof(staged));
    if(!sync) pl_catalog_save();
    return true;
}

bool pl_catalog_favorite(int id)
{
    if(!valid_id(id)) return false;
    unsigned i=(unsigned)(id-PL_TRACK_MIN);
    return (local.favorites[i/8] & (1u<<(i%8)))!=0;
}

void pl_catalog_star(int id)
{
    if(!valid_id(id)) return;
    unsigned i=(unsigned)(id-PL_TRACK_MIN);
    local.favorites[i/8]^=1u<<(i%8);
    pl_catalog_save();
}

static bool includes_folded(const char *value, const char *search)
{
    if(!*search) return true;
    for(;*value;value++) {
        size_t i=0;
        while(value[i] && search[i] &&
              tolower((unsigned char)value[i])==tolower((unsigned char)search[i])) i++;
        if(!search[i]) return true;
    }
    return false;
}

bool pl_catalog_match(int id, int first, const char *search)
{
    char fallback[24], number[12], query[PL_TITLE_SIZE];
    copy_trimmed(query,sizeof(query),search,strlen(search));
    snprintf(number,sizeof(number),"%d",id);
    return includes_folded(number,query) || includes_folded(pl_catalog_title(id,first,fallback),query);
}

const char *pl_catalog_playlist_name(int slot)
{
    return slot>=0 && slot<PL_PLAYLIST_LIMIT?local.playlists[slot].name:"";
}
int pl_catalog_playlist_track(int slot, int offset)
{
    if(slot<0 || slot>=PL_PLAYLIST_LIMIT || offset<0 || offset>=PL_PLAYLIST_TRACKS) return 0;
    return local.playlists[slot].ids[offset];
}

bool pl_catalog_save_playlist(int slot, const char *name, const char *ids, int first, int last)
{
    if(slot<0 || slot>=PL_PLAYLIST_LIMIT || !pl_catalog_range(first,last)) return false;
    playlist_t next={0};
    copy_trimmed(next.name,sizeof(next.name),name,strlen(name));
    if(next.name[0]) {
        int count=0;
        for(const char *p=ids;*p;) {
            while(isspace((unsigned char)*p) || *p==',') p++;
            if(!*p) break;
            char *end;
            long id=strtol(p,&end,10);
            if(end==p || id<first || id>last || (*end && *end!=',' && !isspace((unsigned char)*end))) return false;
            bool duplicate=false;
            for(int i=0;i<count;i++) if(next.ids[i]==id) duplicate=true;
            if(!duplicate) {
                if(count>=PL_PLAYLIST_TRACKS) return false;
                next.ids[count++]=(uint16_t)id;
            }
            p=end;
        }
        if(!count) return false;
    }
    local.playlists[slot]=next;
    pl_catalog_save();
    return true;
}
