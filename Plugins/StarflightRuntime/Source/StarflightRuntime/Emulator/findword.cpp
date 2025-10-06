#include"findword.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include <string>
#include <algorithm>
#include <unordered_map>

#include"cpu/cpu.h"
#include"callstack.h"

// Stub constants - will need proper values from game data
static const int FILESTAR0SIZE = 0x4000;

// Stub overlay structure
struct Overlay {
    const char* name;
    int id;
};
static Overlay overlays[] = { {"STARFLT", 0} };

// Stub implementations - all return placeholder values
const char* GetOverlayName(int word, int ovidx)
{
    if (word < (FILESTAR0SIZE+0x100)) return "STARFLT";
    return (ovidx==-1)?"STARFLT":overlays[0].name;
}

const char* GetOverlayName(int ovidx)
{
    return "STARFLT";
}

int GetOverlayIndex(int address, const char** overlayName)
{
    if (overlayName) *overlayName = "STARFLT";
    return 0;
}

int FindClosestWord(int si, int ovidx)
{
    return si; // Stub: just return the input
}

const char* FindWord(int word, int ovidx)
{
    return "UNKNOWN"; // Stub
}

const char* FindWordCanFail(int word, int& ovidx, int canFail)
{
    ovidx = 0;
    return "UNKNOWN"; // Stub
}

int FindWordByName(char* s, int n)
{
    return 0; // Stub
}

const char *FindDirectoryName(int idx)
{
    return "UNKNOWN"; // Stub
}

const SF_WORD* GetWord(int word, int ovidx)
{
    static SF_WORD stub = {0, "UNKNOWN"};
    return &stub; // Stub
}