// Gerald -- a pixel-art virtual pet frog for the Nintendo 3DS.
// citro2d for graphics, NDSP for sound. Pet simulation lives in gerald.c.
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "atlas.h"
#include "gerald.h"
#include "progression.h"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

#define SAVE_DIR  "sdmc:/3ds/gerald"
#define SAVE_PATH "sdmc:/3ds/gerald/save.dat"
#define GENE_PATH "sdmc:/3ds/gerald/generations.dat"

#define RGBA(r, g, b, a) C2D_Color32(r, g, b, a)
#define RGB(r, g, b) C2D_Color32(r, g, b, 255)
#define WHITE_C RGB(255, 255, 255)

// ===========================================================================
// graphics helpers
// ===========================================================================
typedef struct { C3D_Tex *tex; int w, h; } Atlas;
static C2D_SpriteSheet sheetSpr, sheetBg, sheetUi;
static Atlas aSpr, aBg, aUi;

static void draw_region(const Atlas *a, int sx, int sy, int sw, int sh, float x, float y,
                        float scale, u32 tint, float alpha) {
    Tex3DS_SubTexture st;
    st.width = (u16)sw;
    st.height = (u16)sh;
    st.left = sx / (float)a->w;
    st.right = (sx + sw) / (float)a->w;
    st.top = 1.0f - sy / (float)a->h;
    st.bottom = 1.0f - (sy + sh) / (float)a->h;
    C2D_Image img = {a->tex, &st};
    C2D_ImageTint t;
    if (tint) C2D_PlainImageTint(&t, tint, 1.0f);
    else C2D_AlphaImageTint(&t, alpha);
    C2D_DrawImageAt(img, floorf(x), floorf(y), 0.0f, &t, scale, scale);
}

static void spr(int sx, int sy, int sw, int sh, float x, float y, float scale) {
    draw_region(&aSpr, sx, sy, sw, sh, x, y, scale, 0, 1.0f);
}
static void obj(int id, float x, float y, float scale, float alpha) {
    draw_region(&aSpr, id * OBJ_CELL, OBJ_Y, OBJ_CELL, OBJ_CELL, x, y, scale, 0, alpha);
}
static void icon(int id, float x, float y, float scale) {
    draw_region(&aSpr, id * OBJ_CELL, ICON_Y, OBJ_CELL, OBJ_CELL, x, y, scale, 0, 1.0f);
}
static void frog(int stage, int expr, float x, float y, float scale, float alpha) {
    draw_region(&aSpr, expr * FROG_CELL, stage * FROG_CELL, FROG_CELL, FROG_CELL, x, y, scale, 0, alpha);
}

static void rect(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, 0.0f, w, h, c); }
// wooden UI kit (ui.png) is authored at 1x and drawn at 2x like the rest of the pixel art
#define UI(name, dx, dy) draw_region(&aUi, UI_##name##_X, UI_##name##_Y, UI_##name##_W, UI_##name##_H, (dx), (dy), 2, 0, 1.0f)
#define CREAM    RGB(255, 238, 196)
#define WOOD_SH  RGB(48, 26, 10)
#define INK      RGB(72, 42, 18)
#define INK_SH   RGB(238, 216, 162)
#define GOLD     RGB(255, 214, 96)

static void chain(float x, float y0, float y1) {
    for (float y = y0; y < y1; y += 6) {
        rect(x, y, 6, 7, RGB(38, 34, 46));
        rect(x + 1, y + 1, 4, 5, RGB(158, 164, 180));
        rect(x + 1, y + 1, 4, 1, RGB(222, 226, 238));
        rect(x + 2, y + 2, 2, 3, RGB(38, 34, 46));
    }
}

static int text_w(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }
static void text(float x, float y, int scale, u32 col, const char *s) {
    for (; *s; s++) {
        int c = toupper((unsigned char)*s);
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) c = '?';
        if (c != ' ') draw_region(&aSpr, (c - FONT_FIRST) * 8, FONT_Y, 6, 8, x, y, (float)scale, col, 1.0f);
        x += 6 * scale;
    }
}
static void text_s(float x, float y, int scale, u32 col, u32 shadow, const char *s) {
    float off = scale >= 6 ? scale / 2.0f : (float)scale;
    text(x + off, y + off, scale, shadow, s);
    text(x, y, scale, col, s);
}
static void text_c(float cx, float y, int scale, u32 col, u32 shadow, const char *s) {
    text_s(cx - text_w(s, scale) / 2.0f, y, scale, col, shadow, s);
}
// multi-line (\n separated)
static void text_lines(float x, float y, int scale, u32 col, u32 shadow, const char *s) {
    char buf[96];
    strncpy(buf, s, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (char *p = strtok(buf, "\n"); p; p = strtok(NULL, "\n")) {
        text_s(x, y, scale, col, shadow, p);
        y += 8 * scale + 2;
    }
}

// ===========================================================================
// audio (NDSP). Raw PCM16 mono 22.05 kHz files from romfs:/snd
// ===========================================================================
enum {
    SND_RIBBIT, SND_EAT, SND_HAPPY, SND_SAD, SND_PLOP, SND_CLEAN, SND_SELECT, SND_BACK,
    SND_CONFIRM, SND_TONGUE, SND_CATCH, SND_MISS, SND_EVOLVE, SND_HATCH, SND_SICK, SND_ALERT,
    SND_MEDICINE, SND_LIGHTS_OFF, SND_LIGHTS_ON, SND_GAMEOVER, SND_BGM_DAY, SND_BGM_NIGHT, SND_COUNT
};
static const char *sndFile[SND_COUNT] = {
    "ribbit", "eat", "happy", "sad", "plop", "clean", "select", "back", "confirm", "tongue", "catch",
    "miss", "evolve", "hatch", "sick", "alert", "medicine", "lights_off", "lights_on", "gameover",
    "bgm_day", "bgm_night"};

typedef struct { s16 *data; u32 samples; } Snd;
static Snd snd[SND_COUNT];
static bool audioOk = false, musicOn = true, sfxOn = true;
static ndspWaveBuf chanBuf[8];
static int nextChan = 1, bgmPlaying = -1;
#define SND_RATE 22050.0f

static bool load_snd(Snd *s, const char *name) {
    char path[64];
    snprintf(path, sizeof path, "romfs:/snd/%s.raw", name);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }
    s->data = (s16 *)linearAlloc((size_t)sz);
    if (!s->data) { fclose(f); return false; }
    size_t got = fread(s->data, 1, (size_t)sz, f);
    fclose(f);
    s->samples = (u32)(got / 2);
    DSP_FlushDataCache(s->data, (u32)got);
    return true;
}

static void audio_init(void) {
    if (R_FAILED(ndspInit())) return;            // no DSP firmware dumped -> run silently
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(1.0f);
    for (int i = 0; i < SND_COUNT; i++) load_snd(&snd[i], sndFile[i]);
    audioOk = true;
}

static void chan_start(int ch, int id, float vol, bool loop) {
    if (!audioOk || !snd[id].data) return;
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, SND_RATE);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0};
    mix[0] = vol;
    mix[1] = vol;
    ndspChnSetMix(ch, mix);
    memset(&chanBuf[ch], 0, sizeof(ndspWaveBuf));
    chanBuf[ch].data_vaddr = snd[id].data;
    chanBuf[ch].nsamples = snd[id].samples;
    chanBuf[ch].looping = loop;
    ndspChnWaveBufAdd(ch, &chanBuf[ch]);
}

static void sfx(int id) {
    if (!sfxOn) return;
    int ch = nextChan;
    nextChan = nextChan % 7 + 1;
    chan_start(ch, id, 0.9f, false);
}

