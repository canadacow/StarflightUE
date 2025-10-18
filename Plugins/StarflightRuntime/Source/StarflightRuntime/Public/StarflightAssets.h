#pragma once

#include "CoreMinimal.h"

class STARFLIGHTRUNTIME_API FStarflightAssets
{
public:
    static FStarflightAssets& Get();
    
    void Initialize();
    void Shutdown();
    
    // Get raw grayscale pixel data from the PNGs
    TArray<uint8> GetMiniEarthData(int32& OutWidth, int32& OutHeight);
    TArray<uint8> GetLofiEarthData(int32& OutWidth, int32& OutHeight);
    
private:
    FStarflightAssets() = default;
    
    bool LoadPNGFile(const FString& FilePath, TArray<uint8>& OutData, int32& OutWidth, int32& OutHeight);
    
    // Cached PNG data
    TArray<uint8> MiniEarthData;
    int32 MiniEarthWidth = 0;
    int32 MiniEarthHeight = 0;
    
    TArray<uint8> LofiEarthData;
    int32 LofiEarthWidth = 0;
    int32 LofiEarthHeight = 0;
};

