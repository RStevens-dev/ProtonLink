#include "faces.h"
#include "fonts/watch_fonts.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.141592653589793
#define GREEN 0x83e5a4
#define AMBER 0xefba69
#define STEEL 0x374e58
#define BLUE 0x76c9e7
#define WHITE 0xeaf4f3
#define DIM 0x83999f
#define RED 0xff574b

static bool contact_labels=true;
static uint32_t marquee_color=AMBER;

static const char *const face_names[]={
    "PKE Scanner","Cyclotron Reactor","CRT Terminal","Pack Blueprint","Analog Instrument",
    "Digital Cassette","Containment Grid","Power Cell","Electronic Marquee","Minimal Field","Mission Control"
};

typedef struct {
    pl_face_t style;
    protonlink_status_t status;
    char clock[16], date[24];
    double time;
} frame_t;

typedef struct { lv_layer_t *layer; int x,y; } surface_t;

const char *pl_face_name(pl_face_t face) { return face_names[face>=0 && face<PL_FACE_COUNT?face:PL_FACE_PKE]; }

void pl_face_options(bool labels, uint32_t color)
{
    contact_labels=labels;
    marquee_color=color;
}

static void rect(surface_t g,int x,int y,int w,int h,int radius,uint32_t fill,uint32_t edge)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(fill);
    d.bg_opa=fill?LV_OPA_COVER:LV_OPA_TRANSP;
    d.border_width=edge?1:0;
    d.border_color=lv_color_hex(edge);
    d.radius=radius;
    lv_area_t area={g.x+x,g.y+y,g.x+x+w-1,g.y+y+h-1};
    lv_draw_rect(g.layer,&d,&area);
}

static void circle(surface_t g,int x,int y,int r,uint32_t fill,uint32_t edge)
{
    rect(g,x-r,y-r,2*r+1,2*r+1,LV_RADIUS_CIRCLE,fill,edge);
}

static void line(surface_t g,int x,int y,int x2,int y2,uint32_t color,int width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){g.x+x,g.y+y};
    d.p2=(lv_point_precise_t){g.x+x2,g.y+y2};
    d.color=lv_color_hex(color);
    d.width=width;
    lv_draw_line(g.layer,&d);
}

static void spoke(surface_t g,int x,int y,int start,int end,double angle,uint32_t color,int width)
{
    double c=cos(angle),s=sin(angle);
    line(g,x+(int)(start*c),y+(int)(start*s),x+(int)(end*c),y+(int)(end*s),color,width);
}

static void text(surface_t g,const char *value,int x,int y,int width,const lv_font_t *font,
                 uint32_t color,lv_text_align_t align)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font=font;
    d.color=lv_color_hex(color);
    d.align=align;
    d.text=value;
    d.text_local=1;
    lv_area_t area={g.x+x,g.y+y,g.x+x+width-1,g.y+y+font->line_height-1};
    lv_draw_label(g.layer,&d,&area);
}

static void centered(surface_t g,const char *value,int y,const lv_font_t *font,uint32_t color)
{
    text(g,value,0,y,360,font,color,LV_TEXT_ALIGN_CENTER);
}

static void clock_header(surface_t g,const frame_t *f,const char *title,uint32_t color)
{
    centered(g,title,9,&pl_mono_12,color);
    centered(g,f->clock,29,&pl_mono_48,color);
}

static uint32_t condition(const protonlink_status_t *s)
{
    if(!s->online || !strcmp(s->cyclotron,"Critical")) return RED;
    if(!strcmp(s->cyclotron,"Warning") || !strcmp(s->temperature,"Venting")) return AMBER;
    if(!strcmp(s->cyclotron,"Recovery")) return BLUE;
    return s->pack_on?GREEN:DIM;
}

static void lights(surface_t g,int x,int y,int radius,const frame_t *f)
{
    int phase=(int)(f->time*4)%4;
    for(int i=0;i<4;i++) {
        double a=(i+.5)*PI/2;
        circle(g,x+(int)(cos(a)*radius),y+(int)(sin(a)*radius),8,
               f->status.pack_on && i==phase?RED:0x432b2c,0x684a49);
    }
}

