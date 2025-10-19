#pragma once

#include "CoreMinimal.h"

/**
 * Static helper class for routing Unreal Engine input to the Starflight emulator
 */
class STARFLIGHTRUNTIME_API FStarflightInput
{
public:
    // Convert Unreal key to DOS BIOS scan code and push to emulator
    static void PushKey(const FKey& Key, bool bShift = false, bool bCtrl = false, bool bAlt = false);
    
    // Push raw DOS scan code directly (for special cases)
    static void PushRawScanCode(uint16_t ScanCode);
    
    // Check if emulator has pending keys
    static bool HasKey();
    
private:
    // Convert Unreal FKey to DOS BIOS scan code
    static uint16_t ConvertToDOSScanCode(const FKey& Key, bool bShift, bool bCtrl, bool bAlt);
};

