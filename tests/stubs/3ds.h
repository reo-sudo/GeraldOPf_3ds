#ifndef FAKE_3DS_H
#define FAKE_3DS_H
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint32_t u32;typedef uint64_t u64;typedef uint8_t u8;typedef int16_t s16;typedef int Result;
#define R_FAILED(n) ((n)<0)
#define NDSP_OUTPUT_STEREO 1
#define NDSP_INTERP_LINEAR 1
#define NDSP_FORMAT_MONO_PCM16 1
#define NDSP_WBUF_DONE 2
#define NDSP_WBUF_FREE 0
#define GFX_TOP 0
#define GFX_BOTTOM 1
#define GFX_LEFT 0
#define KEY_START 1u
#define KEY_TOUCH 2u
#define KEY_A 4u
#define KEY_B 8u
#define KEY_X 16u
#define KEY_Y 32u
#define KEY_L 64u
#define KEY_R 128u
#define KEY_LEFT 256u
#define KEY_RIGHT 512u
#define KEY_UP 1024u
#define KEY_DOWN 2048u
#define C3D_DEFAULT_CMDBUF_SIZE 0x1000
#define C3D_FRAME_SYNCDRAW 1
struct ndspWaveBuf {void*data_vaddr;u32 nsamples;bool looping;int status;};
typedef struct ndspWaveBuf ndspWaveBuf;
typedef struct{int px,py;} touchPosition;
Result ndspInit(void);void ndspExit(void);void ndspSetOutputMode(int);
void ndspChnReset(int);void ndspChnSetInterp(int,int);void ndspChnSetRate(int,float);
void ndspChnSetFormat(int,int);void ndspChnSetMix(int,const float*);
void ndspChnWaveBufAdd(int,ndspWaveBuf*);
void*linearAlloc(size_t);void linearFree(void*);void DSP_FlushDataCache(const void*,u32);
void gfxInitDefault(void);void gfxExit(void);int romfsInit(void);void romfsExit(void);
u64 osGetTime(void);bool aptMainLoop(void);void hidScanInput(void);u32 hidKeysDown(void);void hidTouchRead(touchPosition*);
typedef struct {int a;} SwkbdState;
typedef enum {SWKBD_BUTTON_NONE=0,SWKBD_BUTTON_CONFIRM=2} SwkbdButton;
#define SWKBD_TYPE_NORMAL 0
void swkbdInit(SwkbdState*,int,int,int);
void swkbdSetHintText(SwkbdState*,const char*);
SwkbdButton swkbdInputText(SwkbdState*,char*,size_t);
#define KEY_DLEFT KEY_LEFT
#define KEY_DRIGHT KEY_RIGHT
#define SWKBD_TYPE_NUMPAD 1
void hidTouchRead(touchPosition *);
u32 hidKeysHeld(void);
void ndspSetMasterVol(float);
void ndspChnSetPaused(int,bool);
void gfxFlushBuffers(void);
void gfxSwapBuffers(void);
void gspWaitForVBlank(void);
void *consoleInit(int,void*);
typedef uint16_t u16;
#define KEY_SELECT 4096u
#endif
