// Disable warnings for emulator code
#pragma warning(disable: 4883) // function size suppresses optimizations

#include "graphics.h"
#include "StarflightBridge.h"
#include "cpu/cpu.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>
#include <algorithm>

// EGA color palette (BGRA format for UE)
// Made non-static so call.cpp can access it
uint32_t colortable[16] = {
    0xFF000000, // 0: Black
    0xFF0000AA, // 1: Blue
    0xFF00AA00, // 2: Green
    0xFF00AAAA, // 3: Cyan
    0xFFAA0000, // 4: Red
    0xFFAA00AA, // 5: Magenta
    0xFFAA5500, // 6: Brown
    0xFFAAAAAA, // 7: Light Gray
    0xFF555555, // 8: Dark Gray
    0xFF5555FF, // 9: Light Blue
    0xFF55FF55, // 10: Light Green
    0xFF55FFFF, // 11: Light Cyan
    0xFFFF5555, // 12: Light Red
    0xFFFF55FF, // 13: Light Magenta
    0xFFFFFF55, // 14: Yellow
    0xFFFFFFFF  // 15: White
};

bool graphicsIsShutdown = false;

// Graphics state
static std::atomic<int> s_graphicsMode{0}; // 0 = text, 1 = graphics
static std::mutex s_framebufferMutex;
static std::vector<uint8_t> s_framebuffer;
static int s_cursorX = 0;
static int s_cursorY = 0;

// Dimensions
constexpr int TEXT_WIDTH = 80;
constexpr int TEXT_HEIGHT = 25;
constexpr int TEXT_CHAR_WIDTH = 8;
constexpr int TEXT_CHAR_HEIGHT = 8;
constexpr int GRAPHICS_WIDTH = 320;
constexpr int GRAPHICS_HEIGHT = 200;

// Video memory offsets
constexpr uint32_t TEXT_SEGMENT = 0xB800;
constexpr uint32_t GRAPHICS_SEGMENT = 0xA000;

// Simple 8x8 font (ASCII 32-127)
static const uint8_t s_font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // #
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // %
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // &
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    // ... (rest would be filled in, using simplified versions for now)
};

void GraphicsInit()
{
    s_framebuffer.resize(TEXT_WIDTH * TEXT_CHAR_WIDTH * TEXT_HEIGHT * TEXT_CHAR_HEIGHT * 4, 0);
    graphicsIsShutdown = false;
    s_graphicsMode.store(0); // Start in text mode (80x25), game will switch to graphics mode
    s_cursorX = 0;
    s_cursorY = 0;
    
    // Clear text memory (0xB800) to black background, light gray foreground
    uint32_t textMemBase = ComputeAddress(TEXT_SEGMENT, 0);
    for (int i = 0; i < TEXT_WIDTH * TEXT_HEIGHT; ++i) {
        m[textMemBase + i * 2] = 0x20; // Space character
        m[textMemBase + i * 2 + 1] = 0x07; // Light gray on black
    }
}

void GraphicsQuit()
{
    graphicsIsShutdown = true;
}

