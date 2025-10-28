#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Texture2D.h"
#include "Components/ActorComponent.h"
#include "Components/MeshComponent.h"
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


// Component that receives emulator frames and applies them to a mesh material parameter
UCLASS(ClassGroup=(Starflight), meta=(BlueprintSpawnableComponent))
class STARFLIGHTRUNTIME_API UStarflightViewportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStarflightViewportComponent();

	// Target mesh to drive. If unset, the first UMeshComponent on the owner will be used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight")
	UMeshComponent* TargetMesh = nullptr;

	// Material element index on the mesh to set (default 0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight")
	int32 MaterialElementIndex = 0;

	// Texture parameter name in the material (must be a TextureSampleParameter2D)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight")
	FName TextureParameterName = TEXT("Screen");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Runtime texture updated from emulator frames
	UPROPERTY(Transient)
	UTexture2D* OutputTexture = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicMID = nullptr;

	FCriticalSection FrameMutex;
	TArray<uint8> LatestFrame;
	int32 LatestPitch = 0;
	int32 Width = 640;
	int32 Height = 360;

	void OnFrame(const uint8* BGRA, int W, int H, int Pitch);
	void UpdateTexture();
	void EnsureMID();
};