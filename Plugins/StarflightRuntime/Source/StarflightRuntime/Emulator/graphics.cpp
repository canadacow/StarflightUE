// Disable warnings for emulator code
#pragma warning(disable: 4883) // function size suppresses optimizations

#include "graphics.h"
#include "../Public/StarflightBridge.h"
#include "cpu/cpu.h"
#include "font_cp437.h"

#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>
#include <algorithm>

// EGA color palette in 0x00RRGGBB format (matches native)
// Made non-static so call.cpp can access it
uint32_t colortable[16] = {
    0x00000000, // 0: Black
    0x000000AA, // 1: Blue
    0x0000AA00, // 2: Green
    0x0000AAAA, // 3: Cyan
    0x00AA0000, // 4: Red
    0x00AA00AA, // 5: Magenta
    0x00AA5500, // 6: Brown
    0x00AAAAAA, // 7: Light Gray
    0x00555555, // 8: Dark Gray
    0x005555FF, // 9: Light Blue
    0x0055FF55, // 10: Light Green
    0x0055FFFF, // 11: Light Cyan
    0x00FF5555, // 12: Light Red
    0x00FF55FF, // 13: Light Magenta
    0x00FFFF55, // 14: Yellow
    0x00FFFFFF  // 15: White
};

bool graphicsIsShutdown = false;

// Global emulation control flag used by call.cpp
std::atomic<bool> stopEmulationThread{false};

// Graphics state
static std::atomic<int> s_graphicsMode{0}; // 0 = text, 1 = graphics
static std::mutex s_framebufferMutex;
static std::vector<uint8_t> s_framebuffer;
static std::vector<uint32_t> graphicsPixels; // 0x00RRGGBB like native
static std::vector<Rotoscope> rotoscopePixels;
static std::mutex rotoscopePixelMutex;
static int s_cursorX = 0;
static int s_cursorY = 0;

// Dimensions
constexpr int TEXT_WIDTH = 80;
constexpr int TEXT_HEIGHT = 25;
constexpr int TEXT_CHAR_WIDTH = 8;
constexpr int TEXT_CHAR_HEIGHT = 8;
constexpr int GRAPHICS_MODE_WIDTH = 160;   // Match native
constexpr int GRAPHICS_MODE_HEIGHT = 200;  // Match native
constexpr int GRAPHICS_PAGE_COUNT = 2;
constexpr int GRAPHICS_MEMORY_ALLOC = 65536; // Matches native backing store

// Video memory offsets
constexpr uint32_t TEXT_SEGMENT = 0xB800;
constexpr uint32_t GRAPHICS_SEGMENT = 0xA000;

void GraphicsInit()
{
    s_framebuffer.resize(TEXT_WIDTH * TEXT_CHAR_WIDTH * TEXT_HEIGHT * TEXT_CHAR_HEIGHT * 4, 0);
    graphicsIsShutdown = false;
    s_graphicsMode.store(0); // Start in text mode (80x25), game will switch to graphics mode
    s_cursorX = 0;
    s_cursorY = 0;
    graphicsPixels.assign(GRAPHICS_MEMORY_ALLOC, 0);
    rotoscopePixels.assign(GRAPHICS_MEMORY_ALLOC, Rotoscope{});
    
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
                
                // Render 8x8 character using CP437 font
                for (int cy = 0; cy < TEXT_CHAR_HEIGHT; ++cy) {
                    int fontOffset = ch * 8 + cy;
                    uint8_t fontRow = vgafont8[fontOffset];
                    
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
        // Graphics mode: render from backing store like native
        if (s_framebuffer.size() != GRAPHICS_MODE_WIDTH * GRAPHICS_MODE_HEIGHT * 4) {
            s_framebuffer.resize(GRAPHICS_MODE_WIDTH * GRAPHICS_MODE_HEIGHT * 4);
        }

        for (int y = 0; y < GRAPHICS_MODE_HEIGHT; ++y) {
            for (int x = 0; x < GRAPHICS_MODE_WIDTH; ++x) {
                // Display page base is 0xA000 -> offset index 0 in our arrays
                const uint32_t pixel = graphicsPixels[y * GRAPHICS_MODE_WIDTH + x]; // already y-flipped at write time
                const uint8_t r = (pixel >> 16) & 0xFF;
                const uint8_t g = (pixel >> 8) & 0xFF;
                const uint8_t b = (pixel >> 0) & 0xFF;

                const int idx = (y * GRAPHICS_MODE_WIDTH + x) * 4;
                s_framebuffer[idx + 0] = b;
                s_framebuffer[idx + 1] = g;
                s_framebuffer[idx + 2] = r;
                s_framebuffer[idx + 3] = 0xFF;
            }
        }

        EmitFrame(s_framebuffer.data(), GRAPHICS_MODE_WIDTH, GRAPHICS_MODE_HEIGHT, GRAPHICS_MODE_WIDTH * 4);
    }
}

void GraphicsMode(int mode)
{
    s_graphicsMode.store(mode);
}

