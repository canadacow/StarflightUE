// Disable warnings for emulator code
#pragma warning(disable: 4883) // function size suppresses optimizations

#include "graphics.h"
#include "../Public/StarflightBridge.h"
#include "cpu/cpu.h"
#include "font_cp437.h"
#include "tables.h"
#include <cassert>

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

// Debug: color mapping per PixelContents for rotoscope visualization (BGRA)
static inline uint32_t RotoDebugBGRA(uint8_t content)
{
    switch (content)
    {
        case ClearPixel:        return 0x00000000u; // transparent/black
        case NavigationalPixel: return 0x00FFFFFFu; // white-ish (BGRA: FF FF FF 00)
        case TextPixel:         return 0x00FFFFFFu; // white
        case LinePixel:         return 0x000000FFu; // red
        case EllipsePixel:      return 0x00FF00FFu; // magenta
        case BoxFillPixel:      return 0x0000FF00u; // green
        case PolyFillPixel:     return 0x0000FFFFu; // yellow
        case PicPixel:          return 0x00FF0000u; // blue
        case PlotPixel:         return 0x0000FFFFu; // yellow
        case TilePixel:         return 0x00800080u; // purple-ish
        case RunBitPixel:       return 0x0080FFFFu; // orange-ish
        case AuxSysPixel:       return 0x0080FF80u; // pink-ish/green-ish
        case StarMapPixel:      return 0x00FF80FFu; // soft pink
        case SpaceManPixel:     return 0x00808040u; // brown-ish
        default:                return 0x00808080u; // gray
    }
}

// Build and emit the 160x200 rotoscope debug buffer (and metadata) once per GraphicsUpdate
static void EmitRotoscopeBuffers()
{
    static std::vector<uint8_t> s_rotoDebug;
	static std::vector<FStarflightRotoTexel> s_rotoMeta;
    s_rotoDebug.resize(GRAPHICS_MODE_WIDTH * GRAPHICS_MODE_HEIGHT * 4);
	s_rotoMeta.resize(GRAPHICS_MODE_WIDTH * GRAPHICS_MODE_HEIGHT);
    std::lock_guard<std::mutex> rg(rotoscopePixelMutex);
    for (int y = 0; y < GRAPHICS_MODE_HEIGHT; ++y)
    {
        for (int x = 0; x < GRAPHICS_MODE_WIDTH; ++x)
        {
            const Rotoscope& rs = rotoscopePixels[y * GRAPHICS_MODE_WIDTH + x];
            const uint32_t bgra = RotoDebugBGRA(static_cast<uint8_t>(rs.content));
            const int o = (y * GRAPHICS_MODE_WIDTH + x) * 4;
            s_rotoDebug[o + 0] = (uint8_t)((bgra >> 0) & 0xFF);
            s_rotoDebug[o + 1] = (uint8_t)((bgra >> 8) & 0xFF);
            s_rotoDebug[o + 2] = (uint8_t)((bgra >> 16) & 0xFF);
            s_rotoDebug[o + 3] = (uint8_t)((bgra >> 24) & 0xFF);

			FStarflightRotoTexel meta{};
			meta.Content = static_cast<uint8_t>(rs.content);
			meta.FontNumber = rs.textData.fontNum;
			meta.Character = static_cast<uint8_t>(rs.textData.character);
			meta.Flags = rs.textData.xormode;
			meta.GlyphX = rs.blt_x;
			meta.GlyphY = rs.blt_y;
			meta.GlyphWidth = rs.blt_w;
			meta.GlyphHeight = rs.blt_h;
			meta.FGColor = rs.fgColor;
			meta.BGColor = rs.bgColor;
			s_rotoMeta[y * GRAPHICS_MODE_WIDTH + x] = meta;
        }
    }
    EmitRotoscope(s_rotoDebug.data(), GRAPHICS_MODE_WIDTH, GRAPHICS_MODE_HEIGHT, GRAPHICS_MODE_WIDTH * 4);
	EmitRotoscopeMeta(s_rotoMeta.data(), GRAPHICS_MODE_WIDTH, GRAPHICS_MODE_HEIGHT);
}

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

    // Emit rotoscope-derived buffers once per frame
    EmitRotoscopeBuffers();
}

void GraphicsMode(int mode)
{
    s_graphicsMode.store(mode);
}

void GraphicsClear(int color, uint32_t offset, int byteCount)
{
    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);
    uint32_t dest = (uint32_t)offset;

    dest <<= 4; // Convert to linear addres
    dest -= 0xa0000; // Subtract from EGA page
    dest *= 4; // Convert to our SDL memory linear address

    uint32_t destOffset = 0;

    auto c = colortable[color&0xF];

    byteCount = 0x2000;

    for(uint32_t i = 0; i < (uint32_t)byteCount * 4; ++i)
    {
        graphicsPixels[dest + destOffset + i] = c;
        rotoscopePixels[dest + destOffset + i] = ClearPixel;
    }
}

void GraphicsPixelDirect(int x, int y, uint32_t color, uint32_t offset, Rotoscope pc)
{
    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);

    if (offset == 0)
    {
        offset = 0xA000;
    }

    uint32_t base = (uint32_t)offset;
    base <<= 4;          // segment->linear
    base -= 0xA0000;     // subtract EGA base
    base *= 4;           // 4-byte pixels

    int yy = 199 - y;
    if (x < 0 || x >= GRAPHICS_MODE_WIDTH || yy < 0 || yy >= GRAPHICS_MODE_HEIGHT)
    {
        return;
    }

    pc.argb = color;
    const uint32_t idx = yy * GRAPHICS_MODE_WIDTH + x + base;
    rotoscopePixels[idx] = pc;
    graphicsPixels[idx] = color;
}

