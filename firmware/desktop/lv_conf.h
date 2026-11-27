#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_USE_COLOR_FILTER 1
/* Desktop rendering uses the host heap rather than the embedded LVGL pool. */
#ifdef PROTONLINK_BOUNDED_MEMORY
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (2U * 1024U * 1024U)
#else
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#endif
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH "SDL.h"
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_LOG 1
#define LV_LOG_PRINTF 1
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
#endif
