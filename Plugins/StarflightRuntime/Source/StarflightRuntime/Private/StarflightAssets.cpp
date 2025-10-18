#include "StarflightAssets.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Logging/LogMacros.h"
#include "Interfaces/IPluginManager.h"

// lodepng for PNG decoding
#include "../Emulator/util/lodepng.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightAssets, Log, All);

FStarflightAssets& FStarflightAssets::Get()
{
    static FStarflightAssets Instance;
    return Instance;
}

void FStarflightAssets::Initialize()
{
    // Get plugin content directory
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarflightRuntime"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogStarflightAssets, Error, TEXT("Failed to find StarflightRuntime plugin"));
        return;
    }
    
    FString PluginContentDir = Plugin->GetContentDir();
    
    // Load mini_earth.png
    FString MiniEarthPath = FPaths::Combine(PluginContentDir, TEXT("mini_earth.png"));
    if (LoadPNGFile(MiniEarthPath, MiniEarthData, MiniEarthWidth, MiniEarthHeight))
    {
        UE_LOG(LogStarflightAssets, Log, TEXT("Loaded mini_earth.png: %dx%d (%d bytes)"), 
            MiniEarthWidth, MiniEarthHeight, MiniEarthData.Num());
    }
    else
    {
        UE_LOG(LogStarflightAssets, Error, TEXT("Failed to load mini_earth.png from: %s"), *MiniEarthPath);
    }
    
    // Load lofi_earth.png
    FString LofiEarthPath = FPaths::Combine(PluginContentDir, TEXT("lofi_earth.png"));
    if (LoadPNGFile(LofiEarthPath, LofiEarthData, LofiEarthWidth, LofiEarthHeight))
    {
        UE_LOG(LogStarflightAssets, Log, TEXT("Loaded lofi_earth.png: %dx%d (%d bytes)"), 
            LofiEarthWidth, LofiEarthHeight, LofiEarthData.Num());
    }
    else
    {
        UE_LOG(LogStarflightAssets, Error, TEXT("Failed to load lofi_earth.png from: %s"), *LofiEarthPath);
    }
}

bool FStarflightAssets::LoadPNGFile(const FString& FilePath, TArray<uint8>& OutData, int32& OutWidth, int32& OutHeight)
{
    // Read file into memory
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        return false;
    }
    
    // Decode PNG
    std::vector<unsigned char> ImageData;
    unsigned Width, Height;
    unsigned Error = lodepng::decode(ImageData, Width, Height, FileData.GetData(), FileData.Num(), LCT_GREY, 8);
    
    if (Error)
    {
        UE_LOG(LogStarflightAssets, Error, TEXT("lodepng decode error %u: %s"), Error, ANSI_TO_TCHAR(lodepng_error_text(Error)));
        return false;
    }
    
    // Copy to output
    OutWidth = Width;
    OutHeight = Height;
    OutData.SetNum(ImageData.size());
    FMemory::Memcpy(OutData.GetData(), ImageData.data(), ImageData.size());
    
    return true;
}

void FStarflightAssets::Shutdown()
{
    MiniEarthData.Empty();
    LofiEarthData.Empty();
}

TArray<uint8> FStarflightAssets::GetMiniEarthData(int32& OutWidth, int32& OutHeight)
{
    OutWidth = MiniEarthWidth;
    OutHeight = MiniEarthHeight;
    return MiniEarthData;
}

TArray<uint8> FStarflightAssets::GetLofiEarthData(int32& OutWidth, int32& OutHeight)
{
    OutWidth = LofiEarthWidth;
    OutHeight = LofiEarthHeight;
    return LofiEarthData;
}