static void radar(surface_t g,const frame_t *f)
{
    const protonlink_status_t *s=&f->status;
    clock_header(g,f,"EQUIPMENT STATUS SCAN",GREEN);
    const int x=180,y=221,r=130;
    circle(g,x,y,r+3,0x07150b,0x467958);
    for(int i=1;i<=4;i++) circle(g,x,y,r*i/4,0,0x214f31);
    for(int i=0;i<8;i++) spoke(g,x,y,0,r,i*PI/4,0x214f31,1);
    for(int i=0;i<60;i++) spoke(g,x,y,r-(i%5?3:7),r,i*PI/30,0x467958,1);
    double rate=!strcmp(s->cyclotron,"Critical")?2.7:!strcmp(s->cyclotron,"Warning")?1.8:1.0;
    if(s->online) {
        for(int i=1;i<13;i++) spoke(g,x,y,2,r,f->time*rate-i*.035,i<5?0x24482e:0x183521,1);
        spoke(g,x,y,0,r,f->time*rate,0xa1f9b7,2);
    }
    const char *names[]={"PACK","WAND","RIBBON","LID"};
    uint32_t colors[]={condition(s),!strcmp(s->wand,"Connected")?GREEN:RED,
                       !strcmp(s->cable,"Connected")?GREEN:RED,!s->lid_known?DIM:s->lid_fitted?GREEN:AMBER};
    for(int i=0;i<4;i++) {
        double a=(i+1)*PI/2+PI/4+sin(f->time*.47+i*2.1)*.19;
        int radius=79+(int)(sin(f->time*.35+i*1.8)*12);
        int px=x+(int)(cos(a)*radius),py=y+(int)(sin(a)*radius);
        uint32_t color=s->online?colors[i]:0x526059;
        circle(g,px,py,9,0,color);
        circle(g,px,py,4,color,0);
        if(contact_labels) text(g,names[i],px-38,py-26,76,&pl_mono_12,color,LV_TEXT_ALIGN_CENTER);
    }
    circle(g,x,y,4,s->online?GREEN:RED,0);
}

static void reactor(surface_t g,const frame_t *f,bool analog)
{
    const int x=180,y=220;
    clock_header(g,f,analog?"ELECTROMECHANICAL INSTRUMENT":"CYCLOTRON / FIELD INSTRUMENT",AMBER);
    circle(g,x,y,128,0x161e21,STEEL);
    circle(g,x,y,109,0x0b1316,0x547076);
    for(int i=0;i<60;i++) {
        if(analog && (i<5 || i>55)) continue;
        double a=(i-30)*PI/30+PI*1.5;
        spoke(g,x,y,i%5?113:108,122,a,i%5?DIM:AMBER,1);
    }
    if(analog) {
        double a=(1.0+(f->status.power_level>0?f->status.power_level:0)*.2)*PI;
        spoke(g,x,y,-14,98,a,AMBER,3);
        circle(g,x,y,9,AMBER,STEEL);
        centered(g,"DISCHARGE LEVEL",275,&pl_mono_12,DIM);
    } else {
        circle(g,x,y,74,0x17272a,STEEL);
        lights(g,x,y,88,f);
        char level[12];
        snprintf(level,sizeof(level),"L-%d",f->status.power_level);
        centered(g,f->status.power_level?level:"L--",190,&pl_mono_48,condition(&f->status));
        centered(g,"WAND LEVEL",249,&pl_mono_12,DIM);
    }
}