void GraphicsUpdate()
{
    if (graphicsIsShutdown) return;

    std::lock_guard<std::mutex> lock(s_framebufferMutex);

    int mode = s_graphicsMode.load();
    
    if (mode == 0) {
        // Text mode: 80x25 characters, read from segment 0xB800
        // Each character is 2 bytes: [char, attribute]
        int fbWidth = TEXT_WIDTH * TEXT_CHAR_WIDTH;
        int fbHeight = TEXT_HEIGHT * TEXT_CHAR_HEIGHT;
        
        if (s_framebuffer.size() != fbWidth * fbHeight * 4) {
            s_framebuffer.resize(fbWidth * fbHeight * 4);
        }

        uint32_t textMemBase = ComputeAddress(TEXT_SEGMENT, 0);
        
        for (int row = 0; row < TEXT_HEIGHT; ++row) {
            for (int col = 0; col < TEXT_WIDTH; ++col) {
                uint32_t offset = textMemBase + (row * TEXT_WIDTH + col) * 2;
                uint8_t ch = m[offset];
                uint8_t attr = m[offset + 1];
                
                uint8_t fgColor = attr & 0x0F;
                uint8_t bgColor = (attr >> 4) & 0x0F;
                
                // Render 8x8 character
                for (int cy = 0; cy < TEXT_CHAR_HEIGHT; ++cy) {
                    uint8_t fontRow = (ch >= 32 && ch < 128) ? s_font8x8[ch - 32][cy] : 0;
                    
                    for (int cx = 0; cx < TEXT_CHAR_WIDTH; ++cx) {
                        bool pixelOn = (fontRow & (1 << (7 - cx))) != 0;
                        uint32_t color = pixelOn ? colortable[fgColor] : colortable[bgColor];
                        
                        int px = col * TEXT_CHAR_WIDTH + cx;
                        int py = row * TEXT_CHAR_HEIGHT + cy;
                        int idx = (py * fbWidth + px) * 4;
                        
                        s_framebuffer[idx + 0] = (color >> 0) & 0xFF;  // B
                        s_framebuffer[idx + 1] = (color >> 8) & 0xFF;  // G
                        s_framebuffer[idx + 2] = (color >> 16) & 0xFF; // R
                        s_framebuffer[idx + 3] = 0xFF;                  // A
                    }
                }
            }
        }
        
        // Emit frame
        EmitFrame(s_framebuffer.data(), fbWidth, fbHeight, fbWidth * 4);
    }
    else {
        // Graphics mode: 320x200, 4 planes EGA
        // For now, just read plane 0 (simplified)
        uint32_t gfxMemBase = ComputeAddress(GRAPHICS_SEGMENT, 0);
        
        if (s_framebuffer.size() != GRAPHICS_WIDTH * GRAPHICS_HEIGHT * 4) {
            s_framebuffer.resize(GRAPHICS_WIDTH * GRAPHICS_HEIGHT * 4);
        }
        
        for (int y = 0; y < GRAPHICS_HEIGHT; ++y) {
            for (int x = 0; x < GRAPHICS_WIDTH; ++x) {
                // Simplified: read linear framebuffer (not actual EGA planar)
                uint32_t offset = gfxMemBase + y * GRAPHICS_WIDTH + x;
                uint8_t colorIdx = m[offset] & 0x0F;
                uint32_t color = colortable[colorIdx];
                
                int idx = (y * GRAPHICS_WIDTH + x) * 4;
                s_framebuffer[idx + 0] = (color >> 0) & 0xFF;  // B
                s_framebuffer[idx + 1] = (color >> 8) & 0xFF;  // G
                s_framebuffer[idx + 2] = (color >> 16) & 0xFF; // R
                s_framebuffer[idx + 3] = 0xFF;                  // A
            }
        }
        
        EmitFrame(s_framebuffer.data(), GRAPHICS_WIDTH, GRAPHICS_HEIGHT, GRAPHICS_WIDTH * 4);
    }
}

void GraphicsMode(int mode)
{
    s_graphicsMode.store(mode);
}

void GraphicsClear(int color, uint32_t offset, int byteCount)
{
    // Write to CPU memory at the given offset
    uint32_t addr = ComputeAddress(GRAPHICS_SEGMENT, offset);
    std::memset(&m[addr], color, byteCount);
}

void GraphicsPixel(int x, int y, int color, uint32_t offset, Rotoscope rs)
{
    if (x < 0 || x >= GRAPHICS_WIDTH || y < 0 || y >= GRAPHICS_HEIGHT) return;
    
    uint32_t addr = ComputeAddress(GRAPHICS_SEGMENT, offset + y * GRAPHICS_WIDTH + x);
    m[addr] = color & 0x0F;
}

uint8_t GraphicsPeek(int x, int y, uint32_t offset, Rotoscope* rs)
{
    if (x < 0 || x >= GRAPHICS_WIDTH || y < 0 || y >= GRAPHICS_HEIGHT) return 0;
    
    uint32_t addr = ComputeAddress(GRAPHICS_SEGMENT, offset + y * GRAPHICS_WIDTH + x);
    return m[addr];
}