// keep the right music playing for day/night + the mute setting
static void bgm_update(bool sleeping) {
    if (!audioOk) return;
    int want = sleeping ? SND_BGM_NIGHT : SND_BGM_DAY;
    if (want != bgmPlaying) {
        chan_start(0, want, 0.33f, true);
        bgmPlaying = want;
    }
    ndspChnSetPaused(0, !musicOn);
}

// ===========================================================================
// game state
// ===========================================================================
typedef enum { SCR_TITLE, SCR_MAIN, SCR_FEED, SCR_INFO, SCR_GAME, SCR_HOP } Screen;
typedef enum { TAB_CARE, TAB_EXPLORE, TAB_SHOP, TAB_FAMILY, TAB_DECOR, TAB_COUNT } Page;
typedef struct { int icon; const char *label; } Btn;

static Gerald G;
static GeneSave P;
static Page page = TAB_CARE;
static int shopPage = 0, familyScroll = 0;
static int hopLeft = 0, hopScore = 0, hopTarget = 0, hopSelected = 0;
static const char *const tabs[5] = {"CARE","EXPLORE","SHOP","FAMILY","DECOR"};
static Screen scr = SCR_TITLE;
static int cursor = 0;
static u32 frame = 0;
static char msg[64] = "";
static int msgT = 0;
static int eatT = 0, eatItem = OBJ_CRICKET, cleanT = 0, cleanN = 0, petT = 0, tongueT = 0;
static int lastSave = 0;
static bool lastSleeping = false;

// particles
typedef struct { float x, y, vx, vy; int life, maxLife, spr, onTop; } Part;
static Part parts[32];
static void spawn(int onTop, float x, float y, float vx, float vy, int life, int s) {
    for (int i = 0; i < 32; i++)
        if (parts[i].life <= 0) { parts[i] = (Part){x, y, vx, vy, life, life, s, onTop}; return; }
}
static void burst(int onTop, float x, float y, int n, int s) {
    for (int i = 0; i < n; i++)
        spawn(onTop, x + (rand() % 40) - 20, y + (rand() % 20) - 10, ((rand() % 100) - 50) / 60.0f,
              -0.6f - (rand() % 100) / 120.0f, 50 + rand() % 25, s);
}

static void set_msg(const char *s) {
    strncpy(msg, s, sizeof msg - 1);
    msg[sizeof msg - 1] = 0;
    msgT = 260;
}

static void save_game(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    g_save(&G, SAVE_PATH);
    gene_save(&P, GENE_PATH);
}

static const char *status_text(void) {
    if (G.stage == G_EGG) return "SOMETHING IS WIGGLING\nINSIDE THE EGG...";
    switch (g_need(&G)) {
    case NEED_FAINT: return "GERALD FAINTED!\nGIVE HIM MEDICINE!";
    case NEED_SICK:  return "GERALD IS SICK.\nGIVE HIM MEDS!";
    case NEED_SLEEP: return "GERALD IS SLEEPY.\nTURN THE LIGHTS OFF.";
    case NEED_FOOD:  return "GERALD IS HUNGRY!\nFEED HIM.";
    case NEED_POOP:  return "IT'S MESSY IN HERE.\nTIME TO CLEAN!";
    case NEED_JOY:   return "GERALD IS BORED.\nPLAY WITH HIM!";
    default: break;
    }
    if (G.sleeping) return "ZZZ... GERALD IS\nSLEEPING.";
    return g_mood(&G) == M_HAPPY ? "GERALD IS HAPPY!\nRIBBIT RIBBIT!" : "GERALD IS DOING FINE.";
}

static void act_message(ActResult r) {
    switch (r) {
    case ACT_EGG:           set_msg("SHH... THE EGG IS\nSTILL HATCHING."); break;
    case ACT_FAINTED:       set_msg("GERALD FAINTED!\nGIVE HIM MEDICINE!"); break;
    case ACT_SLEEPING:      set_msg("SHH! GERALD IS SLEEPING.\nTURN THE LIGHTS ON."); break;
    case ACT_FULL:          set_msg("GERALD IS FULL!"); break;
    case ACT_NOT_SICK:      set_msg("GERALD ISN'T SICK."); break;
    case ACT_REVIVED:       set_msg("GERALD IS BACK!\nTAKE GOOD CARE OF HIM!"); break;
    case ACT_NOT_TIRED:     set_msg("NOT SLEEPY YET!"); break;
    case ACT_TOO_TIRED:     set_msg("TOO TIRED TO PLAY!\nLET HIM REST."); break;
    case ACT_TOO_SICK:      set_msg("TOO SICK TO PLAY!"); break;
    case ACT_ALREADY_CLEAN: set_msg("ALREADY SPOTLESS!"); break;
    case ACT_TUMMY_ACHE:    set_msg("TUMMY ACHE! TOO MANY\nSNACKS... GIVE MEDS!"); break;
    default: break;
    }
}

static void handle_events(uint32_t ev) {
    if (!ev) return;
    char b[64];
    if (ev & EV_HATCHED) gene_gain(&P, 10, 15, &G);
    if (ev & EV_EVOLVED) gene_gain(&P, 20, 25, &G);
    if (ev & EV_FAINTED)      { set_msg("GERALD FAINTED!\nGIVE HIM MEDICINE!"); sfx(SND_SAD); }
    else if (ev & EV_EVOLVED) { snprintf(b, sizeof b, "GERALD GREW INTO A\n%s!", g_stage_name(G.stage)); set_msg(b); sfx(SND_EVOLVE); burst(1, 200, 120, 8, OBJ_SPARKLE); }
    else if (ev & EV_HATCHED) { set_msg("GERALD HATCHED!\nSAY HELLO!"); sfx(SND_HATCH); burst(1, 200, 120, 8, OBJ_SPARKLE); }
    else if (ev & EV_SICK)    { set_msg("GERALD GOT SICK!\nGIVE HIM MEDS."); sfx(SND_SICK); }
    else if (ev & EV_WOKE)    { set_msg("GOOD MORNING!\nGERALD WOKE UP."); sfx(SND_LIGHTS_ON); }
    else if (ev & EV_POOPED)  { set_msg("PLOP! GERALD POOPED."); sfx(SND_PLOP); }
    else if (ev & EV_HUNGRY)  { set_msg("GERALD IS HUNGRY!"); sfx(SND_ALERT); }
    else if (ev & EV_SLEEPY)  { set_msg("GERALD IS SLEEPY."); sfx(SND_ALERT); }
    else if (ev & EV_DIRTY)   { set_msg("IT'S GETTING DIRTY..."); sfx(SND_ALERT); }
    else if (ev & EV_SAD)     { set_msg("GERALD FEELS LONELY.\nPLAY WITH HIM!"); sfx(SND_ALERT); }
}

// real time -> simulation, once per second (and big jumps like closing the lid count as "offline")
static void tick(void) {
    int64_t now = (int64_t)time(NULL);
    int64_t el = now - G.last_time;
    if (el < 1) return;
    uint32_t ev;
    if (el <= 5) { G.last_time = now; ev = g_step(&G, (double)el, 1.0); }
    else ev = g_catchup(&G, now);
    handle_events(ev);
}

// ===========================================================================
// menu definitions
// ===========================================================================
static Btn mainBtns[6] = {{ICO_FEED, "FEED"}, {ICO_PLAY, "PLAY"}, {ICO_CLEAN, "CLEAN"},
                          {ICO_SLEEP, "LIGHTS"}, {ICO_MEDS, "MEDS"}, {ICO_INFO, "INFO"}};
static const Btn feedBtns[3] = {{ICO_MEAL, "MEAL"}, {ICO_SNACK, "SNACK"}, {ICO_BACK, "BACK"}};

#define BTN_Y 166
#define BTN_H 70
static int btn_w(int n) { return (308 - (n - 1) * 4) / n; }
static int btn_x(int n, int i) { return 6 + i * (btn_w(n) + 4); }
static int hit_button(int n, int px, int py) {
    if (py < BTN_Y || py > BTN_Y + (n == 6 ? 58 : BTN_H)) return -1;
    for (int i = 0; i < n; i++)
        if (px >= btn_x(n, i) && px < btn_x(n, i) + btn_w(n)) return i;
    return -1;
}