void GraphicsPixel(int x, int y, int color, uint32_t offset, Rotoscope pc)
{
    pc.EGAcolor = color & 0xF;
    GraphicsPixelDirect(x, y, colortable[color & 0xF], offset, pc);
}

uint32_t GraphicsPeekDirect(int x, int y, uint32_t offset, Rotoscope* pc)
{
    if(offset == 0)
    {
        offset = 0xa000;
    }

    offset <<= 4; // Convert to linear addres
    offset -= 0xa0000; // Subtract from EGA page
    offset *= 4; // Convert to our SDL memory linear address

    y = 199 - y;

    if(x < 0 || x >= GRAPHICS_MODE_WIDTH || y < 0 || y >= GRAPHICS_MODE_HEIGHT)
    {
        return colortable[0];
    }

    if(pc)
    {
        *pc = rotoscopePixels[y * GRAPHICS_MODE_WIDTH + x + offset];
    }

    return graphicsPixels[y * GRAPHICS_MODE_WIDTH + x + offset];
}

uint8_t GraphicsPeek(int x, int y, uint32_t offset, Rotoscope* pc)
{
    const uint32_t pixel = GraphicsPeekDirect(x, y, offset, pc);
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

void GraphicsBLT(int16_t x1, int16_t y1, int16_t h, int16_t w, const char* image, int color, int xormode, uint32_t offset, Rotoscope pc)
{
    auto img = (const short int*)image;
    int n = 0;

    uint16_t xoffset = 0;
    uint16_t yoffset = 0;

    pc.blt_w = w;
    pc.blt_h = h;

    for(int y=y1; y>y1-h; y--)
    {
        xoffset = 0;

        for(int x=x1; x<x1+w; x++)
        {
            int x0 = x;
            int y0 = y;

            Rotoscope srcPc{};
            bool hasPixel = false;
            auto src = GraphicsPeek(x0, y0, offset, &srcPc);

            pc.blt_x = xoffset;
            pc.blt_y = yoffset;

            if(pc.content == TextPixel)
            {
                pc.bgColor = src;
            }

            if ((*img) & (1<<(15-n)))
            {
                if(xormode) {
                    auto xored = src ^ (color&0xF);

                    if(srcPc.content == TextPixel)
                    {
                        srcPc.bgColor = srcPc.bgColor ^ (color & 0xf);
                        srcPc.fgColor = srcPc.fgColor ^ (color & 0xf);
                        GraphicsPixel(x0, y0, xored, offset, srcPc);
                    }
                    else
                    {
                        GraphicsPixel(x0, y0, xored, offset, pc);
                    }
                }
                else
                {
                    GraphicsPixel(x0, y0, color, offset, pc);
                }
            }
            else
            {
                GraphicsPixel(x0, y0, src, offset, pc);
            }
            
            n++;
            if (n == 16)
            {
                n = 0;
                img++;
            }

            ++xoffset;
        }

        ++yoffset;
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
    char c = (char)character;

    Rotoscope rs{};

    rs.content = TextPixel;
    rs.textData.character = c;
    rs.textData.fontNum = num;
    rs.fgColor = color;
    rs.textData.xormode = xormode;

    switch(num)
    {
        case 1:
        {
            auto width = 3;
            auto height = 5;
            auto image = font1_table[c];

            GraphicsBLT(x1, y1, height, width, (const char*)&image, color, xormode, offset, rs);

            return width;
        }
        case 2:
        {
            auto width = char_width_table[c];
            auto height = 7;
            auto image = font2_table[c].data();

            GraphicsBLT(x1, y1, height, width, (const char*)image, color, xormode, offset, rs);

            return width;
        }
        case 3:
        {
            auto width = char_width_table[c];
            auto height = 9;
            auto image = font3_table[c].data();

            GraphicsBLT(x1, y1, height, width, (const char*)image, color, xormode, offset, rs);

            return width;            
        }
        default:
            assert(false);
            break;
    }

    assert(false);
    return 1;
}

void GraphicsCopyLine(uint16_t sourceSeg, uint16_t destSeg, uint16_t si, uint16_t di, uint16_t count)
{
    std::lock_guard<std::mutex> lg(rotoscopePixelMutex);

    uint32_t src = (uint32_t)sourceSeg;
    uint32_t dest = (uint32_t)destSeg;

    src <<= 4; // Convert to linear addres
    src -= 0xa0000; // Subtract from EGA page
    src *= 4; // Convert to our SDL memory linear address

    dest <<= 4; // Convert to linear addres
    dest -= 0xa0000; // Subtract from EGA page
    dest *= 4; // Convert to our SDL memory linear address

    uint32_t srcOffset = (uint32_t)si * 4;
    uint32_t destOffset = (uint32_t)di * 4;

    for(uint32_t i = 0; i < (uint32_t)count * 4; ++i)
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

void GraphicsMoveSpaceMan(uint16_t x, uint16_t y)
{
	EmitSpaceManMove(x, y);
}
