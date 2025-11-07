#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Texture2D.h"
#include "StarflightViewportComponent.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;

UCLASS()
class STARFLIGHTRUNTIME_API AStarflightHUD : public AHUD
{
	GENERATED_BODY()

public:
	AStarflightHUD();

	UPROPERTY(BlueprintReadOnly, Category = "Starflight")
	UTexture2D* OutputTexture;

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

	// Runtime binding to a mesh using the Screen material
	TWeakObjectPtr<UMaterialInstanceDynamic> ScreenMID;
	TWeakObjectPtr<UMeshComponent> ScreenMesh;
	int32 ScreenElementIndex = 0;

	void OnFrame(const uint8* BGRA, int W, int H, int Pitch);
	void UpdateTexture();
	void FillTextureSolid(const FColor& Color);
	void TryBindScreenMID();
	void PushTextureToMID();
};