// ===========================================================================
// fly-catching minigame
// ===========================================================================
#define MAX_FLIES 8
#define GAME_FRAMES (60 * 20)
typedef struct { float x, y, vx, vy, ph; int alive, respawn; } Fly;
static Fly flies[MAX_FLIES];
static int gameLeft, gameScore, tongueX, tongueY;

static void fly_spawn(Fly *f) {
    int edge = rand() % 4;
    f->x = edge == 0 ? 8 : (edge == 1 ? 296 : 20 + rand() % 270);
    f->y = edge == 2 ? 44 : (edge == 3 ? 200 : 50 + rand() % 150);
    float sp = 0.9f + (rand() % 100) / 70.0f + gameScore * 0.02f;
    float a = (rand() % 628) / 100.0f;
    f->vx = cosf(a) * sp;
    f->vy = sinf(a) * sp;
    f->ph = (rand() % 628) / 100.0f;
    f->alive = 1;
    f->respawn = 0;
}

static void game_start(void) {
    scr = SCR_GAME;
    gameLeft = GAME_FRAMES;
    gameScore = 0;
    tongueT = 0;
    memset(flies, 0, sizeof flies);
    for (int i = 0; i < 4; i++) fly_spawn(&flies[i]);
    sfx(SND_CONFIRM);
}

static void game_end(void) {
    int old = (int)G.best_score;
    g_finish_game(&G, gameScore);
    gene_gain(&P, gameScore * 4 + 3, gameScore * 3 + 5, &G);
    char b[64];
    if (gameScore > old && gameScore > 0) snprintf(b, sizeof b, "CAUGHT %d FLIES!\nNEW BEST SCORE!", gameScore);
    else snprintf(b, sizeof b, "CAUGHT %d FLIES!\nYUM!", gameScore);
    set_msg(b);
    sfx(gameScore >= 5 ? SND_HAPPY : SND_GAMEOVER);
    petT = 90;
    burst(1, 200, 120, 6, OBJ_HEART);
    scr = SCR_MAIN;
    cursor = 0;
    save_game();
}

static void game_update(u32 kDown, touchPosition tp) {
    gameLeft--;
    if (tongueT > 0) tongueT--;
    int want = 4 + gameScore / 5;
    if (want > MAX_FLIES) want = MAX_FLIES;
    for (int i = 0; i < MAX_FLIES; i++) {
        Fly *f = &flies[i];
        if (!f->alive) {
            if (i < want && ++f->respawn > 20) fly_spawn(f);
            continue;
        }
        f->ph += 0.15f;
        f->x += f->vx + sinf(f->ph) * 0.9f;
        f->y += f->vy + cosf(f->ph * 1.3f) * 0.9f;
        if (f->x < 6) { f->x = 6; f->vx = fabsf(f->vx); }
        if (f->x > 298) { f->x = 298; f->vx = -fabsf(f->vx); }
        if (f->y < 40) { f->y = 40; f->vy = fabsf(f->vy); }
        if (f->y > 206) { f->y = 206; f->vy = -fabsf(f->vy); }
    }
    if (kDown & KEY_TOUCH) {
        int hit = 0;
        for (int i = 0; i < MAX_FLIES && !hit; i++) {
            Fly *f = &flies[i];
            if (!f->alive) continue;
            float dx = (f->x + 16) - tp.px, dy = (f->y + 16) - tp.py;
            if (dx * dx + dy * dy <= 26 * 26) {
                f->alive = 0;
                f->respawn = 0;
                gameScore++;
                tongueT = 8;
                tongueX = (int)f->x + 16;
                tongueY = (int)f->y + 16;
                sfx(SND_TONGUE);
                sfx(SND_CATCH);
                spawn(0, f->x, f->y, 0, -0.8f, 30, OBJ_SPARKLE);
                hit = 1;
            }
        }
        if (!hit) sfx(SND_MISS);
    }
    if (gameLeft <= 0 || (kDown & KEY_B)) game_end();
}

// ===========================================================================
// actions
// ===========================================================================
static void do_meal(bool snack) {
    ActResult r = snack ? g_feed_snack(&G) : g_feed_meal(&G);
    if (r == ACT_OK || r == ACT_TUMMY_ACHE) {
        eatT = 100;
        eatItem = snack ? OBJ_CANDY : OBJ_CRICKET;
        set_msg(snack ? "YUM! A SWEET SNACK!" : "NOM NOM NOM!");
        if (r == ACT_TUMMY_ACHE) act_message(r);
        sfx(SND_SELECT);
        P.care_count++;
        gene_gain(&P, snack ? 4 : 6, snack ? 1 : 3, &G);
        save_game();
    } else {
        act_message(r);
        sfx(SND_MISS);
    }
    scr = SCR_MAIN;
    cursor = 0;
}

static void activate_main(int i) {
    ActResult r;
    switch (i) {
    case 0: sfx(SND_SELECT); scr = SCR_FEED; cursor = 0; break;
    case 1:
        r = g_can_play(&G);
        if (r == ACT_OK) game_start();
        else { act_message(r); sfx(SND_MISS); }
        break;
    case 2:
        cleanN = G.poops;
        r = g_clean(&G);
        if (r == ACT_OK) { cleanT = 80; sfx(SND_CLEAN); set_msg("SPARKLY CLEAN!"); P.care_count++;gene_gain(&P,8,5,&G);save_game(); }
        else { act_message(r); sfx(SND_MISS); }
        break;
    case 3:
        r = g_toggle_lights(&G);
        if (r == ACT_OK) { sfx(G.sleeping ? SND_LIGHTS_OFF : SND_LIGHTS_ON); set_msg(G.sleeping ? "LIGHTS OFF.\nSWEET DREAMS..." : "LIGHTS ON!");gene_gain(&P,3,1,&G);save_game(); }
        else { act_message(r); sfx(SND_MISS); }
        break;
    case 4:
        r = g_medicine(&G);
        if (r == ACT_OK) { sfx(SND_MEDICINE); set_msg("BLEH! BUT HE FEELS\nBETTER NOW."); burst(1, 200, 120, 4, OBJ_CROSS);gene_gain(&P,7,2,&G);save_game(); }
        else if (r == ACT_REVIVED) { sfx(SND_MEDICINE); act_message(r); burst(1, 200, 120, 6, OBJ_HEART);gene_gain(&P,7,2,&G);save_game(); }
        else { act_message(r); sfx(SND_MISS); }
        break;
    case 5: sfx(SND_SELECT); scr = SCR_INFO; break;
    }
}

static void pet_gerald(void) {
    if (G.stage == G_EGG || G.fainted || G.sleeping) return;
    if (petT > 0) return;
    g_pet(&G);
    petT = 45;
    sfx(SND_RIBBIT);
    burst(1, 200, 110, 2, OBJ_HEART);
    gene_gain(&P,1,0,&G);
}

