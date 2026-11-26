#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "ui.h"
#include "mock.h"
#include <SDL.h>
#include <stdbool.h>
#include <string.h>
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
    bool smoke=argc>1 && !strcmp(argv[1],"--smoke");
    uint32_t start=SDL_GetTicks();
    while(running){
        lv_timer_handler();
        SDL_Delay(5);
        if(smoke && SDL_GetTicks()-start>1500)break;
    }
    SDL_DelEventWatch(watch_events,NULL);
    return 0;
}