void GraphicsLine(int x1, int y1, int x2, int y2, int color, int xormode, uint32_t offset)
{
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        if (xormode) {
            uint8_t existing = GraphicsPeek(x1, y1, offset);
            GraphicsPixel(x1, y1, existing ^ color, offset);
        } else {
            GraphicsPixel(x1, y1, color, offset);
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void GraphicsBLT(int16_t x1, int16_t y1, int16_t w, int16_t h, const char* image, int color, int xormode, uint32_t offset, Rotoscope rs)
{
    // Blit a monochrome bitmap
    for (int16_t y = 0; y < h; ++y) {
        for (int16_t x = 0; x < w; ++x) {
            int byteIdx = (y * ((w + 7) / 8)) + (x / 8);
            int bitIdx = 7 - (x % 8);
            bool pixelOn = (image[byteIdx] & (1 << bitIdx)) != 0;
            
            if (pixelOn) {
                if (xormode) {
                    uint8_t existing = GraphicsPeek(x1 + x, y1 + y, offset);
                    GraphicsPixel(x1 + x, y1 + y, existing ^ color, offset);
                } else {
                    GraphicsPixel(x1 + x, y1 + y, color, offset);
                }
            }
        }
    }
}

void GraphicsText(char *s, int n)
{
    uint32_t textMemBase = ComputeAddress(TEXT_SEGMENT, 0);
    
    for (int i = 0; i < n; ++i) {
        if (s[i] == '\n') {
            GraphicsCarriageReturn();
        } else {
            uint32_t offset = textMemBase + (s_cursorY * TEXT_WIDTH + s_cursorX) * 2;
            m[offset] = s[i];
            m[offset + 1] = 0x07; // White on black
            
            s_cursorX++;
            if (s_cursorX >= TEXT_WIDTH) {
                GraphicsCarriageReturn();
            }
        }
    }
}

void GraphicsChar(unsigned char s)
{
    char ch = s;
    GraphicsText(&ch, 1);
}

void GraphicsCarriageReturn()
{
    s_cursorX = 0;
    s_cursorY++;
    if (s_cursorY >= TEXT_HEIGHT) {
        s_cursorY = TEXT_HEIGHT - 1;
        // TODO: Scroll screen
    }
}

void GraphicsSetCursor(int x, int y)
{
    s_cursorX = x;
    s_cursorY = y;
}

int16_t GraphicsFONT(uint16_t num, uint32_t character, int x1, int y1, int color, int xormode, uint32_t offset)
{
    // Stub: render character at position
    // Return character width
    return TEXT_CHAR_WIDTH;
}

void GraphicsCopyLine(uint16_t sourceSeg, uint16_t destSeg, uint16_t si, uint16_t di, uint16_t count)
{
    uint32_t srcAddr = ComputeAddress(sourceSeg, si);
    uint32_t dstAddr = ComputeAddress(destSeg, di);
    std::memcpy(&m[dstAddr], &m[srcAddr], count);
}

void GraphicsSave(char *filename)
{
    // Stub: save screenshot
}

void BeepOn() {}
void BeepTone(uint16_t pitFreq) {}
void BeepOff() {}

// Keyboard stubs
static std::vector<uint16_t> s_keyQueue;
static std::mutex s_keyMutex;

bool GraphicsHasKey()
{
    std::lock_guard<std::mutex> lock(s_keyMutex);
    return !s_keyQueue.empty();
}

uint16_t GraphicsGetKey()
{
    std::lock_guard<std::mutex> lock(s_keyMutex);
    if (s_keyQueue.empty()) return 0;
    
    uint16_t key = s_keyQueue.front();
    s_keyQueue.erase(s_keyQueue.begin());
    return key;
}

void GraphicsPushKey(uint16_t key)
{
    std::lock_guard<std::mutex> lock(s_keyMutex);
    s_keyQueue.push_back(key);
}

void WaitForVBlank()
{
    // Stub: in real implementation, would sync to 60Hz
}

bool IsGraphicsShutdown()
{
    return graphicsIsShutdown;
}