// ==================== Gerald Generations: new actions =====================
static void hatch_nurture(void) {
    if (G.stage != G_EGG) return;
    if (gene_eggcare(&P, &G)) handle_events(g_step(&G, 0.1, 1.0));
    sfx(SND_PLOP);
    set_msg("YOUR EGG WIGGLES!\nKEEP IT WARM!");
    save_game();
}
static void try_shop(int idx) {
    if (idx < 0 || idx >= GENE_SHOP_ITEMS) return;
    if (idx>=2 && (P.owned & (1u<<idx))) {
        gene_equip(&P,idx);
        set_msg("DECORATION CHANGED!");
        sfx(SND_SELECT);
    } else if(gene_buy(&P,&G,idx)) {
        set_msg("PURCHASE COMPLETE!");sfx(SND_CONFIRM);
        burst(0,175,108,5,OBJ_SPARKLE);
    } else {set_msg("NOT ENOUGH COINS,\nOR ITEM UNAVAILABLE.");sfx(SND_MISS);}
    save_game();
}
static void explore_zone(int idx) {
    if (idx<0 || idx>=GENE_ZONES) return;
    int reward=0;
    if (P.level<gene_zone_level[idx]) {set_msg("LEVEL UP TO\nUNLOCK THIS POND!");sfx(SND_MISS);return;}
    if (gene_explore(&P,idx,&reward)) {
        char b[64];snprintf(b,sizeof b,"EXPLORED %s!\n+%d COINS",gene_zones[idx],reward);
        set_msg(b);sfx(SND_HAPPY);burst(1,200,130,9,OBJ_SPARKLE);save_game();
    } else {set_msg("REST A LITTLE BEFORE\nEXPLORING AGAIN!");sfx(SND_MISS);}
}
static void start_hop(void){
    ActResult r=g_can_play(&G);
    if(r!=ACT_OK){act_message(r);sfx(SND_MISS);return;}
    hopLeft=60*20;hopScore=0;hopTarget=rand()%4;hopSelected=0;scr=SCR_HOP;sfx(SND_CONFIRM);
}
static void end_hop(void) {
    g_finish_game(&G,hopScore);
    if((uint32_t)hopScore>P.best_hop)P.best_hop=(uint32_t)hopScore;
    gene_gain(&P,hopScore*5+2,hopScore*4+5,&G);
    char b[64];snprintf(b,sizeof b,"LILY HOP! %d POINTS\n+%d COINS",hopScore,hopScore*4+5);
    set_msg(b);sfx(SND_HAPPY);scr=SCR_MAIN;page=TAB_EXPLORE;save_game();
}
static void update_hop(u32 kd,touchPosition tp) {
    hopLeft--;
    if(kd&KEY_DLEFT)hopSelected=(hopSelected+3)%4;
    if(kd&KEY_DRIGHT)hopSelected=(hopSelected+1)%4;
    if(kd&KEY_TOUCH){
        if(tp.py>56 && tp.py<203){hopSelected=(int)tp.px/80;if(hopSelected>3)hopSelected=3;kd|=KEY_A;}
    }
    if(kd&KEY_A){if(hopSelected==hopTarget){hopScore++;sfx(SND_CATCH);hopTarget=(hopTarget+1+rand()%3)%4;}
                  else sfx(SND_MISS);}
    if(hopLeft<=0||(kd&KEY_B))end_hop();
}
static void import_frog_passport(void) {
    SwkbdState keyboard;char input[16]={0};
    swkbdInit(&keyboard,SWKBD_TYPE_NUMPAD,2,6);
    swkbdSetHintText(&keyboard,"Enter six-digit frog code");
    if(swkbdInputText(&keyboard,input,sizeof input)==SWKBD_BUTTON_CONFIRM){
        if(strlen(input)==6&&gene_visit(&P,(uint32_t)strtoul(input,NULL,10))){
            set_msg("NEW FROG FRIEND!\n+20 FRIENDSHIP COINS!");sfx(SND_HAPPY);save_game();
        }else {set_msg("INVALID OR ALREADY\nVISITED PASSPORT!");sfx(SND_MISS);}
    }
}
static void family_action(int choice) {
    if(choice==0){
        if(gene_descendant(&P,&G,(int64_t)time(NULL))){
            set_msg("WELCOME, NEXT GEN!\nA NEW EGG APPEARS!");sfx(SND_HATCH);burst(1,200,120,10,OBJ_SPARKLE);save_game();
        } else {set_msg("NEED ADULT, LV10\nAND 100 COINS!");sfx(SND_MISS);}
    } else if(choice==1) import_frog_passport();
    else familyScroll=(familyScroll+1)%GENE_HISTORY;
}
static void page_action(int choice) {
    switch(page){
    case TAB_CARE:
        if(G.stage==G_EGG){hatch_nurture();return;}
        if(choice<6)activate_main(choice);
        break;
    case TAB_EXPLORE:
        if(choice<4) explore_zone(choice);
        else if(choice==4)game_start();
        else start_hop();
        break;
    case TAB_SHOP:
        if(choice<4)try_shop(shopPage*4+choice);
        else shopPage=(shopPage+1)%2;
        break;
    case TAB_FAMILY:family_action(choice);break;
    case TAB_DECOR:
        if(choice<6)try_shop(choice+2);
        else page=TAB_SHOP;
        break;
    default:break;
    }
}
// ===========================================================================
// update
// ===========================================================================
static void update(u32 kDown,u32 kHeld,touchPosition tp){
    frame++;
    if(msgT>0)msgT--;
    if(eatT>0){eatT--;if(eatT==60){sfx(SND_EAT);burst(1,200,120,3,OBJ_HEART);}}
    if(cleanT>0)cleanT--;
    if(petT>0)petT--;
    for(int i=0;i<32;i++)if(parts[i].life>0){parts[i].x+=parts[i].vx;parts[i].y+=parts[i].vy;parts[i].life--;}
    if(G.sleeping!=lastSleeping)lastSleeping=G.sleeping;
    bgm_update(G.sleeping);
    if((kHeld&KEY_L)&&(kHeld&KEY_R)){
        if(kDown&KEY_Y)handle_events(g_skip(&G,3600));
        if(kDown&KEY_X)handle_events(g_skip(&G,86400));
    }
    if(kDown&KEY_SELECT){musicOn=!musicOn;sfx(SND_SELECT);}
    if((int)frame-lastSave>60*30){lastSave=(int)frame;save_game();}
    if(scr==SCR_TITLE){if(kDown&(KEY_A|KEY_TOUCH)){scr=SCR_MAIN;sfx(SND_RIBBIT);}return;}
    if(scr==SCR_GAME){game_update(kDown,tp);return;}
    if(scr==SCR_HOP){update_hop(kDown,tp);return;}
    if(scr==SCR_INFO){if(kDown&(KEY_B|KEY_A|KEY_TOUCH))scr=SCR_MAIN;return;}
    if(scr==SCR_FEED){
        if(kDown&KEY_B){scr=SCR_MAIN;return;}
        if(kDown&KEY_DLEFT)cursor=(cursor+2)%3;
        if(kDown&KEY_DRIGHT)cursor=(cursor+1)%3;
        if(kDown&KEY_A){if(cursor==2)scr=SCR_MAIN;else do_meal(cursor==1);}
        if(kDown&KEY_TOUCH){int i=hit_button(3,tp.px,tp.py);if(i>=0){if(i==2)scr=SCR_MAIN;else do_meal(i==1);}}
        return;
    }
    // All main pages share the five navigation tabs.
    if(kDown&KEY_L){page=(Page)((page+TAB_COUNT-1)%TAB_COUNT);cursor=0;sfx(SND_SELECT);}
    if(kDown&KEY_R){page=(Page)((page+1)%TAB_COUNT);cursor=0;sfx(SND_SELECT);}
    if(kDown&KEY_DLEFT){cursor=(cursor+5)%6;sfx(SND_SELECT);}
    if(kDown&KEY_DRIGHT){cursor=(cursor+1)%6;sfx(SND_SELECT);}
    if(kDown&KEY_A)page_action(cursor);
    if(kDown&KEY_B){page=TAB_CARE;cursor=0;}
    if(kDown&KEY_X){scr=SCR_INFO;return;}
    if(kDown&KEY_TOUCH){
        if(tp.py>=28&&tp.py<62){int t=(int)tp.px/64;if(t>=0&&t<5){page=(Page)t;cursor=0;sfx(SND_SELECT);}return;}
        if(page==TAB_CARE){
            if(tp.py>=201){int i=(int)tp.px/53;if(i>5)i=5;page_action(i);}
            else if(tp.px>230&&tp.py>72&&tp.py<176){pet_gerald();}
        }else if(page==TAB_EXPLORE){
            if(tp.py>=197){page_action(tp.px<160?4:5);}
            else if(tp.py>=75&&tp.py<195){int ix=(tp.px>=160),iy=(tp.py>=135);page_action(ix+iy*2);}
        }else if(page==TAB_SHOP){
            if(tp.py>=197)page_action(4);
            else if(tp.py>=72&&tp.py<194){int ix=(tp.px>=160),iy=(tp.py>=133);page_action(ix+iy*2);}
        }else if(page==TAB_FAMILY){
            if(tp.py>=195){page_action(tp.px<110?0:(tp.px<220?1:2));}
        }else if(page==TAB_DECOR){
            if(tp.py>=77&&tp.py<193){int ix=(tp.px/106);if(ix>2)ix=2;int iy=(tp.py>=133);page_action(ix+iy*3);}
            else if(tp.py>=198)page_action(6);
        }
    }
}

