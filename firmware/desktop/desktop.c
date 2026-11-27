#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "ui.h"
#include "mock.h"
#include "control.h"
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
    advance(1200);
    if(!protonlink_snapshot(&s) || s.master!=20 || s.effects!=25 || s.music!=15) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    if(!press_button("TEMPORARY 50% AUDIO REDUCTION",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    advance(1200);
    if(!press_button("RESTORE PREVIOUS AUDIO",100)) {fprintf(stderr,"Subsystem check failed at line %d\n",__LINE__); return false;}
    advance(1200);
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

static int run_smoke(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(410, 502);
    if (!display) return 1;
    lv_display_set_buffers(display, smoke_buffer, NULL, sizeof(smoke_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, smoke_flush);
    protonlink_ui_create();
    lv_indev_t *pointer=lv_indev_create();
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer,smoke_read);
    lv_indev_set_display(pointer,display);
    advance(100);
    if(pl_is_unlocked()) return 1;
    touch(205,250,true,2200); touch(205,250,false,100);
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
    if(!check_menus()) { fprintf(stderr,"Subsystem regression failed\n"); return 1; }
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
        SDL_Delay(5);
    }
    SDL_DelEventWatch(watch_events,NULL);
    return 0;
}
