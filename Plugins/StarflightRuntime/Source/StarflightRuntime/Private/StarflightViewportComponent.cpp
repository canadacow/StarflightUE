#ifdef UpdateResource
#undef UpdateResource
#endif

#include "StarflightViewportComponent.h"
#include "StarflightBridge.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
 #include "EngineUtils.h"

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

	// Start the emulator when the HUD begins (when Play is pressed)
	StartStarflight();

    // Bind to a mesh that uses the Screen material so we can drive it at runtime
    TryBindScreenMID();

	UE_LOG(LogStarflightHUD, Warning, TEXT("Starflight HUD started and emulator launched"));
}

void AStarflightHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	// Stop the emulator when the HUD ends (when Play is stopped)
	StopStarflight();
	
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
	const int32 ExpectedSize = LocalW * LocalH * 4;
	if (LocalCopy.Num() != ExpectedSize)
	{
		UE_LOG(LogStarflightHUD, Error, TEXT("Data size mismatch: %d bytes for %dx%d (expected %d)"), 
			LocalCopy.Num(), LocalW, LocalH, ExpectedSize);
		return;
	}

	// Update texture using platform data (safer than UpdateTextureRegions)
	FTexture2DMipMap& Mip = OutputTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, LocalCopy.GetData(), ExpectedSize);
	Mip.BulkData.Unlock();
	OutputTexture->UpdateResource();
    PushTextureToMID();
}

void AStarflightHUD::FillTextureSolid(const FColor& Color)
{
	if (!OutputTexture || !OutputTexture->IsValidLowLevel()) return;

	// Ensure texture matches desired size
	if (Width != OutputTexture->GetSizeX() || Height != OutputTexture->GetSizeY())
	{
		OutputTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		OutputTexture->SRGB = false;
		OutputTexture->Filter = TF_Nearest;
		OutputTexture->UpdateResource();
	}

	const int32 LocalW = OutputTexture->GetSizeX();
	const int32 LocalH = OutputTexture->GetSizeY();
	const int32 NumPixels = LocalW * LocalH;
	const int32 NumBytes = NumPixels * 4;

	// Build a temporary buffer of BGRA8
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(NumBytes);
	const uint8 B = Color.B;
	const uint8 G = Color.G;
	const uint8 R = Color.R;
	const uint8 A = Color.A;
	for (int32 i = 0; i < NumPixels; ++i)
	{
		const int32 o = i * 4;
		Bytes[o + 0] = B;
		Bytes[o + 1] = G;
		Bytes[o + 2] = R;
		Bytes[o + 3] = A;
	}

	FTexture2DMipMap& Mip = OutputTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Bytes.GetData(), NumBytes);
	Mip.BulkData.Unlock();
	OutputTexture->UpdateResource();
}

void AStarflightHUD::TryBindScreenMID()
{
    if (ScreenMID.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TArray<UMeshComponent*> Meshes;
            It->GetComponents<UMeshComponent>(Meshes);
            for (UMeshComponent* MC : Meshes)
            {
                const int32 Num = MC->GetNumMaterials();
                for (int32 i = 0; i < Num; ++i)
                {
                    if (UMaterialInterface* Mat = MC->GetMaterial(i))
                    {
                        if (Mat->GetFName() == ScreenMaterialName)
                        {
                            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
                            MC->SetMaterial(i, MID);
                            ScreenMID = MID;
                            ScreenMesh = MC;
                            ScreenElementIndex = i;
                            PushTextureToMID();
                            return;
                        }
                    }
                }
            }
        }
    }
}

void AStarflightHUD::PushTextureToMID()
{
    if (ScreenMID.IsValid() && OutputTexture)
    {
        ScreenMID->SetTextureParameterValue(TextureParamName, OutputTexture);
    }
}

void AStarflightHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bDebugAlternating)
	{
		// Alternate between red and blue every frame
		const bool bRed = (FrameCounter++ % 2) == 0;
		FillTextureSolid(bRed ? FColor(255, 0, 0, 255) : FColor(0, 0, 255, 255));
	}
	else
	{
		UpdateTexture();
	}

    if (!ScreenMID.IsValid() && OutputTexture && Canvas)
	{
		// Draw texture fullscreen
		FCanvasTileItem TileItem(FVector2D(0, 0), OutputTexture->GetResource(), FVector2D(Canvas->SizeX, Canvas->SizeY), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(TileItem);
		
		//UE_LOG(LogStarflightHUD, Log, TEXT("Drew texture %dx%d to canvas %dx%d"), OutputTexture->GetSizeX(), OutputTexture->GetSizeY(), (int)Canvas->SizeX, (int)Canvas->SizeY);
	}
    else if (!ScreenMID.IsValid())
	{
		UE_LOG(LogStarflightHUD, Warning, TEXT("DrawHUD called but OutputTexture=%p Canvas=%p"), OutputTexture, (void*)Canvas);
	}
}