void GraphicsClear(int color, uint32_t offset, int byteCount)
{
    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);

    uint32_t dest = (uint32_t)offset;
    dest <<= 4;               // segment: convert to linear
    dest -= 0xA0000;          // subtract EGA base
    dest *= 4;                // 4 bytes per pixel backing store

    auto c = colortable[color & 0xF];

    // Native clamps to a fixed 0x2000 range per call
    byteCount = 0x2000;

    for (uint32_t i = 0; i < (uint32_t)byteCount * 4; ++i)
    {
        graphicsPixels[dest + i] = c;
        rotoscopePixels[dest + i] = Rotoscope{}; // ClearPixel
    }
}

void GraphicsPixel(int x, int y, int color, uint32_t offset, Rotoscope pc)
{
    pc.EGAcolor = color & 0xF;
    // Convert EGA color index to 0x00RRGGBB and write via native direct path
    uint32_t argb = colortable[color & 0xF];

    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);

    if (offset == 0)
    {
        offset = 0xA000;
    }

    uint32_t base = (uint32_t)offset;
    base <<= 4;               // segment to linear
    base -= 0xA0000;          // subtract EGA base
    base *= 4;                // 4-byte pixels in backing store

    // Native Y flip
    int yy = 199 - y;
    if (x < 0 || x >= GRAPHICS_MODE_WIDTH || yy < 0 || yy >= GRAPHICS_MODE_HEIGHT) return;

    pc.argb = argb;
    rotoscopePixels[yy * GRAPHICS_MODE_WIDTH + x + base] = pc;
    graphicsPixels[yy * GRAPHICS_MODE_WIDTH + x + base] = argb;
}

uint8_t GraphicsPeek(int x, int y, uint32_t offset, Rotoscope* pc)
{
    if (offset == 0) offset = 0xA000;

    uint32_t base = (uint32_t)offset;
    base <<= 4;
    base -= 0xA0000;
    base *= 4;

    int yy = 199 - y;
    if (x < 0 || x >= GRAPHICS_MODE_WIDTH || yy < 0 || yy >= GRAPHICS_MODE_HEIGHT)
        return 0;

    const uint32_t pixel = graphicsPixels[yy * GRAPHICS_MODE_WIDTH + x + base];
    if (pc) { *pc = rotoscopePixels[yy * GRAPHICS_MODE_WIDTH + x + base]; }

    // Map back to EGA index
    for (int i = 0; i < 16; ++i) if (colortable[i] == pixel) return (uint8_t)i;
    return 0;
}

void GraphicsLine(int x1, int y1, int x2, int y2, int color, int xormode, uint32_t offset)
{
    float x = (float)x1;
    float y = (float)y1;
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    int n = (int)fabsf(dx);
    if ((int)fabsf(dy) > n) n = (int)fabsf(dy);
    if (n == 0) return;
    dx /= n;
    dy /= n;

    Rotoscope rs{};
    rs.content = LinePixel;
    rs.lineData.x0 = x1;
    rs.lineData.x1 = x2;
    rs.lineData.y0 = 199 - y1;
    rs.lineData.y1 = 199 - y2;
    rs.lineData.total = n;
    rs.fgColor = color;

    for (int i = 0; i <= n; ++i)
    {
        rs.lineData.n = i;
        rs.bgColor = GraphicsPeek((int)x, (int)y, offset);
        GraphicsPixel((int)x, (int)y, color, offset, rs);
        x += dx;
        y += dy;
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
    for(int i=0; i<n; i++)
    {
        GraphicsChar(s[i]);
    }
}

void GraphicsChar(unsigned char s)
{
    if (s_graphicsMode.load() != 0)
    {
        // Graphics mode - ignore for now
        return;
    }
    
    // Text mode - write character to text memory segment 0xB800
    // GraphicsUpdate() will render it from there
    uint32_t textMemBase = ComputeAddress(TEXT_SEGMENT, 0);
    uint32_t offset = textMemBase + (s_cursorY * TEXT_WIDTH + s_cursorX) * 2;
    
    m[offset] = s;          // Character
    m[offset + 1] = 0x07;   // Attribute (light gray on black)
    
    s_cursorX++;
    if (s_cursorX >= 80)
    {
        GraphicsCarriageReturn();
    }
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
    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);

    uint32_t src = (uint32_t)sourceSeg;
    uint32_t dest = (uint32_t)destSeg;

    src <<= 4;   // segment->linear
    src -= 0xA0000;
    src *= 4;

    dest <<= 4;  // segment->linear
    dest -= 0xA0000;
    dest *= 4;

    uint32_t srcOffset = (uint32_t)si * 4;
    uint32_t destOffset = (uint32_t)di * 4;

    for (uint32_t i = 0; i < (uint32_t)count * 4; ++i)
    {
        graphicsPixels[dest + destOffset + i] = graphicsPixels[src + srcOffset + i];
        rotoscopePixels[dest + destOffset + i] = rotoscopePixels[src + srcOffset + i];
    }
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