// ===========================================================================
// drawing: top screen
// ===========================================================================
static void frog_genetics(int stage,int expr,float x,float y,float scale,float alpha);
static const float poopPos[G_MAX_POOPS][2] = {{96, 168}, {262, 170}, {62, 152}, {296, 150}};

static int pick_expr(void) {
    if (G.stage == G_EGG) {
        double t = G.egg_time;
        if (t < 18) return EX_IDLE;
        if (t < 34) return (frame / 14) % 2 ? EX_IDLE2 : EX_BLINK;           // wobble left / right
        return (frame / 6) % 3 == 0 ? EX_HAPPY : ((frame / 6) % 3 == 1 ? EX_IDLE2 : EX_BLINK);   // crack + wobble
    }
    if (scr == SCR_GAME && tongueT > 0) return EX_TONGUE;
    if (eatT > 0 && eatT <= 60) return (eatT / 8) % 2 ? EX_EAT : EX_IDLE;
    if (petT > 0) return EX_HAPPY;
    switch (g_mood(&G)) {
    case M_FAINT: case M_SICK: return EX_SICK;
    case M_SLEEP: return EX_SLEEP;
    case M_SAD:   return EX_SAD;
    case M_HAPPY: return (frame % 240) < 10 ? EX_BLINK : ((frame / 45) % 3 == 0 ? EX_HAPPY : EX_IDLE2);
    default:      return (frame % 210) < 8 ? EX_BLINK : ((frame / 36) % 2 ? EX_IDLE2 : EX_IDLE);
    }
}

static void gerald_xy(float *x, float *y) {
    *x = 136;
    *y = 84;
    if (G.stage == G_EGG) *y = 80;
    if (G.stage == G_TADPOLE) *y = 104 + 5 * sinf(frame * 0.05f);
    if (G.stage > G_TADPOLE && !G.sleeping && !G.fainted && !G.sick) {
        bool bounce = (scr == SCR_GAME) || petT > 0 || g_mood(&G) == M_HAPPY;
        if (bounce) *y -= fabsf(sinf(frame * (petT > 0 ? 0.22f : 0.09f))) * (petT > 0 ? 10 : 6);
    }
    if (G.sick && !G.fainted) *x += ((frame / 3) % 2) ? 1 : -1;
    if (G.fainted) *y += 10;
}

static void draw_top(void) {
    bool night = G.sleeping;
    draw_region(&aBg, 0, night ? 128 : 0, 200, 120, 0, 0, 2, 0, 1.0f);
    if (night) {
        for (int i = 0; i < 7; i++) {      // fireflies
            float fx = 40 + i * 52 + sinf(frame * 0.021f + i * 2.1f) * 22;
            float fy = 130 + 18 * sinf(frame * 0.017f + i * 1.7f) + (i % 3) * 14;
            float a = 0.5f + 0.5f * sinf(frame * 0.07f + i);
            rect(fx - 1, fy - 1, 3, 3, RGBA(255, 244, 140, (int)(60 + 180 * a)));
            rect(fx, fy, 1, 1, RGBA(255, 255, 220, 255));
        }
    }
    // Explore unlocks are actual scene variants, drawn above the uploaded pond
    // backdrop without discarding the original day/night pixel artwork.
    if(P.zone==1){ // Waterfall: cascading pixel water with white foam
        for(int i=0;i<6;i++){
            float xx=39+i*6;
            rect(xx,51,4,98,RGB(i%2?124:62,183,238));
            for(int j=0;j<3;j++)rect(xx,58+((int)frame*2+j*43+i*19)%82,3,8,RGB(207,244,255));
        }
        for(int i=0;i<7;i++)rect(31+i*10,148+(i%3)*2,8,3,RGB(222,251,254));
    } else if(P.zone==2){ // Moss Cave: shadowy stone and hanging moss
        rect(0,23,400,21,RGB(63,83,92));
        for(int i=0;i<16;i++){
            rect(i*29,26+(i%4)*3,17,5,RGB(49,71,73));
            rect(i*27+5,39,3,12+(i%3)*8,RGB(61,126,85));
        }
    } else if(P.zone==3){ // Blossom: drifting petals over the familiar pond
        for(int i=0;i<17;i++){
            float xx=fmodf(i*39+frame*0.37f,410.0f)-8;
            float yy=30+((i*47+(int)(frame*0.19f))%160);
            rect(xx,yy,6,5,RGB(251,159+(i%3)*15,206));
            rect(xx+1,yy+1,3,3,RGB(255,215,228));
        }
    }
    // lily pad / ripples
    if (G.stage != G_TADPOLE) {
        spr(PAD_X, MISC_Y, PAD_W, PAD_H, 104, 170, 4);
    } else {
        int rf = (frame / 20) % 3;
        draw_region(&aSpr, RIPPLE_X + rf * RIPPLE_W, MISC_Y, RIPPLE_W, RIPPLE_H, 136, 196, 4, 0, 1.0f);
    }
    // poop
    for (int i = 0; i < G.poops && i < G_MAX_POOPS; i++)
        obj(OBJ_POOP0 + ((frame / 20 + i) % 2), poopPos[i][0], poopPos[i][1], 3, 1.0f);
    if (cleanT > 0) {
        for (int i = 0; i < cleanN && i < G_MAX_POOPS; i++) {
            float k = cleanT / 80.0f;
            obj(OBJ_BUBBLE, poopPos[i][0] + 8 * sinf(frame * 0.3f + i), poopPos[i][1] - (1 - k) * 30, 3, k);
            obj(OBJ_SPARKLE, poopPos[i][0] + 16, poopPos[i][1] + 10 * sinf(frame * 0.2f), 2, k);
        }
    }

    // Gerald
    float gx, gy;
    gerald_xy(&gx, &gy);
    int expr = pick_expr();
    int frogStage=(P.rare && G.stage>=G_FROG)?G_ELDER:G.stage;
    frog_genetics(frogStage, expr, gx, gy, 4, G.fainted ? 0.65f : 1.0f);
    if(P.equip_hat==3 && G.stage>=G_FROG){ // blue star hat
        rect(gx+43,gy-10,49,7,RGB(31,54,94));rect(gx+55,gy-31,27,23,RGB(63,161,233));
        obj(OBJ_SPARKLE,gx+58,gy-34,2,1);
    } else if(P.equip_hat==4 && G.stage>=G_FROG){ // sunny straw hat
        rect(gx+32,gy-6,67,9,RGB(126,77,31));rect(gx+47,gy-28,42,23,RGB(243,204,90));
    }
    if(P.equip_decor==5) {obj(OBJ_HEART,34,153,2,1);obj(OBJ_SPARKLE,70,175,2,1);}
    if(P.equip_decor==6) {obj(OBJ_CANDY,42,165,3,1);}
    if(P.equip_decor==8 && night){obj(OBJ_SPARKLE,310,125,2,1);obj(OBJ_SPARKLE,81,105,2,1);}
    if(P.equip_buddy==7){
        rect(294,185,25,16,RGB(250,202,75));rect(305,173,15,17,RGB(250,202,75));
        rect(317,181,9,5,RGB(240,129,40));rect(313,179,3,3,RGB(25,34,44));
    }

    // food flying in
    if (eatT > 60) {
        float t = (100 - eatT) / 40.0f;
        obj(eatItem, 184, -20 + t * 150, 2, 1.0f);
    }
    // sleeping Zs
    if (G.sleeping) {
        for (int i = 0; i < 2; i++) {
            int ph = (frame + i * 45) % 90;
            obj(OBJ_ZZZ, 250 + ph / 3 + i * 10, 96 - ph * 0.6f, 2, 1.0f - ph / 90.0f);
        }
    }
    // need bubble
    Need nd = g_need(&G);
    if (nd != NEED_NONE && scr != SCR_GAME) {
        int ic = OBJ_EXCL;
        switch (nd) {
        case NEED_FAINT: ic = OBJ_SKULL; break;
        case NEED_SICK: ic = OBJ_CROSS; break;
        case NEED_SLEEP: ic = OBJ_ZZZ; break;
        case NEED_FOOD: ic = OBJ_CRICKET; break;
        case NEED_POOP: ic = OBJ_POOP0; break;
        case NEED_JOY: ic = OBJ_NOTE; break;
        default: break;
        }
        float bx = 262, by = 52 + 3 * sinf(frame * 0.1f);
        rect(bx - 2, by - 2, 52, 52, RGB(38, 32, 58));
        rect(bx, by, 48, 48, RGB(250, 250, 255));
        rect(bx + 4, by + 48, 10, 6, RGB(38, 32, 58));
        rect(bx + 6, by + 46, 8, 6, RGB(250, 250, 255));
        obj(ic, bx + 8, by + 8, 2, 1.0f);
    }
    // particles
    for (int i = 0; i < 32; i++) {
        Part *p = &parts[i];
        if (p->life > 0 && p->onTop) obj(p->spr, p->x, p->y, 3, p->life < 12 ? p->life / 12.0f : 1.0f);
    }

    // HUD strip
    rect(0, 0, TOP_W, 22, RGBA(16, 22, 40, 170));
    text_s(8, 4, 2, RGB(255, 230, 109), RGB(30, 40, 30), "GERALD");
    char b[40], age[24];
    int s = (int)G.age;
    if (G.stage == G_EGG) snprintf(age, sizeof age, "EGG");
    else if (s < 3600) snprintf(age, sizeof age, "%dM", s / 60);
    else if (s < 86400) snprintf(age, sizeof age, "%dH", s / 3600);
    else snprintf(age, sizeof age, "%dD", s / 86400);
    snprintf(b,sizeof b,"GEN%u LV%u  %s",(unsigned)P.generation,(unsigned)P.level,age);
    text_s(TOP_W - 8 - text_w(b, 2), 4, 2, WHITE_C, RGB(30, 40, 30), b);
    icon(musicOn ? ICO_SOUND_ON : ICO_SOUND_OFF, TOP_W - 8 - text_w(b, 2) - 22, 3, 1);

    if (scr == SCR_GAME) {
        snprintf(b, sizeof b, "FLIES %d", gameScore);
        text_c(200, 30, 3, RGB(255, 255, 255), RGB(30, 40, 60), b);
        float w = 360.0f * gameLeft / GAME_FRAMES;
        rect(18, 226, 364, 10, RGB(38, 32, 58));
        rect(20, 228, w, 6, gameLeft < 300 ? RGB(255, 100, 90) : RGB(110, 220, 110));
    }
}

