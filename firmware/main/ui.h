#pragma once
void protonlink_ui_create(void);
#ifdef PROTONLINK_DESKTOP
#include <stdbool.h>
bool protonlink_ui_paste_text(const char *text);
#endif
