#include "StarflightTextUVComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "Templates/SharedPointer.h"
#include "StarflightEmulatorSubsystem.h"
#include "RHICommandList.h"

UStarflightTextUVComponent::UStarflightTextUVComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UStarflightTextUVComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeRenderTarget();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UStarflightEmulatorSubsystem* Subsystem = GameInstance->GetSubsystem<UStarflightEmulatorSubsystem>())
			{
				EmulatorSubsystem = Subsystem;

				TWeakObjectPtr<UStarflightTextUVComponent> WeakThis(this);
				MetaListenerHandle = Subsystem->RegisterRotoscopeMetaListener(
					[WeakThis](const FStarflightRotoTexel* Texels, int32 Width, int32 Height)
					{
						if (UStarflightTextUVComponent* StrongThis = WeakThis.Get())
						{
							StrongThis->HandleRotoscopeMeta(Texels, Width, Height);
						}
					});
			}
		}
	}
}

void UStarflightTextUVComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UStarflightEmulatorSubsystem* Subsystem = EmulatorSubsystem.Get())
	{
		if (MetaListenerHandle.IsValid())
		{
			Subsystem->UnregisterRotoscopeMetaListener(MetaListenerHandle);
			MetaListenerHandle.Reset();
		}
	}

	LatestTexels.Reset();
	EmulatorSubsystem = nullptr;
	bRenderTargetInitialized = false;

	Super::EndPlay(EndPlayReason);
}

void UStarflightTextUVComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateUVTexture();
}

void UStarflightTextUVComponent::InitializeRenderTarget()
{
	if (bRenderTargetInitialized)
	{
		return;
	}

	TextUVRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	if (TextUVRenderTarget)
	{
		TextUVRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
		TextUVRenderTarget->ClearColor = FLinearColor::Black;
		TextUVRenderTarget->bAutoGenerateMips = false;
		TextUVRenderTarget->bCanCreateUAV = true;
		TextUVRenderTarget->Filter = TF_Bilinear;
		TextUVRenderTarget->AddressX = TA_Clamp;
		TextUVRenderTarget->AddressY = TA_Clamp;
		TextUVRenderTarget->SRGB = false;
		TextUVRenderTarget->InitAutoFormat(FMath::Max(16, OutputWidth), FMath::Max(16, OutputHeight));
		TextUVRenderTarget->UpdateResourceImmediate(true);
		bRenderTargetInitialized = true;
	}
}

void UStarflightTextUVComponent::HandleRotoscopeMeta(const FStarflightRotoTexel* Texels, int32 Width, int32 Height)
{
	if (!Texels || Width <= 0 || Height <= 0)
	{
		return;
	}

	const int32 Count = Width * Height;

	FScopeLock Lock(&MetaMutex);
	LatestTexels.SetNumUninitialized(Count);
	FMemory::Memcpy(LatestTexels.GetData(), Texels, sizeof(FStarflightRotoTexel) * Count);
	SourceWidth = Width;
	SourceHeight = Height;
	++PendingRevision;
}

void UStarflightTextUVComponent::UpdateUVTexture()
{
	if (!TextUVRenderTarget)
	{
		InitializeRenderTarget();
	}

	if (!TextUVRenderTarget)
	{
		return;
	}

	TArray<FStarflightRotoTexel> LocalTexels;
	int32 LocalWidth = 0;
	int32 LocalHeight = 0;
	uint64 LocalRevision = 0;

	{
		FScopeLock Lock(&MetaMutex);
		if (PendingRevision == ProcessedRevision || LatestTexels.Num() == 0)
		{
			return;
		}

		LocalTexels = LatestTexels;
		LocalWidth = SourceWidth;
		LocalHeight = SourceHeight;
		LocalRevision = PendingRevision;
	}

	if (LocalWidth <= 0 || LocalHeight <= 0)
	{
		return;
	}

	const int32 DestW = FMath::Max(16, OutputWidth);
	const int32 DestH = FMath::Max(16, OutputHeight);
	const float ScaleX = static_cast<float>(LocalWidth) / static_cast<float>(DestW);
	const float ScaleY = static_cast<float>(LocalHeight) / static_cast<float>(DestH);

	TArray<FFloat16Color> PixelData;
	PixelData.SetNumUninitialized(DestW * DestH);

	for (int32 y = 0; y < DestH; ++y)
	{
		const int32 SrcY = FMath::Clamp(FMath::FloorToInt(y * ScaleY), 0, LocalHeight - 1);
		for (int32 x = 0; x < DestW; ++x)
		{
			const int32 SrcX = FMath::Clamp(FMath::FloorToInt(x * ScaleX), 0, LocalWidth - 1);
			const FStarflightRotoTexel& Texel = LocalTexels[SrcY * LocalWidth + SrcX];

			FFloat16Color Encoded(FLinearColor::Black);
			if (Texel.Content == static_cast<uint8>(FStarflightRotoContent::Text) &&
				Texel.GlyphWidth > 0 && Texel.GlyphHeight > 0)
			{
				const float U = FMath::Clamp(
					(static_cast<float>(Texel.GlyphX) + 0.5f) / static_cast<float>(Texel.GlyphWidth), 0.0f, 1.0f);
				const float V = FMath::Clamp(
					(static_cast<float>(Texel.GlyphY) + 0.5f) / static_cast<float>(Texel.GlyphHeight), 0.0f, 1.0f);
				const float FontEncoded = (Texel.FontNumber > 0)
					? FMath::Clamp((static_cast<float>(Texel.FontNumber) + 1.0f) / 8.0f, 0.0f, 1.0f)
					: 0.0f;
				const float CharEncoded = FMath::Clamp((static_cast<float>(Texel.Character) + 1.0f) / 256.0f, 0.0f, 1.0f);

				Encoded = FFloat16Color(FLinearColor(U, V, FontEncoded, CharEncoded));
			}

			PixelData[y * DestW + x] = Encoded;
		}
	}

	FTextureRenderTargetResource* Resource = TextUVRenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return;
	}

	TSharedPtr<TArray<FFloat16Color>, ESPMode::ThreadSafe> Buffer = MakeShared<TArray<FFloat16Color>, ESPMode::ThreadSafe>(MoveTemp(PixelData));

	ENQUEUE_RENDER_COMMAND(UpdateStarflightTextUV)(
		[TexResource = Resource, Buffer, DestW, DestH](FRHICommandListImmediate& RHICmdList)
		{
			if (!TexResource)
			{
				return;
			}

			FRHITexture2D* TextureRHI = TexResource->GetTexture2DRHI();
			if (!TextureRHI)
			{
				return;
			}

			const uint32 SrcPitch = DestW * sizeof(FFloat16Color);
			const FUpdateTextureRegion2D Region(0, 0, 0, 0, DestW, DestH);
			RHICmdList.UpdateTexture2D(TextureRHI, 0, Region, SrcPitch, reinterpret_cast<const uint8*>(Buffer->GetData()));
		});

	ProcessedRevision = LocalRevision;
}