// ===========================================================================
// drawing: bottom screen
// ===========================================================================
static u32 bar_color(float v) {
    return v > 50 ? RGB(120, 224, 100) : (v > 25 ? RGB(255, 208, 64) : RGB(255, 92, 80));
}

static void draw_stat(int row, const char *label, float v) {
    float y = 20 + row * 17;
    text_s(14, y, 2, CREAM, WOOD_SH, label);
    rect(90, y - 1, 124, 16, RGB(30, 17, 8));                // recessed track
    rect(91, y + 14, 122, 1, RGB(120, 76, 40));
    int segs = (int)ceilf(v / 10.0f);
    for (int i = 0; i < 10; i++) {
        float sx = 92 + i * 12;
        rect(sx, y + 1, 11, 12, RGB(58, 36, 20));
        if (i < segs) {
            rect(sx, y + 1, 11, 12, bar_color(v));
            rect(sx, y + 1, 11, 3, RGBA(255, 255, 255, 80));
            rect(sx, y + 10, 11, 3, RGBA(0, 0, 0, 50));
        }
    }
}

static void draw_buttons(const Btn *b, int n) {
    int w = btn_w(n);
    for (int i = 0; i < n; i++) {
        int x = btn_x(n, i);
        bool sel = (i == cursor);
        if (n == 6) {                                       // round metal buttons + caption
            float by = BTN_Y + (sel ? -2 : 0) + (sel ? 1.0f : 0.0f) * (((frame / 24) % 2) ? 1 : 0);
            if (sel) UI(BTN_ROUND_SEL, x + 2, by); else UI(BTN_ROUND, x + 2, by);
            icon(b[i].icon, x + 8, by + 6, 2);
            text_c(x + w / 2.0f, BTN_Y + 49, 1, sel ? GOLD : CREAM, WOOD_SH, b[i].label);
        } else {                                            // hanging wooden signs
            float by = BTN_Y + (sel ? -2 : 0);
            if (sel) UI(BTN_SIGN_SEL, x, by); else UI(BTN_SIGN, x, by);
            icon(b[i].icon, x + (w - 32) / 2, by + 8, 2);
            text_c(x + w / 2.0f, by + 46, 2, sel ? GOLD : CREAM, WOOD_SH, b[i].label);
        }
    }
}

static int portrait_expr(void) {
    if (G.stage == G_EGG) return EX_IDLE;
    switch (g_mood(&G)) {
    case M_FAINT: case M_SICK: return EX_SICK;
    case M_SLEEP: return EX_SLEEP;
    case M_SAD: return EX_SAD;
    case M_HAPPY: return EX_HAPPY;
    default: return (frame % 200) < 8 ? EX_BLINK : EX_IDLE;
    }
}

/* Adult body-color genes and rare evolutions use dedicated frames in
   the expanded 512px atlas; other stages preserve the uploaded originals. */
static void frog_genetics(int stage,int expr,float x,float y,float scale,float alpha){
    int sy=stage*FROG_CELL;
    if(stage>=G_FROG){
        if(P.rare>=1&&P.rare<=3)sy=328+(int)(P.rare-1)*32;
        else if(P.color>0&&P.color<4)sy=232+(int)(P.color-1)*32;
    }
    draw_region(&aSpr,expr*FROG_CELL,sy,FROG_CELL,FROG_CELL,x,y,scale,0,alpha);
}
static void draw_portrait(float fx, float fy) {
    UI(FRAME, fx, fy);
    frog_genetics(G.stage, portrait_expr(), fx + 10, fy + 12 + (G.stage == G_TADPOLE ? 8 : 0), 2, 1.0f);
}

