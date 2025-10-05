#ifndef GRAPHICS_UE_H
#define GRAPHICS_UE_H

#include <stdint.h>
#include "call_stubs.h"

// EGA color palette (shared with call.cpp)
extern uint32_t colortable[16];

// Minimal graphics interface for UE integration
// Stubs for all the graphics functions that call.cpp expects

void GraphicsInit();
void GraphicsQuit();
void GraphicsUpdate();

void GraphicsMode(int mode); // 0 = text, 1 = ega graphics
void GraphicsClear(int color, uint32_t offset, int byteCount);
void GraphicsText(char *s, int n);
void GraphicsCarriageReturn();
void GraphicsSetCursor(int x, int y);
void GraphicsChar(unsigned char s);
void GraphicsLine(int x1, int y1, int x2, int y2, int color, int xormode, uint32_t offset);

void GraphicsPixel(int x, int y, int color, uint32_t offset, Rotoscope rs = Rotoscope());
void GraphicsBLT(int16_t x1, int16_t y1, int16_t w, int16_t h, const char* image, int color, int xormode, uint32_t offset, Rotoscope rs = Rotoscope());
void GraphicsSave(char *filename);

uint8_t GraphicsPeek(int x, int y, uint32_t offset, Rotoscope* rs = nullptr);
int16_t GraphicsFONT(uint16_t num, uint32_t character, int x1, int y1, int color, int xormode, uint32_t offset);

void GraphicsCopyLine(uint16_t sourceSeg, uint16_t destSeg, uint16_t si, uint16_t di, uint16_t count);

void BeepOn();
void BeepTone(uint16_t pitFreq);
void BeepOff();

bool GraphicsHasKey();
uint16_t GraphicsGetKey();
void GraphicsPushKey(uint16_t key);

void WaitForVBlank();

bool IsGraphicsShutdown();

// EGA color table
extern uint32_t colortable[16];
extern bool graphicsIsShutdown;

#endif
