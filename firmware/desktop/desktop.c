#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "ui.h"
#include "mock.h"
#include "control.h"
#include "catalog.h"
#include "faces.h"
#include "options.h"
#include <SDL.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* Render headlessly in CI; SDL dummy video is not a portable renderer. */
static uint32_t smoke_buffer[410 * 32];
static unsigned smoke_flushes;
static void smoke_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    ++smoke_flushes;
    lv_display_flush_ready(display);
}
static lv_point_t touch_point;
static bool touch_pressed;
static void smoke_read(lv_indev_t *indev,lv_indev_data_t *data)
{
    (void)indev; data->point=touch_point; data->state=touch_pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void advance(unsigned ms)
{
    for(unsigned t=0;t<ms;t+=5) { lv_tick_inc(5); lv_timer_handler(); }
}
static void touch(int x,int y,bool pressed,unsigned ms)
{
    touch_point=(lv_point_t){x,y}; touch_pressed=pressed; advance(ms);
}
static lv_obj_t *find_visible_button(lv_obj_t *obj,const char *name)
{
    if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if(lv_obj_check_type(obj,&lv_button_class)) {
        lv_obj_t *child=lv_obj_get_child(obj,0);
        if(child && lv_obj_check_type(child,&lv_label_class) &&
           !strcmp(lv_label_get_text(child),name)) return obj;
    }
    for(uint32_t i=lv_obj_get_child_count(obj);i>0;i--) {
        lv_obj_t *found=find_visible_button(lv_obj_get_child(obj,i-1),name);
        if(found) return found;
    }
    return NULL;
}
static lv_obj_t *find_visible_label(lv_obj_t *obj,const char *text)
{
    if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if(lv_obj_check_type(obj,&lv_label_class) && !strcmp(lv_label_get_text(obj),text)) return obj;
    for(uint32_t i=0;i<lv_obj_get_child_count(obj);i++) {
        lv_obj_t *result=find_visible_label(lv_obj_get_child(obj,i),text);
        if(result) return result;
    }
    return NULL;
}

static bool centered_prompt(const char *text)
{
    lv_obj_t *label=find_visible_label(lv_screen_active(),text);
    if(!label || lv_obj_get_style_text_align(label,LV_PART_MAIN)!=LV_TEXT_ALIGN_CENTER) return false;
    lv_area_t bounds;
    lv_obj_get_coords(label,&bounds);
    return bounds.x1+bounds.x2==409;
}

static bool press_button(const char *name, unsigned duration)
{
    lv_obj_t *button=find_visible_button(lv_screen_active(),name);
    if(!button) {
        fprintf(stderr,"Missing visible button: %s\n",name);
        return false;
    }
    lv_area_t area;
    lv_obj_get_coords(button,&area);
    int x=(area.x1+area.x2)/2, y=(area.y1+area.y2)/2;
    touch(x,y,true,duration);
    touch(x,y,false,350);
    return true;
}

static lv_obj_t *find_visible_slider(lv_obj_t *obj)
{
    if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if(lv_obj_check_type(obj,&lv_slider_class)) return obj;
    for(uint32_t i=0;i<lv_obj_get_child_count(obj);i++) {
        lv_obj_t *found=find_visible_slider(lv_obj_get_child(obj,i));
        if(found) return found;
    }
    return NULL;
}

static bool check_volume_drag(void)
{
    lv_obj_t *slider=find_visible_slider(lv_screen_active());
    protonlink_status_t s;
    if(!slider || !protonlink_snapshot(&s)) return false;
    int original=s.master;
    lv_area_t area;
    lv_obj_get_coords(slider,&area);
    int y=(area.y1+area.y2)/2, width=lv_area_get_width(&area);
    touch(area.x1+width*original/100,y,true,80);
    touch(area.x1+width*72/100,y,true,90);
    touch(area.x1+width*72/100,y,false,250);
    int target=lv_slider_get_value(slider);
    if(target<=original || target%5 || !protonlink_snapshot(&s) || s.master!=original) return false;
    advance(850);
    if(!pl_busy() || !protonlink_snapshot(&s) || s.master<=original || s.master>=target ||
       lv_slider_get_value(slider)!=target) return false;
    advance(4000);
    return !pl_busy() && protonlink_snapshot(&s) && s.master==target && lv_slider_get_value(slider)==target;
}

static bool check_menus(void)
{
    protonlink_status_t s;
    if(!press_button("SYSTEM STANDBY",100) || !press_button("PACK",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    lv_obj_t *vent=find_visible_button(lv_screen_active(),"MANUAL VENT");
    if(!vent || !lv_obj_has_state(vent,LV_STATE_DISABLED)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    lv_obj_t *label=lv_obj_get_child(vent,0);
    lv_color_t base=lv_obj_get_style_text_color(label,LV_PART_MAIN);
    lv_color_t filtered=lv_obj_get_style_text_color_filtered(label,LV_PART_MAIN);
    uint8_t gray=lv_color_luminance(base);
    if(!lv_color_eq(filtered,lv_color_make(gray,gray,gray))) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("MANUAL VENT",1200) || !protonlink_snapshot(&s) ||
       strcmp(s.temperature,"Normal")) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("STARTUP CHECK",100) || !press_button("BACK",100) ||
       !press_button("BACK",100) || !press_button("STATUS",100) || !press_button("BACK",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}

    if(!press_button("MUSIC",100) || !press_button("START",100) ||
       !press_button("PAUSE",100) || !protonlink_snapshot(&s) || !s.playing || !s.paused) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("RESUME",100) || !press_button("TRACK LIBRARY",100) ||
       !press_button("NEXT PAGE",100) || !press_button("PREVIOUS PAGE",100) ||
       !press_button("STAR CURRENT",100) || !press_button("ALL TRACKS",100) ||
       !find_visible_button(lv_screen_active(),"FAVORITES") ||
       !press_button("> 01    Track 1    500  *",100) ||
       !press_button("TRACK LIBRARY",100) || !press_button("  02    Track 2    501",100) ||
       !press_button("STOP",100) || !protonlink_snapshot(&s) || s.playing || s.track!=501) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}

    if(!press_button("BACK",100) || !press_button("MIXER",100) ||
       !press_button("AUDIO PRESETS",100) || !press_button("QUIET   20 / 25 / 15",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    advance(11000);
    if(!protonlink_snapshot(&s) || s.master!=20 || s.effects!=25 || s.music!=15) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("TEMPORARY 50% AUDIO REDUCTION",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    advance(4200);
    if(!press_button("RESTORE PREVIOUS AUDIO",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    advance(4200);
    if(!protonlink_snapshot(&s) || s.master!=20 || s.effects!=25 || s.music!=15) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("BACK",100) || !press_button("REDUCED MOTION: OFF",100) ||
       !find_visible_button(lv_screen_active(),"REDUCED MOTION: ON") ||
       !press_button("FIELD LAYOUT",100) || !press_button("HIDE",100) ||
       !find_visible_button(lv_screen_active(),"SHOW") || !press_button("MOVE UP",100) ||
       !press_button("BACK",100) || !press_button("OPEN INDUSTRIAL CONTROL",100) ||
       find_visible_button(lv_screen_active(),"LIST")) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    puts("Subsystem navigation, desaturation, audio and library regression passed");
    return true;
}

static bool check_track_formats(void)
{
    char fallback[24];
    pl_catalog_load();
    pl_catalog_switch_profile();
    if(!pl_catalog_import("999 Firehouse\r\n501 Proton",500,505,true) ||
       strcmp(pl_catalog_title(500,500,fallback),"Firehouse") ||
       !pl_catalog_import("id,title\r\n502,\"Night, shift\"",500,505,false) ||
       strcmp(pl_catalog_title(502,500,fallback),"Night, shift") ||
       !pl_catalog_import("{\"tracks\":[{\"id\":503,\"title\":\"Ecto Patrol\"}]}",500,505,false) ||
       !pl_catalog_match(503,500,"ecto pa") ||
       !pl_catalog_import("#EXTM3U\n#EXTINF:120,March\n/music/001.ogg",500,505,false) ||
       strcmp(pl_catalog_title(500,500,fallback),"March") ||
       pl_catalog_import("500,Duplicate\n500,Again",500,505,false) ||
       pl_catalog_import("[{\"id\":506,\"title\":\"Invalid\"}]",500,505,false) ||
       strcmp(pl_catalog_title(500,500,fallback),"March") ||
       !pl_catalog_save_playlist(0,"Night Patrol","503,500,503,502",500,505) ||
       pl_catalog_playlist_track(0,0)!=503 || pl_catalog_playlist_track(0,1)!=500 ||
       pl_catalog_playlist_track(0,2)!=502 || pl_catalog_playlist_track(0,3)!=0) return false;
    for(int i=1;i<PL_PROFILE_COUNT;i++) pl_catalog_switch_profile();
    if(strcmp(pl_catalog_title(500,500,fallback),"Firehouse") || *pl_catalog_playlist_name(0)) return false;
    pl_catalog_import("",500,505,true);
    return true;
}

static bool check_optional_telemetry(void)
{
    protonlink_status_t s;
    const char *status="{\"pack\":\"Idle\",\"firing\":\"Idle\",\"volMaster\":20,\"volEffects\":25,\"volMusic\":15,"
                       "\"power\":\"4\",\"musicLooping\":false,\"musicShuffled\":true,\"vgMode\":true,"
                       "\"canChangeStream\":true,\"theme\":\"1984\"}";
    if(!protonlink_decode(status,&s) || s.power_level!=4 || !s.vg_mode || !s.stream_change_reported) return false;
    return s.known_options==((1u<<PL_OPT_REPEAT)|(1u<<PL_OPT_SHUFFLE)) &&
           s.option_values==(1u<<PL_OPT_SHUFFLE) && !s.advertised_options;
}

static bool check_option_guards(void)
{
    protonlink_status_t s;
    if(!protonlink_snapshot(&s) || !pl_option_path(PL_OPT_REPEAT,&s)) return false;
    s.advertised_options&=~(1u<<PL_OPT_REPEAT);
    if(pl_option_path(PL_OPT_REPEAT,&s)) return false;
    s.known_options&=~(1u<<PL_OPT_SHUFFLE);
    if(pl_option_path(PL_OPT_SHUFFLE,&s) || !pl_option_path(PL_OPT_1989,&s)) return false;
    s.pack_on=true;
    if(pl_option_path(PL_OPT_1989,&s) || !pl_option_path(PL_OPT_STASIS,&s)) return false;
    s.firing=true;
    return !pl_option_path(PL_OPT_STASIS,&s) && !pl_option_path(PL_OPT_SMOKE,&s);
}

static bool check_catalog_ui(void)
{
    if(!press_button("SYSTEM STANDBY",100) || !press_button("MUSIC",100) ||
       !press_button("REPEAT ALL",100) || !find_visible_button(lv_screen_active(),"REPEAT ONE") ||
       !press_button("TRACK LIBRARY",100) || !press_button("TRACK TOOLS",100) ||
       !press_button("SYNC DEVICE TITLES",100) || !press_button("BACK",100) ||
       !press_button("SEARCH",100) || !protonlink_ui_paste_text("Firehouse") ||
       !press_button("APPLY",100) ||
       !find_visible_button(lv_screen_active(),"> 02    Firehouse Theme    501") ||
       find_visible_button(lv_screen_active(),"  01    Proton March    500  *")) return false;
    if(!press_button("TRACK TOOLS",100) || !press_button("IMPORT TITLES",100) ||
       !protonlink_ui_paste_text("501,Night Watch") || !press_button("APPLY",100) ||
       !press_button("BACK",100) || !press_button("SEARCH",100)) return false;
    /* Search opens with its previous query. Return through Music to reset it. */
    if(!press_button("BACK",100) || !press_button("RETURN TO MUSIC",100) ||
       !press_button("TRACK LIBRARY",100) ||
       !find_visible_button(lv_screen_active(),"> 02    Night Watch    501")) return false;
    if(!press_button("TRACK TOOLS",100) || !press_button("PLAYLISTS",100) ||
       !press_button("1   Empty",100) || !protonlink_ui_paste_text("Night Patrol")) return false;
    touch(150,140,true,100); touch(150,140,false,100);
    if(!protonlink_ui_paste_text("501,500") || !press_button("APPLY",100) ||
       !press_button("1   Night Patrol",100) ||
       !find_visible_button(lv_screen_active(),"PLAYLIST")) return false;
    if(!press_button("RETURN TO MUSIC",100) || !press_button("BACK",100) ||
       !press_button("EFFECTS",100) || !press_button("MOVIE ERA",100) ||
       !press_button("1989 / GHOSTBUSTERS II",100)) return false;
    protonlink_status_t s;
    return protonlink_snapshot(&s) && !strcmp(s.theme,"1989") &&
           press_button("BACK",100) && press_button("BACK",100) && press_button("SETUP",100) &&
           press_button("DISPLAY SETTINGS",100) && press_button("FACE: PKE Scanner",100) &&
           find_visible_button(lv_screen_active(),"FACE: Cyclotron Reactor");
}

static bool render_faces(void)
{
    lv_obj_t *face=pl_face_create(lv_screen_active(),56,386);
    protonlink_status_t s={.online=true,.pack_on=true,.playing=true,.power_level=3,.track=501};
    for(int i=0;i<PL_FACE_COUNT;i++) {
        unsigned before=smoke_flushes;
        pl_face_update(face,i,&s,"12:34","Thu Nov 26",false);
        advance(60);
        if(smoke_flushes==before) {
            lv_obj_delete(face);
            return false;
        }
    }
    lv_obj_delete(face);
    return true;
}

static int run_smoke(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(410, 502);
    if (!display) return 1;
    lv_display_set_buffers(display, smoke_buffer, NULL, sizeof(smoke_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, smoke_flush);
    if(!check_track_formats() || !check_optional_telemetry() || !check_option_guards()) {
        fprintf(stderr,"Track import or option guard regression failed\n");
        return 1;
    }
    protonlink_ui_create();
    lv_indev_t *pointer=lv_indev_create();
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer,smoke_read);
    lv_indev_set_display(pointer,display);
    advance(100);
    if(pl_is_unlocked()) return 1;
    if(!centered_prompt("HOLD TO UNLOCK")) {
        fprintf(stderr,"Field Lock prompt is off-center\n");
        return 1;
    }
    touch(205,250,true,2200); touch(205,250,false,100);
    if(!centered_prompt("RELEASE, THEN SWIPE LEFT")) {
        fprintf(stderr,"Field Lock swipe prompt is off-center\n");
        return 1;
    }
    touch(300,250,true,100); touch(160,250,true,100); touch(160,250,false,300);
    if(!pl_is_unlocked()) { fprintf(stderr,"Field Lock hold/swipe failed\n"); return 1; }
    /* Quick Control is visible. Resolve the actual pack button positions by
     * their labels so the pointer test survives layout adjustments. */
    const char *names[]={"START","VENT","STOP"};
    protonlink_status_t sample;
    if(!press_button("START",250) || !protonlink_snapshot(&sample) || sample.pack_on) {
        fprintf(stderr,"A short START press changed pack state\n");
        return 1;
    }
    for(int i=0;i<3;i++) {
        if(!press_button(names[i],1200)) return 1;
        if(!protonlink_snapshot(&sample)) return 1;
        if((i==0 && !sample.pack_on) ||
           (i==1 && strcmp(sample.temperature,"Venting")) ||
           (i==2 && sample.pack_on)) {
            fprintf(stderr,"Pack hold command %d failed\n",i); return 1;
        }
        if(i==1) {
            advance(4500);
            if(!protonlink_snapshot(&sample) || strcmp(sample.temperature,"Normal")) {
                fprintf(stderr,"Vent did not recover\n"); return 1;
            }
        }
    }
    puts("Pack START / VENT / STOP pointer regression passed");
    if(!check_volume_drag()) { fprintf(stderr,"Drag-to-target volume regression failed\n"); return 1; }
    puts("Drag-to-target volume stepped through telemetry to the selected level");
    if(!check_menus()) { fprintf(stderr,"Subsystem regression failed\n"); return 1; }
    if(!check_catalog_ui() || !render_faces()) {
        fprintf(stderr,"Catalog navigation or face rendering regression failed\n");
        return 1;
    }
    puts("Track import, search, sync, capability guards, and face rendering passed");
    mock_scenario(0); advance(300);
    if(pl_is_unlocked()) { fprintf(stderr,"Link loss did not lock controls\n"); return 1; }
    mock_scenario(2); advance(300);
    mock_scenario(3); advance(300);
    if (!smoke_flushes) {
        fprintf(stderr, "Smoke test did not render any frames\n");
        return 1;
    }
#ifdef PROTONLINK_BOUNDED_MEMORY
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("LVGL pool: %zu total, %zu free, %zu largest free, %zu peak used bytes\n",
           mem.total_size, mem.free_size, mem.free_biggest_size, mem.max_used);
#endif
    puts("Desktop UI smoke test passed");
    return 0;
}
static bool running=true;
static SDL_atomic_t paste_requested;
static int watch_events(void *data,SDL_Event *event){
    (void)data;
    if(event->type==SDL_QUIT || (event->type==SDL_WINDOWEVENT && event->window.event==SDL_WINDOWEVENT_CLOSE))running=false;
    if(event->type==SDL_KEYDOWN && !event->key.repeat){
        switch(event->key.keysym.sym){
            case SDLK_0:mock_scenario(0);break;
            case SDLK_1:mock_scenario(1);break;
            case SDLK_2:mock_scenario(2);break;
            case SDLK_3:mock_scenario(3);break;
            case SDLK_ESCAPE:running=false;break;
            case SDLK_v:
                if(event->key.keysym.mod & (KMOD_GUI|KMOD_CTRL)) SDL_AtomicSet(&paste_requested,1);
                break;
        }
    }return 1;
}
int main(int argc,char **argv){
    if (argc > 1 && !strcmp(argv[1], "--smoke")) return run_smoke();
    SDL_SetMainReady();
    lv_init();
    lv_display_t *display=lv_sdl_window_create(410,502);
    if(!display)return 1;
    lv_sdl_window_set_title(display,"ProtonLink - SIMULATION ONLY");
    lv_indev_t *mouse=lv_sdl_mouse_create();
    if(!mouse)return 1;
    lv_indev_set_display(mouse,display);
    SDL_AddEventWatch(watch_events,NULL);
    protonlink_ui_create();
    while(running){
        lv_timer_handler();
        if(SDL_AtomicCAS(&paste_requested,1,0)) {
            char *text=SDL_GetClipboardText();
            protonlink_ui_paste_text(text);
            SDL_free(text);
        }
        SDL_Delay(5);
    }
    SDL_DelEventWatch(watch_events,NULL);
    return 0;
}