static void timber(float x,float y,float w,float h,bool selected){
    rect(x,y+3,w,h,RGB(42,25,17));
    rect(x+2,y+2,w-4,h-5,RGB(113,66,33));
    for(int i=0;i<(int)h;i+=9){
        rect(x+4,y+5+i,w-8,1,RGB(137,84,44));
        rect(x+10+(i*13)%(int)(w>40?w-28:8),y+8+i,16,1,RGB(82,47,29));
    }
    rect(x+4,y+4,3,3,RGB(223,169,86));
    rect(x+w-9,y+4,3,3,RGB(223,169,86));
    if(selected){rect(x+2,y+2,w-4,3,GOLD);rect(x+2,y+2,3,h-4,GOLD);rect(x+w-5,y+2,3,h-4,GOLD);}
}
static void nav_tabs(void){
    char b[40];
    timber(2,2,316,24,false);
    snprintf(b,sizeof b,"GERALD  GEN %u  LV %u",(unsigned)P.generation,(unsigned)P.level);
    text_s(10,8,1,GOLD,WOOD_SH,b);
    snprintf(b,sizeof b,"%u C",(unsigned)P.coins);
    text_s(319-text_w(b,1)-8,8,1,CREAM,WOOD_SH,b);
    for(int i=0;i<5;i++){
        int x=i*64+1;bool chosen=(int)page==i;
        timber(x,30,62,31,chosen);
        text_c(x+31,41,1,chosen?GOLD:CREAM,WOOD_SH,tabs[i]);
    }
}
static void draw_button_wood(float x,float y,float w,float h,const char *label,bool selected){
    timber(x,y,w,h,selected);
    text_c(x+w/2,y+h/2-4,1,selected?GOLD:CREAM,WOOD_SH,label);
}
static void draw_care_page(void){
    timber(4,66,312,133,false);
    char b[64];
    snprintf(b,sizeof b,"%s  %s",g_stage_name(G.stage),P.rare?"RARE":"");
    text_s(12,72,1,GOLD,WOOD_SH,b);
    static const char *const labels[5]={"BELLY","JOY","CLEAN","ENERGY","HEALTH"};
    float values[5]={G.food,G.joy,G.clean,G.energy,G.health};
    for(int i=0;i<5;i++){
        int y=88+i*18;
        text_s(12,y,1,CREAM,WOOD_SH,labels[i]);
        rect(68,y-1,151,13,RGB(38,26,20));
        int fill=(int)(147*values[i]/100);
        rect(70,y+1,fill,9,bar_color(values[i]));
        rect(70,y+1,fill,2,RGBA(255,255,255,60));
    }
    draw_portrait(222,88);
    if(G.stage==G_EGG){
        snprintf(b,sizeof b,"EGG: %d%%  TAP TO NURTURE",(int)(G.egg_time/45.0*100)>100?100:(int)(G.egg_time/45.0*100));
    }else snprintf(b,sizeof b,"XP %u/%u   CARE %u",(unsigned)P.xp,(unsigned)gene_need_xp(&P),(unsigned)P.care_count);
    text_s(13,182,1,GOLD,WOOD_SH,b);
    static const char *const actions[]={"FEED","PLAY","CLEAN","SLEEP","MEDS","INFO"};
    for(int i=0;i<6;i++){
        float x=i*53+2;
        timber(x,204,51,32,false);
        icon(mainBtns[i].icon,x+17,205,1);
        text_c(x+25,226,1,CREAM,WOOD_SH,actions[i]);
    }
}
static void draw_explore_page(void){
    for(int i=0;i<4;i++){
        int x=(i%2)*158+3,y=69+(i/2)*64;
        bool open=P.level>=gene_zone_level[i];
        timber(x,y,154,61,(int)P.zone==i);
        obj(i==0?OBJ_HEART:(i==1?OBJ_DROP:(i==2?OBJ_SPARKLE:OBJ_NOTE)),x+11,y+10,2,1);
        text_s(x+53,y+17,1,open?CREAM:RGB(170,150,135),WOOD_SH,gene_zones[i]);
        char b[40];snprintf(b,sizeof b,open?"TAP TO EXPLORE":"UNLOCK LV %u",(unsigned)gene_zone_level[i]);
        text_s(x+14,y+43,1,open?GOLD:CREAM,WOOD_SH,b);
    }
    draw_button_wood(4,199,153,37,"FLY CATCHER",false);
    draw_button_wood(163,199,153,37,"LILY HOP",false);
}
static void draw_shop_page(void){
    char b[72];
    for(int i=0;i<4;i++){
        int idx=shopPage*4+i, x=(i%2)*158+3,y=69+(i/2)*64;
        bool owned=idx>=2&&(P.owned&(1u<<idx));
        timber(x,y,154,60,owned);
        text_s(x+10,y+11,1,GOLD,WOOD_SH,gene_shop_names[idx]);
        snprintf(b,sizeof b,owned?"OWNED - TAP TO EQUIP":"BUY: %u COINS",(unsigned)gene_shop_prices[idx]);
        text_s(x+10,y+35,1,CREAM,WOOD_SH,b);
    }
    draw_button_wood(3,200,314,36,shopPage==0?"NEXT ITEMS  >":"<  PREVIOUS ITEMS",false);
}
static void draw_family_page(void){
    timber(3,66,314,131,false);
    char b[80];
    snprintf(b,sizeof b,"GEN %u  -  %s / %s",(unsigned)P.generation,gene_colors[P.color],gene_personalities[P.personality]);
    text_s(12,74,1,GOLD,WOOD_SH,b);
    snprintf(b,sizeof b,"GENETICS: %s  LUCK %u",gene_patterns[P.pattern],(unsigned)P.luck);
    text_s(12,92,1,CREAM,WOOD_SH,b);
    text_s(12,115,1,CREAM,WOOD_SH,"FAMILY TREE (EARLIEST -> NEWEST)");
    for(unsigned int i=0;i<4;i++){
        int idx=(int)P.history_count-1-familyScroll-(int)i;
        if(idx<0)break;
        if(idx>=(int)P.history_count)continue;
        int x=11+i*78;
        timber(x,132,73,41,false);
        snprintf(b,sizeof b,"GEN %u",(unsigned)P.family[idx].generation);
        text_s(x+5,140,1,GOLD,WOOD_SH,b);
        snprintf(b,sizeof b,"LV%u %s",(unsigned)P.family[idx].level,P.family[idx].rare?"RARE":"FROG");
        text_s(x+5,157,1,CREAM,WOOD_SH,b);
    }
    snprintf(b,sizeof b,"YOUR FROG CODE: %06u   FRIENDS: %u",(unsigned)gene_passport(&P),(unsigned)P.friends);
    text_s(12,181,1,GOLD,WOOD_SH,b);
    draw_button_wood(2,201,105,36,"NEXT GEN",false);
    draw_button_wood(109,201,105,36,"IMPORT CODE",false);
    draw_button_wood(216,201,103,36,"TREE >",false);
}
static void draw_decor_page(void){
    char b[44];
    for(int i=0;i<6;i++){
        int idx=i+2,x=(i%3)*106+2,y=70+(i/3)*62;
        bool owned=(P.owned&(1u<<idx))!=0;
        timber(x,y,104,59,owned);
        // Compact labels keep the text readable on a real 320px touch screen.
        text_c(x+52,y+12,1,GOLD,WOOD_SH,gene_shop_names[idx]);
        snprintf(b,sizeof b,owned?"EQUIP":"%u COINS",(unsigned)gene_shop_prices[idx]);
        text_c(x+52,y+39,1,CREAM,WOOD_SH,b);
    }
    draw_button_wood(3,201,314,36,"CUSTOMIZE YOUR POND - SHOP MORE",false);
}
static void draw_hop_page(void){
    char b[40];
    timber(3,4,314,49,false);
    snprintf(b,sizeof b,"LILY HOP  SCORE %d  TIME %d",hopScore,hopLeft/60);
    text_c(160,20,2,GOLD,WOOD_SH,b);
    for(int i=0;i<4;i++){
        float x=9+i*80;
        spr(PAD_X,MISC_Y,PAD_W,PAD_H,x,99,1.7f);
        if(hopSelected==i)obj(OBJ_HEART,x+24,76,2,1);
        if(hopTarget==i){obj(OBJ_SPARKLE,x+25,110,2,1);text_c(x+38,163,1,GOLD,WOOD_SH,"HOP!");}
    }
    text_c(160,208,1,CREAM,WOOD_SH,"TAP THE GLOWING PAD OR D-PAD + A");
}
static void draw_bottom(void){
    draw_region(&aBg,0,256,160,120,0,0,2,0,1.0f);
    if(scr==SCR_GAME){
        for(int i=0;i<MAX_FLIES;i++)if(flies[i].alive)obj(OBJ_FLY0+((frame/3+i)%2),flies[i].x,flies[i].y,2,1.0f);
        if(tongueT>0){C2D_DrawLine(160,240,RGB(255,111,138),tongueX,tongueY,RGB(255,111,138),6,0);}
        timber(5,3,310,35,false);
        char b[50];snprintf(b,sizeof b,"FLY CATCHER  SCORE %d  %d SEC",gameScore,gameLeft/60);
        text_c(160,15,1,GOLD,WOOD_SH,b);return;
    }
    if(scr==SCR_HOP){draw_hop_page();return;}
    if(scr==SCR_INFO){
        timber(4,8,312,224,false);
        text_c(160,19,2,GOLD,WOOD_SH,"GERALD PROFILE");
        draw_portrait(15,46);
        char b[70];
        snprintf(b,sizeof b,"GEN %u  LV %u",(unsigned)P.generation,(unsigned)P.level);text_s(113,60,2,CREAM,WOOD_SH,b);
        snprintf(b,sizeof b,"%s",g_stage_name(G.stage));text_s(113,86,2,GOLD,WOOD_SH,b);
        snprintf(b,sizeof b,"MEALS %u",(unsigned)G.meals);text_s(113,119,1,CREAM,WOOD_SH,b);
        snprintf(b,sizeof b,"BEST FLY %u",(unsigned)G.best_score);text_s(113,136,1,CREAM,WOOD_SH,b);
        snprintf(b,sizeof b,"BEST HOP %u",(unsigned)P.best_hop);text_s(113,153,1,CREAM,WOOD_SH,b);
        text_s(18,187,1,GOLD,WOOD_SH,"B: BACK  SELECT: MUSIC  START: SAVE+EXIT");return;
    }
    if(G.sleeping)rect(0,0,320,240,RGBA(8,10,40,90));
    if(scr==SCR_FEED){
        timber(3,29,314,126,false);
        text_c(160,52,2,GOLD,WOOD_SH,"WHAT'S FOR DINNER?");
        text_c(160,105,1,CREAM,WOOD_SH,"FROGS LOVE A TASTY BUG!");
        draw_buttons(feedBtns,3);return;
    }
    nav_tabs();
    switch(page){
    case TAB_CARE: draw_care_page();break;
    case TAB_EXPLORE:draw_explore_page();break;
    case TAB_SHOP:draw_shop_page();break;
    case TAB_FAMILY:draw_family_page();break;
    case TAB_DECOR:draw_decor_page();break;
    default:break;
    }
    if(msgT>0&&page==TAB_CARE){
        timber(52,156,218,37,false);
        text_lines(59,160,1,GOLD,WOOD_SH,msg);
    }
}

