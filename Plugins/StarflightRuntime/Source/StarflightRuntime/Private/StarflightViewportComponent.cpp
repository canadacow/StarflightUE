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
	LatestFrame.SetNum(W * H * 4);
	FMemory::Memcpy(LatestFrame.GetData(), BGRA, (SIZE_T)(H * Pitch));
	LatestPitch = Pitch;
	Width = W;
	Height = H;
}

void AStarflightHUD::UpdateTexture()
{
	if (!OutputTexture) return;

	TArray<uint8> LocalCopy;
	int32 LocalW, LocalH;
	{
		FScopeLock Lock(&FrameMutex);
		if (LatestFrame.Num() == 0) return;
		LocalCopy = LatestFrame;
		LocalW = Width;
		LocalH = Height;
	}

	FTexture2DMipMap& Mip = OutputTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, LocalCopy.GetData(), LocalCopy.Num());
	Mip.BulkData.Unlock();
	OutputTexture->UpdateResource();
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