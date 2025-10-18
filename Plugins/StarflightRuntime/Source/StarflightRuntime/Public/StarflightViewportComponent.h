#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Texture2D.h"
#include "StarflightViewportComponent.generated.h"

UCLASS()
class STARFLIGHTRUNTIME_API AStarflightHUD : public AHUD
{
	GENERATED_BODY()

public:
	AStarflightHUD();

	UPROPERTY(BlueprintReadOnly, Category = "Starflight")
	UTexture2D* OutputTexture;

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

	void OnFrame(const uint8* BGRA, int W, int H, int Pitch);
	void UpdateTexture();
	void FillTextureSolid(const FColor& Color);
};