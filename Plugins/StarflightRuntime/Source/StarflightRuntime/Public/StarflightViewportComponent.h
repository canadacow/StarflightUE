#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "Engine/Texture2D.h"
#include "StarflightViewportComponent.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;
class UTextureRenderTarget2D;
class UStarflightEmulatorSubsystem;

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

	TWeakObjectPtr<UStarflightEmulatorSubsystem> EmulatorSubsystem;
	FDelegateHandle FrameListenerHandle;
	FDelegateHandle RotoListenerHandle;
};

UCLASS(ClassGroup = (Starflight), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class STARFLIGHTRUNTIME_API UStarflightViewportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStarflightViewportComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 640x200/400 upscaled emulator output
	UPROPERTY(BlueprintReadOnly, Category = "Starflight")
	UTextureRenderTarget2D* UpscaledRenderTarget = nullptr;

	// 3840x1200 CRT output (compute shader, trilinear filtered with mips)
	UPROPERTY(BlueprintReadOnly, Category = "Starflight")
	UTextureRenderTarget2D* CRT6x6RenderTarget = nullptr;

	// Material parameter and target material name used for the in-world screen
	UPROPERTY(EditAnywhere, Category = "Starflight|Screen")
	FName TextureParamName = TEXT("Tex");

	UPROPERTY(EditAnywhere, Category = "Starflight|Screen")
	FName ScreenMaterialName = TEXT("Screen_WithPlugin");

protected:
	void HandleFrame(const uint8* BGRA, int32 InWidth, int32 InHeight, int32 InPitch);
	void UpdateTexture();
	void GenerateCRT6x6();

private:
	TWeakObjectPtr<UStarflightEmulatorSubsystem> EmulatorSubsystem;
	FDelegateHandle ComponentFrameListenerHandle;

	FCriticalSection ComponentFrameMutex;
	TArray<uint8> LatestFrame;
	int32 LatestPitch = 0;
	int32 Width = 640;
	int32 Height = 360;
	uint64 FrameCounter = 0;

	// Intermediate 640x400 CPU-upscaled texture used for blitting to the RT
	UTexture2D* UpscaledIntermediateTexture = nullptr;

	// Runtime binding to a mesh using the Screen material
	TWeakObjectPtr<UMaterialInstanceDynamic> ScreenMID;
	TWeakObjectPtr<UMeshComponent> ScreenMesh;
	int32 ScreenElementIndex = 0;

	void TryBindScreenMID();
	void PushTextureToMID();
};