static void schematic(surface_t g,const frame_t *f)
{
    clock_header(g,f,"EQUIPMENT SCHEMATIC",BLUE);
    for(int i=10;i<360;i+=20) line(g,i,93,i,352,0x183440,1);
    for(int i=93;i<357;i+=20) line(g,10,i,350,i,0x183440,1);
    rect(g,103,121,146,206,7,0x0e212b,BLUE);
    rect(g,114,133,32,159,3,0x142e3c,BLUE);
    for(int i=0;i<10;i++) rect(g,121,141+i*14,18,7,0,
                              f->status.pack_on?0x4aaad2:0x31505d,0);
    circle(g,191,259,49,0x101920,BLUE);
    lights(g,191,259,28,f);
    rect(g,274,155,16,133,4,0x192c33,BLUE);
    line(g,245,315,275,281,BLUE,1);
    line(g,25,137,110,153,BLUE,1);
    text(g,"CELL",20,120,62,&pl_mono_12,BLUE,LV_TEXT_ALIGN_LEFT);
    line(g,233,263,335,295,BLUE,1);
    text(g,"CYC",312,302,42,&pl_mono_12,BLUE,LV_TEXT_ALIGN_LEFT);
}

static void cassette(surface_t g,const frame_t *f)
{
    clock_header(g,f,"AUDIO TRANSPORT",BLUE);
    rect(g,16,115,328,206,12,0x212a2e,0x728189);
    rect(g,30,135,300,96,8,0x0a1217,STEEL);
    for(int i=0;i<2;i++) {
        int x=89+i*182;
        circle(g,x,183,39,0x172427,0x7b9298);
        for(int j=0;j<6;j++) spoke(g,x,183,12,29,j*PI/3+(f->status.playing && !f->status.paused?f->time*3:0),BLUE,2);
        circle(g,x,183,9,0x0a1217,0x7b9298);
    }
    rect(g,133,165,94,36,3,0x342d20,AMBER);
    char track[24];
    snprintf(track,sizeof(track),"TRACK %d",f->status.track);
    centered(g,f->status.track>=500?track:"TRACK --",250,&lv_font_montserrat_24,AMBER);
    centered(g,f->status.paused?"PAUSED":f->status.playing?"PLAYING":"STOPPED",281,&pl_mono_12,DIM);
}

static void containment(surface_t g,const frame_t *f)
{
    clock_header(g,f,"CONTAINMENT / FIELD DISPLAY",GREEN);
    const int x=180,y=222;
    for(int r=28;r<=112;r+=28) circle(g,x,y,r,0,0x1e5444);
    for(int i=0;i<8;i++) {
        double a=i*PI/4+(f->status.pack_on?f->time*.22:0);
        spoke(g,x,y,0,112,a,0x2e8061,1);
        circle(g,x+(int)(cos(a)*112),y+(int)(sin(a)*112),7,i%2?GREEN:BLUE,0);
    }
    circle(g,x,y,26,0x182b24,condition(&f->status));
    centered(g,"FIELD GRID",352,&pl_mono_12,GREEN);
}

static void power_cell(surface_t g,const frame_t *f)
{
    clock_header(g,f,"POWER CELL",BLUE);
    rect(g,27,107,137,240,10,0x0a141b,0x436477);
    for(int i=0;i<13;i++) rect(g,43,121+i*16,104,11,2,
                              i>=13-f->status.power_level*2?BLUE:0x162a35,0);
    text(g,"WAND LEVEL",188,143,155,&pl_mono_12,DIM,LV_TEXT_ALIGN_CENTER);
    char level[12];
    snprintf(level,sizeof(level),"L-%d",f->status.power_level);
    text(g,f->status.power_level?level:"L--",188,177,155,&pl_mono_48,BLUE,LV_TEXT_ALIGN_CENTER);
    text(g,f->status.pack_on?"PACK ON":"STANDBY",188,252,155,&lv_font_montserrat_24,DIM,LV_TEXT_ALIGN_CENTER);
}

