#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"
#include "StarflightBridge.h"
#include "StarflightTextUVComponent.generated.h"

class UTextureRenderTarget2D;
class UTexture2D;
class UStarflightEmulatorSubsystem;

/**
 * Component that converts the 160x200 rotoscope metadata into a float render target
 * containing glyph UV coordinates (R/G) plus font and character selection (B/A).
 * The texture can be sampled in materials to drive custom upscaling workflows.
 */
UCLASS(ClassGroup = (Starflight), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class STARFLIGHTRUNTIME_API UStarflightTextUVComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStarflightTextUVComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Width of the generated UV render target (in pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 OutputWidth = 1024;

	/** Height of the generated UV render target (in pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 OutputHeight = 640;

	/** Render target updated every frame with UV + glyph selection data. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (AllowPrivateAccess = "true"))
	UTextureRenderTarget2D* TextUVRenderTarget = nullptr;

	/** Packed Content|Font|Char|Flags data (RGBA8, 160x200). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (AllowPrivateAccess = "true"))
	UTexture2D* RotoResourceContentFontCharFlags = nullptr;

	/** Packed GlyphX|GlyphY|GlyphWidth|GlyphHeight data (RGBA32F, 160x200). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (AllowPrivateAccess = "true"))
	UTexture2D* RotoResourceGlyphXYWH = nullptr;

	/** Packed FG/BG color data (RG8, 160x200). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Starflight|TextUV", meta = (AllowPrivateAccess = "true"))
	UTexture2D* RotoResourceFGBGColor = nullptr;

	/** When true, write EXR files for each generated texture every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDumpTexturesEachFrame = false;

	/** Directory that receives per-frame EXR dumps (defaults to C:/Temp). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starflight|Debug", meta = (AllowPrivateAccess = "true", EditCondition = "bDumpTexturesEachFrame"))
	FString TextureDumpDirectory = TEXT("C:/Temp");

private:
	void HandleRotoscopeMeta(const FStarflightRotoTexel* Texels, int32 Width, int32 Height);
	void UpdateUVTexture();
	void InitializeRenderTarget();
	void InitializeRotoDataResources();
	void DumpTexturesToExr(const TArray<FFloat16Color>& UVData, const TArray<uint8>& ContentData, const TArray<FLinearColor>& GlyphData, const TArray<uint8>& FGBGColorData, const TArray<uint8>& ARGBColorData, int32 DestW, int32 DestH);
	FString ResolveDumpDirectory() const;

	TWeakObjectPtr<UStarflightEmulatorSubsystem> EmulatorSubsystem;
	FDelegateHandle MetaListenerHandle;

	FCriticalSection MetaMutex;
	TArray<FStarflightRotoTexel> LatestTexels;
	int32 SourceWidth = 0;
	int32 SourceHeight = 0;
	uint64 PendingRevision = 0;
	uint64 ProcessedRevision = 0;

	bool bRenderTargetInitialized = false;
	uint64 DumpFrameCounter = 0;
};

