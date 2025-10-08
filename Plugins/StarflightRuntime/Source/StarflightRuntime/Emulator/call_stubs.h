#pragma once

#include <mutex>
#include <chrono>
#include <cmath>

// Stubs for missing dependencies in call.cpp

// Diligent graphics engine stubs
namespace Diligent {
    struct float3 {
        float x, y, z;
        float3() : x(0), y(0), z(0) {}
        float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        float3 operator/(float s) const { return float3(x/s, y/s, z/s); }
    };
}

// ZSTD compression stubs
inline size_t ZSTD_compressBound(size_t size) { return size + 100; }
inline size_t ZSTD_compress(void* dst, size_t dstSize, const void* src, size_t srcSize, int level) { return 0; }
inline unsigned ZSTD_isError(size_t code) { return 1; }
inline const char* ZSTD_getErrorName(size_t code) { return "ZSTD disabled"; }
inline size_t ZSTD_decompress(void* dst, size_t dstSize, const void* src, size_t srcSize) { return 0; }

// XXHash stubs
inline uint64_t XXH64(const void* input, size_t length, uint64_t seed) { return 0; }

// Graphics types from original graphics.h
enum PixelContents {
    ClearPixel = 0,
    NavigationalPixel,
    TextPixel,
    LinePixel,
    EllipsePixel,
    BoxFillPixel,
    PolyFillPixel,
    PicPixel,
    PlotPixel,
    TilePixel,
    RunBitPixel,
    AuxSysPixel,
    StarMapPixel,
    SpaceManPixel,
};

// Tagged data structure to replace std::vector with .tag member
struct TaggedData {
    std::vector<uint8_t> data;
    uint32_t tag = 0;
    uint32_t picID = 0;
    
    // Allow implicit conversion to vector for compatibility
    operator std::vector<uint8_t>&() { return data; }
    operator const std::vector<uint8_t>&() const { return data; }
};

struct Rotoscope {
    PixelContents content;
    uint8_t EGAcolor;
    uint32_t argb;
    int16_t blt_x, blt_y, blt_w, blt_h;
    uint8_t bgColor, fgColor;
    TaggedData runBitData;
    TaggedData picData;
    
    Rotoscope() : content(ClearPixel), EGAcolor(0), argb(0), blt_x(0), blt_y(0), blt_w(0), blt_h(0), bgColor(0), fgColor(0) {}
    Rotoscope(PixelContents pc) : content(pc), EGAcolor(0), argb(0), blt_x(0), blt_y(0), blt_w(0), blt_h(0), bgColor(0), fgColor(0) {}
};

struct FrameSync {
    bool inDrawAuxSys = false;
    bool inDrawStarMap = false;
    bool maneuvering = false;
    uint32_t gameContext = 0;
    bool shouldSave = false;
    bool inCombatKey = false;
    bool inCombatRender = false;
    bool inDrawShipButton = false;
    bool inSmallLogo = false;
    float currentPlanetMass = 0.0f;
    float currentPlanetSphereSize = 0.0f;
    bool pastHimus = false;
    bool inGameOps = false;
    bool inNebula = false;
    uint32_t currentPlanet = 0;
    uint32_t completedFrames = 0;
    std::chrono::steady_clock::time_point maneuveringStartTime;
    std::chrono::steady_clock::time_point maneuveringEndTime;
    std::chrono::milliseconds gameTickTimer;
    std::mutex mutex;
};

extern FrameSync frameSync;

// Archive structures
struct SectionHeader {
    uint64_t offset;
    uint64_t compressedSize;
    uint64_t uncompressedSize;
};

struct ArchiveHeader {
    char fourCC[4];
    uint32_t version;
    SectionHeader staraHeader;
    SectionHeader starbHeader;
    SectionHeader rotoscopeHeader;
    SectionHeader screenshotHeader;
};

// Missing types
template<typename T>
struct vec2 {
    T x, y;
    vec2() : x(0), y(0) {}
    vec2(T _x, T _y) : x(_x), y(_y) {}
    vec2 operator-(const vec2& other) const { return vec2(x - other.x, y - other.y); }
};

template<typename T>
struct vec3 {
    T x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}
    vec3 normalize() const {
        T len = std::sqrt(x*x + y*y + z*z);
        if (len > 0) return vec3(x/len, y/len, z/len);
        return *this;
    }
};