static void terminal(surface_t g,const frame_t *f)
{
    rect(g,6,6,348,373,8,0x06150d,0x235335);
    for(int y=14;y<374;y+=7) line(g,11,y,348,y,0x0d271a,1);
    int scan=13+(int)(f->time*65)%354;
    line(g,12,scan,348,scan,0x28543b,2);
    text(g,"PROTONLINK BIOS / R1",22,22,315,&pl_mono_12,GREEN,LV_TEXT_ALIGN_LEFT);
    centered(g,f->clock,56,&pl_mono_48,GREEN);
    char status[72];
    snprintf(status,sizeof(status),"> PACK: %s",f->status.pack_on?"POWERED":"IDLE");
    text(g,status,24,153,310,&lv_font_montserrat_24,GREEN,LV_TEXT_ALIGN_LEFT);
    snprintf(status,sizeof(status),"> LINK: %s",f->status.online?"CONNECTED":"OFFLINE");
    text(g,status,24,205,310,&lv_font_montserrat_24,GREEN,LV_TEXT_ALIGN_LEFT);
    text(g,"> EQUIPMENT SCAN",24,257,310,&lv_font_montserrat_24,GREEN,LV_TEXT_ALIGN_LEFT);
    if((int)(f->time*2)%2==0) rect(g,26,307,14,17,0,GREEN,0);
}

static const uint8_t digits[10][7]={
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,6,8,16,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};
static void marquee(surface_t g,const frame_t *f)
{
    rect(g,6,7,348,365,8,0x12100a,0x635137);
    for(int i=0;i<5;i++) {
        char digit=f->clock[i];
        for(int row=0;row<7;row++) {
            uint8_t bits=digit>='0' && digit<='9'?digits[digit-'0'][row]:
                         digit==':' && (row==2 || row==4)?4:digit=='-' && row==3?31:0;
            for(int col=0;col<5;col++) circle(g,37+i*61+col*10,100+row*11,3,
                                              bits&(16u>>col)?marquee_color:0x302611,0);
        }
    }
    centered(g,"ELECTRONIC MARQUEE",28,&pl_mono_12,marquee_color);
    centered(g,f->status.pack_on?"SYSTEM ACTIVE":"FIELD STANDBY",221,&lv_font_montserrat_24,marquee_color);
    for(int i=0;i<24;i++) circle(g,30+i*13,304,3,i<=(int)(f->time*5)%24 && f->status.pack_on?marquee_color:0x302611,0);
}

static void mission(surface_t g,const frame_t *f)
{
    clock_header(g,f,"MISSION CONTROL",WHITE);
    const char *names[]={"MASTER","EFFECTS","MUSIC"};
    int levels[]={f->status.master,f->status.effects,f->status.music};
    for(int i=0;i<3;i++) {
        int y=114+i*55;
        rect(g,16,y,328,45,5,0x111e23,STEEL);
        text(g,names[i],26,y+12,115,&pl_mono_12,DIM,LV_TEXT_ALIGN_LEFT);
        rect(g,135,y+16,185,12,2,0x25333a,0);
        if(levels[i]>0) rect(g,135,y+16,levels[i]*185/100,12,2,BLUE,0);
    }
    rect(g,16,293,328,55,5,0x111e23,STEEL);
    centered(g,f->status.online?(f->status.pack_on?"PACK ACTIVE":"PACK STANDBY"):"LINK LOST",
             309,&lv_font_montserrat_24,condition(&f->status));
}

static void diagnostics_art(surface_t g,const frame_t *f)
{
    const protonlink_status_t *s=&f->status;
    rect(g,0,0,360,120,6,0x101e23,STEEL);
    rect(g,109,11,142,97,6,0x0d171b,STEEL);
    rect(g,124,24,20,61,3,s->pack_on?BLUE:0x26383e,STEEL);
    circle(g,201,64,34,0x10191d,STEEL);
    lights(g,201,64,24,f);
    rect(g,283,29,14,63,3,!strcmp(s->wand,"Connected")?GREEN:0x713c37,STEEL);
    line(g,253,100,282,85,!strcmp(s->cable,"Connected")?GREEN:RED,2);
    line(g,47,30,126,43,0x65939e,1);
    line(g,224,42,313,19,0x65939e,1);
    text(g,"CELL",17,21,41,&pl_mono_12,BLUE,LV_TEXT_ALIGN_LEFT);
    text(g,"CYC",317,13,35,&pl_mono_12,BLUE,LV_TEXT_ALIGN_LEFT);
    if(s->online) {
        int y=16+(int)(f->time*30)%87;
        line(g,113,y,246,y,0x315660,1);
    }
}

