#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "ui.h"
#include "mock.h"
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
static int run_smoke(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(410, 502);
    if (!display) return 1;
    lv_display_set_buffers(display, smoke_buffer, NULL, sizeof(smoke_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, smoke_flush);
    protonlink_ui_create();
    for (unsigned elapsed = 0; elapsed < 2000; elapsed += 5) {
        if (elapsed == 500) mock_scenario(0);
        if (elapsed == 1000) mock_scenario(2);
        if (elapsed == 1500) mock_scenario(3);
        lv_tick_inc(5);
        lv_timer_handler();
    }
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