enum class IconType {
    Ship,
    Planet,
    Star,
    Sun,
    Nebula,
    Flux,
    Element,
    TVehicle,
    TerrainVehicle,
    Creature,
    Artifact,
    Ruin,
    Vessel,
    Takeoff,
    Holding,
    Other
};

// Allow comparison of uint32_t with IconType
inline bool operator==(uint32_t lhs, IconType rhs) { return lhs == static_cast<uint32_t>(rhs); }
inline bool operator==(IconType lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) == rhs; }

struct Icon {
    float x, y;
    uint32_t id;
    uint32_t clr;
    uint32_t icon_type = (uint32_t)IconType::Other;
    uint32_t inst_type = 0;
    uint32_t seed = 0;
    float planet_to_sunX = 0.0f;
    float planet_to_sunY = 0.0f;
    uint32_t iaddr = 0;
    int16_t locationX = 0;
    int16_t locationY = 0;
    uint16_t quantity = 0;
    uint16_t elementType = 0;
    uint16_t species = 0;
    int16_t screenX = 0;
    int16_t screenY = 0;
    int16_t bltX = 0;
    int16_t bltY = 0;
    float vesselHeading = 0.0f;
    float vesselSpeed = 0.0f;
    uint16_t vesselArmorHits = 0;
    uint16_t vesselShieldHits = 0;
};

struct Explosion {
    vec2<float> worldLocation;
    bool targetsPlayer;
    Explosion(vec2<float> loc, bool target) : worldLocation(loc), targetsPlayer(target) {}
};

struct MissileRecord {
    int16_t currx, curry, destx, desty;
    uint8_t morig, mclass;
    int16_t deltax, deltay;
};

struct MissileRecordUnique {
    MissileRecord mr;
    uint64_t nonce;
};

struct LaserRecord {
    int16_t x0, y0, x1, y1;
    uint16_t color;
    uint64_t hash;
    uint64_t computeHash() const { return hash; }
};

enum class StarMapLocale {
    Hyperspace,
    SolarSystem,
    Orbit
};

struct StarMapSetup {
    std::vector<Icon> starmap;
    vec2<int16_t> offset;
    vec2<int16_t> window;
};

enum class OrbitState {
    None,
    Insertion,
    Landing,
    Takeoff,
    Holding,
    Orbit
};

// Include instance.h for INSTANCEENTRY
#include "instance.h"

// Missing constants
#define REGDI 0x78C
#define FILESTAR0SIZE 54183
#define FILESTAR0 "starflt1-in/STARFLT.COM"
#define FILESTARA "starflt1-in/STARA.COM"
#define FILESTARB "starflt1-in/STARB.COM"

// Missing enums
enum class Vessel { Player, Alien };
enum class TerrainVehicle { ATV };
enum class Takeoff { Normal };
enum class Holding { Orbit };

// Missing constants
static const uint8_t CGAToEGA[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

// Missing functions
inline uint16_t Peek16(int offset) { return 0; }
inline void GraphicsSaveScreen() {}
inline void GraphicsSetDeadReckoning(int16_t x, int16_t y, const std::vector<Icon>&, const std::vector<Icon>&, uint16_t, const StarMapSetup&, const std::vector<MissileRecordUnique>&, const std::vector<LaserRecord>&, const std::vector<Explosion>&) {}
inline void GraphicsSetOrbitState(OrbitState state, vec3<float> sunPos = vec3<float>()) {}
inline void GraphicsInitPlanets(const std::vector<std::vector<uint8_t>>&) {}
inline void GraphicsInitPlanets(const std::unordered_map<uint32_t, struct PlanetSurface>&) {}
inline void GraphicsDeleteMissile(uint64_t id, const MissileRecord& mr) {}
inline void GraphicsReportGameFrame() {}
inline void GraphicsSplash(uint32_t ds, int fileNum) {}
inline void GraphicsMoveSpaceMan(int, int) {}
inline uint8_t GraphicsPeekDirect(int x, int y, uint32_t offset, Rotoscope* rs = nullptr) { return 0; }
inline void GraphicsPixelDirect(int x, int y, int color, uint32_t offset, Rotoscope rs = Rotoscope()) {}