static void transport_art(surface_t g,const frame_t *f)
{
    bool playing=f->status.playing && !f->status.paused;
    rect(g,0,0,360,85,6,0x0e181b,STEEL);
    for(int i=0;i<20;i++) {
        int height=playing?12+(int)((sin(f->time*(3+i%4)+i*.87)+1)*21):6;
        rect(g,14+i*12,67-height,7,height,1,playing?GREEN:0x365148,0);
    }
    circle(g,307,42,34,0x17262a,STEEL);
    for(int i=0;i<3;i++) spoke(g,307,42,12,26,i*2*PI/3+(playing?f->time*2:0),BLUE,5);
    circle(g,307,42,6,AMBER,0);
}

static void draw_face(lv_event_t *e)
{
    lv_obj_t *obj=lv_event_get_target(e);
    const frame_t *f=lv_event_get_user_data(e);
    lv_area_t coords;
    lv_obj_get_coords(obj,&coords);
    surface_t g={lv_event_get_layer(e),coords.x1,coords.y1};
    if(f->style==PL_ART_DIAGNOSTICS) { diagnostics_art(g,f); return; }
    if(f->style==PL_ART_TRANSPORT) { transport_art(g,f); return; }
    rect(g,0,0,360,386,5,0x030709,0);
    switch(f->style) {
    case PL_FACE_CYCLOTRON: reactor(g,f,false); break;
    case PL_FACE_CRT: terminal(g,f); break;
    case PL_FACE_BLUEPRINT: schematic(g,f); break;
    case PL_FACE_ANALOG: reactor(g,f,true); break;
    case PL_FACE_CASSETTE: cassette(g,f); break;
    case PL_FACE_CONTAINMENT: containment(g,f); break;
    case PL_FACE_CELL: power_cell(g,f); break;
    case PL_FACE_MARQUEE: marquee(g,f); break;
    case PL_FACE_MINIMAL:
        centered(g,f->clock,129,&lv_font_montserrat_48,WHITE);
        centered(g,f->date,204,&pl_mono_12,DIM);
        break;
    case PL_FACE_MISSION: mission(g,f); break;
    default: radar(g,f); break;
    }
    const protonlink_status_t *s=&f->status;
    const char *status=!s->online?"TELEMETRY UNAVAILABLE":s->firing?"FIRING":
                        !strcmp(s->temperature,"Venting")?"VENT CYCLE":s->cyclotron;
    centered(g,status,367,&pl_mono_12,condition(s));
}

static void delete_face(lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }

lv_obj_t *pl_face_create(lv_obj_t *parent,int y,int height)
{
    lv_obj_t *obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj,360,height);
    lv_obj_set_pos(obj,25,y);
    frame_t *frame=lv_malloc_zeroed(sizeof(*frame));
    if(frame) {
        lv_obj_set_user_data(obj,frame);
        lv_obj_add_event_cb(obj,draw_face,LV_EVENT_DRAW_MAIN,frame);
        lv_obj_add_event_cb(obj,delete_face,LV_EVENT_DELETE,frame);
    }
    return obj;
}

void pl_face_update(lv_obj_t *face,pl_face_t style,const protonlink_status_t *status,
                    const char *clock,const char *date,bool still)
{
    frame_t *frame=lv_obj_get_user_data(face);
    if(!frame) return;
    frame->style=style;
    frame->status=*status;
    snprintf(frame->clock,sizeof(frame->clock),"%s",clock);
    snprintf(frame->date,sizeof(frame->date),"%s",date);
    frame->time=still?0:lv_tick_get()/1000.0;
    lv_obj_invalidate(face);
}