// ===========================================================================
// title
// ===========================================================================
static void draw_title_top(void) {
    draw_region(&aBg, 0, 0, 200, 120, 0, 0, 2, 0, 1.0f);
    spr(PAD_X, MISC_Y, PAD_W, PAD_H, 104, 170, 4);
    float y = 84 - fabsf(sinf(frame * 0.08f)) * 14;
    frog(G_FROG, EX_HAPPY, 136, y, 4, 1.0f);
    text_c(200, 22, 8, RGB(255, 230, 109), RGB(42, 90, 42), "GERALD");
    text_c(200, 218, 2, WHITE_C, RGB(38, 32, 58), "VIRTUAL FROG PET");
}

static void draw_title_bottom(void) {
    draw_region(&aBg, 0, 256, 160, 120, 0, 0, 2, 0, 1.0f);
    chain(60, -2, 66);
    chain(254, -2, 66);
    UI(PANEL_TITLE, 20, 60);
    if ((frame / 30) % 2 == 0) text_c(160, 84, 2, GOLD, WOOD_SH, "TAP OR PRESS A");
    text_c(160, 114, 1, CREAM, WOOD_SH, "HATCH - EVOLVE - SHOP - EXPLORE");
    text_c(160, 128, 1, CREAM, WOOD_SH, "RAISE A WHOLE FROG FAMILY!");
    text_c(160, 150, 1, RGB(222, 196, 150), WOOD_SH, "PIXEL POND + REAL FROG GENETICS");
    text_c(160, 162, 1, RGB(222, 196, 150), WOOD_SH, "L / R: TABS   X: PROFILE");
}

// ===========================================================================
// main
// ===========================================================================
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    romfsInit();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    sheetSpr = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    sheetBg = C2D_SpriteSheetLoad("romfs:/gfx/bg.t3x");
    sheetUi = C2D_SpriteSheetLoad("romfs:/gfx/ui.t3x");
    if (!sheetSpr || !sheetBg || !sheetUi) {
        // can't do much without graphics -- show an error on the console and bail
        C2D_Fini();
        C3D_Fini();
        consoleInit(GFX_TOP, NULL);
        printf("Gerald: could not load romfs graphics.\nPress START to exit.\n");
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
        romfsExit(); gfxExit();
        return 1;
    }
    aSpr = (Atlas){C2D_SpriteSheetGetImage(sheetSpr, 0).tex, SPR_W, SPR_H};
    aBg = (Atlas){C2D_SpriteSheetGetImage(sheetBg, 0).tex, BG_W, BG_H};
    aUi = (Atlas){C2D_SpriteSheetGetImage(sheetUi, 0).tex, UI_W, UI_H};
    C3D_TexSetFilter(aSpr.tex, GPU_NEAREST, GPU_NEAREST);   // crisp pixels
    C3D_TexSetFilter(aUi.tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetFilter(aBg.tex, GPU_NEAREST, GPU_NEAREST);

    audio_init();
    srand((unsigned)time(NULL));

    int64_t now = (int64_t)time(NULL);
    bool hasPet=g_load(&G,SAVE_PATH);
    bool hasGenes=gene_load(&P,GENE_PATH);
    if(!hasPet)g_new(&G,now);
    if(!hasGenes)gene_new(&P,(uint32_t)now);
    if(!hasPet&&!hasGenes){
        if(gene_import_previous(&P,&G,SAVE_DIR "/save.bin",now)){
            set_msg("OLD FROG FAMILY\nIMPORTED SUCCESSFULLY!");
            save_game();
        }
    }
    uint32_t ev = g_catchup(&G, now);
    lastSleeping = G.sleeping;
    scr = SCR_TITLE;
    handle_events(ev);
    bgm_update(G.sleeping);

    touchPosition tp = {0, 0};
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown(), kHeld = hidKeysHeld();
        if (kHeld & KEY_TOUCH) hidTouchRead(&tp);
        if (kDown & KEY_START) break;

        tick();
        update(kDown, kHeld, tp);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, RGB(0, 0, 0));
        C2D_SceneBegin(top);
        if (scr == SCR_TITLE) draw_title_top(); else draw_top();
        C2D_TargetClear(bot, RGB(0, 0, 0));
        C2D_SceneBegin(bot);
        if (scr == SCR_TITLE) draw_title_bottom(); else draw_bottom();
        C3D_FrameEnd(0);
    }

    G.last_time = (int64_t)time(NULL);
    save_game();

    if (audioOk) {
        ndspChnReset(0);
        for (int i = 0; i < SND_COUNT; i++) if (snd[i].data) linearFree(snd[i].data);
        ndspExit();
    }
    C2D_SpriteSheetFree(sheetSpr);
    C2D_SpriteSheetFree(sheetBg);
    C2D_SpriteSheetFree(sheetUi);
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}
