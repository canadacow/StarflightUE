#ifdef UpdateResource
#undef UpdateResource
#endif

#include "StarflightViewportComponent.h"
#include "StarflightBridge.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightHUD, Log, All);

AStarflightHUD::AStarflightHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AStarflightHUD::BeginPlay()
{
	Super::BeginPlay();

	OutputTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	OutputTexture->SRGB = false;
	OutputTexture->Filter = TF_Nearest;
	OutputTexture->UpdateResource();

	SetFrameSink([this](const uint8* BGRA, int W, int H, int Pitch)
	{
		OnFrame(BGRA, W, H, Pitch);
	});

	UE_LOG(LogStarflightHUD, Warning, TEXT("Starflight HUD started"));
}

void AStarflightHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	SetFrameSink(nullptr);
}

void AStarflightHUD::OnFrame(const uint8* BGRA, int W, int H, int Pitch)
{
	FScopeLock Lock(&FrameMutex);
	Width = W;
	Height = H;
	LatestPitch = Pitch;
	
	// Allocate buffer for the frame
	LatestFrame.SetNum(W * H * 4);
	
	// Copy row by row if pitch != width * 4, otherwise copy directly
	if (Pitch == W * 4)
	{
		FMemory::Memcpy(LatestFrame.GetData(), BGRA, W * H * 4);
	}
	else
	{
		// Copy row by row to handle pitch correctly
		for (int y = 0; y < H; ++y)
		{
			FMemory::Memcpy(
				LatestFrame.GetData() + y * W * 4,
				BGRA + y * Pitch,
				W * 4
			);
		}
	}
}

void AStarflightHUD::UpdateTexture()
{
	if (!OutputTexture || !OutputTexture->IsValidLowLevel()) return;

	TArray<uint8> LocalCopy;
	int32 LocalW, LocalH;
	{
		FScopeLock Lock(&FrameMutex);
		if (LatestFrame.Num() == 0) return;
		LocalCopy = LatestFrame;
		LocalW = Width;
		LocalH = Height;
	}

	// Recreate texture if size changed
	if (LocalW != OutputTexture->GetSizeX() || LocalH != OutputTexture->GetSizeY())
	{
		UE_LOG(LogStarflightHUD, Warning, TEXT("Recreating texture: %dx%d -> %dx%d"), 
			OutputTexture->GetSizeX(), OutputTexture->GetSizeY(), LocalW, LocalH);
		OutputTexture = UTexture2D::CreateTransient(LocalW, LocalH, PF_B8G8R8A8);
		OutputTexture->SRGB = false;
		OutputTexture->Filter = TF_Nearest;
		OutputTexture->UpdateResource();
	}

	// Validate data size
	if (LocalCopy.Num() != LocalW * LocalH * 4)
	{
		UE_LOG(LogStarflightHUD, Error, TEXT("Data size mismatch: %d bytes for %dx%d (expected %d)"), 
			LocalCopy.Num(), LocalW, LocalH, LocalW * LocalH * 4);
		return;
	}

	// Use UpdateTextureRegions for safe texture update
	FUpdateTextureRegion2D Region(0, 0, 0, 0, LocalW, LocalH);
	OutputTexture->UpdateTextureRegions(0, 1, &Region, LocalW * 4, 4, LocalCopy.GetData());
}

void AStarflightHUD::DrawHUD()
{
	Super::DrawHUD();

	UpdateTexture();

	if (OutputTexture && Canvas)
	{
		// Draw texture fullscreen
		FCanvasTileItem TileItem(FVector2D(0, 0), OutputTexture->GetResource(), FVector2D(Canvas->SizeX, Canvas->SizeY), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(TileItem);
		
		UE_LOG(LogStarflightHUD, Log, TEXT("Drew texture %dx%d to canvas %dx%d"), OutputTexture->GetSizeX(), OutputTexture->GetSizeY(), (int)Canvas->SizeX, (int)Canvas->SizeY);
	}
	else
	{
		UE_LOG(LogStarflightHUD, Warning, TEXT("DrawHUD called but OutputTexture=%p Canvas=%p"), OutputTexture, (void*)Canvas);
	}
}