#pragma once
#include <stdbool.h>

/* Bound parser recursion on the watch's UI and network task stacks. */
static inline bool pl_json_depth_ok(const char *text)
{
    int depth=0;
    bool quoted=false, escaped=false;
    for(const char *p=text;*p;p++) {
        if(quoted) {
            if(escaped) escaped=false;
            else if(*p=='\\') escaped=true;
            else if(*p=='"') quoted=false;
        } else {
            if(*p=='"') quoted=true;
            else if(*p=='{' || *p=='[') {
                if(++depth>32) return false;
            } else if((*p=='}' || *p==']') && --depth<0) return false;
        }
    }
    return depth==0 && !quoted;
}
