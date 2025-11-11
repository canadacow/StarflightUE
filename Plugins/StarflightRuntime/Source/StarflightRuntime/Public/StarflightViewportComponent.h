#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Texture2D.h"
#include "StarflightViewportComponent.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;
class UTextureRenderTarget2D;

UCLASS()
class STARFLIGHTRUNTIME_API AStarflightHUD : public AHUD
{
	GENERATED_BODY()

public:
	AStarflightHUD();

    UPROPERTY(BlueprintReadOnly, Category = "Starflight")
    UTextureRenderTarget2D* UpscaledRenderTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Starflight")
    UTextureRenderTarget2D* CRT6x6RenderTarget;

	// Material parameter and target material name used for the in-world screen
	UPROPERTY(EditAnywhere, Category = "Starflight|Screen")
	FName TextureParamName = TEXT("Tex");

	UPROPERTY(EditAnywhere, Category = "Starflight|Screen")
	FName ScreenMaterialName = TEXT("Screen_WithPlugin");

	virtual void DrawHUD() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FCriticalSection FrameMutex;
	TArray<uint8> LatestFrame;
	int32 LatestPitch = 0;
	int32 Width = 640;
	int32 Height = 360;
	bool bDebugAlternating = false;
	uint64 FrameCounter = 0;
	uint32 DumpCounter = 0;

	// Debug: dump compute output frames (3840x1200) to PNG each frame
	UPROPERTY(EditAnywhere, Category = "Starflight|Debug")
	bool bDumpComputeOutput = false;
	uint32 ComputeDumpCounter = 0;

	// Runtime binding to a mesh using the Screen material
	TWeakObjectPtr<UMaterialInstanceDynamic> ScreenMID;
	TWeakObjectPtr<UMeshComponent> ScreenMesh;
	int32 ScreenElementIndex = 0;

    void OnFrame(const uint8* BGRA, int W, int H, int Pitch);
    void UpdateTexture();
	void FillTextureSolid(const FColor& Color);
	void TryBindScreenMID();
	void PushTextureToMID();
	void GenerateCRT6x6();

    // Intermediate 640x400 CPU-upscaled texture used for blitting to the RT
    UTexture2D* UpscaledIntermediateTexture = nullptr;

	// Rotoscope 160x200 debug overlay
	FCriticalSection RotoMutex;
	TArray<uint8> LatestRoto;
	int32 RotoW = 160;
	int32 RotoH = 200;
	int32 RotoPitch = 160 * 4;
	UTexture2D* RotoscopeTexture = nullptr;

	void OnRotoscope(const uint8* BGRA, int W, int H, int Pitch);
	void UpdateRotoscopeTexture();
};