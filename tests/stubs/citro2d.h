#ifndef FAKE_CITRO2D_H
#define FAKE_CITRO2D_H
#include "3ds.h"
typedef struct C3D_RenderTarget C3D_RenderTarget;
typedef void* C2D_TextBuf;
typedef struct C2D_Text {int a;} C2D_Text;
#define C2D_WithColor 1
#define C2D_DEFAULT_MAX_OBJECTS 4096
u32 C2D_Color32(u8,u8,u8,u8);
void C2D_DrawRectSolid(float,float,float,float,float,u32);
void C2D_DrawCircleSolid(float,float,float,float,u32);
C2D_TextBuf C2D_TextBufNew(size_t);void C2D_TextBufClear(C2D_TextBuf);void C2D_TextBufDelete(C2D_TextBuf);
void C2D_TextParse(C2D_Text*,C2D_TextBuf,const char*);void C2D_TextOptimize(C2D_Text*);
void C2D_DrawText(C2D_Text*,int,float,float,float,float,float,u32);
void C2D_TextGetDimensions(C2D_Text*,float,float,float*,float*);
bool C3D_Init(size_t);void C3D_Fini(void);bool C2D_Init(size_t);void C2D_Fini(void);void C2D_Prepare(void);
C3D_RenderTarget*C2D_CreateScreenTarget(int,int);void C3D_FrameBegin(int);void C3D_FrameEnd(int);void C2D_TargetClear(C3D_RenderTarget*,u32);void C2D_SceneBegin(C3D_RenderTarget*);
typedef struct {int width,height; float left,right,top,bottom;} Tex3DS_SubTexture;
typedef struct C3D_Tex {int x;} C3D_Tex;
typedef struct {C3D_Tex *tex;Tex3DS_SubTexture *subtex;} C2D_Image;
typedef struct {int x;} C2D_ImageTint;
typedef void* C2D_SpriteSheet;
#define GPU_NEAREST 0
C2D_SpriteSheet C2D_SpriteSheetLoad(const char *);
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet,int);
void C2D_SpriteSheetFree(C2D_SpriteSheet);
void C2D_AlphaImageTint(C2D_ImageTint*,float);
void C2D_PlainImageTint(C2D_ImageTint*,u32,float);
void C2D_DrawImageAt(C2D_Image,float,float,float,C2D_ImageTint*,float,float);
void C2D_DrawLine(float,float,u32,float,float,u32,float,float);
void C3D_TexSetFilter(C3D_Tex*,int,int);
#endif
