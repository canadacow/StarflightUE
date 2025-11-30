#include "StarflightTextUVComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "Templates/SharedPointer.h"
#include "StarflightEmulatorSubsystem.h"
#include "RHICommandList.h"
#include "RHIResources.h"

namespace
{
constexpr int32 GRotoSourceWidth = 160;
constexpr int32 GRotoSourceHeight = 200;
constexpr int32 GRotoSourcePixelCount = GRotoSourceWidth * GRotoSourceHeight;
}

UStarflightTextUVComponent::UStarflightTextUVComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UStarflightTextUVComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeRenderTarget();
	InitializeRotoDataResources();

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
	checkf(TextUVRenderTarget, TEXT("Failed to allocate TextUVRenderTarget"));

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

void UStarflightTextUVComponent::InitializeRotoDataResources()
{
	if (RotoResourceContentFontCharFlags &&
		RotoResourceGlyphXYWH &&
		RotoResourceFGBGColor)
	{
		return;
	}

	auto ConfigureDataTexture = [](UTexture2D* Texture, const TCHAR* DebugName)
	{
		checkf(Texture, TEXT("Failed to create %s"), DebugName);
		Texture->SRGB = false;
		Texture->Filter = TF_Nearest;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		Texture->NeverStream = true;
		Texture->UpdateResource();
	};

	if (!RotoResourceContentFontCharFlags)
	{
		RotoResourceContentFontCharFlags = UTexture2D::CreateTransient(GRotoSourceWidth, GRotoSourceHeight, PF_R8G8B8A8);
		ConfigureDataTexture(RotoResourceContentFontCharFlags, TEXT("RotoResourceContentFontCharFlags"));
	}

	if (!RotoResourceGlyphXYWH)
	{
		RotoResourceGlyphXYWH = UTexture2D::CreateTransient(GRotoSourceWidth, GRotoSourceHeight, PF_R16G16B16A16_UINT);
		ConfigureDataTexture(RotoResourceGlyphXYWH, TEXT("RotoResourceGlyphXYWH"));
	}

	if (!RotoResourceFGBGColor)
	{
		RotoResourceFGBGColor = UTexture2D::CreateTransient(GRotoSourceWidth, GRotoSourceHeight, PF_R8G8);
		ConfigureDataTexture(RotoResourceFGBGColor, TEXT("RotoResourceFGBGColor"));
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

	InitializeRotoDataResources();

	checkf(TextUVRenderTarget, TEXT("TextUVRenderTarget should have been initialized before UpdateUVTexture"));
	checkf(RotoResourceContentFontCharFlags && RotoResourceGlyphXYWH && RotoResourceFGBGColor,
		TEXT("Roto data textures must be initialized before UpdateUVTexture"));

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

	checkf(LocalWidth == GRotoSourceWidth && LocalHeight == GRotoSourceHeight,
		TEXT("Rotoscope buffer must remain %dx%d but received %dx%d"),
		GRotoSourceWidth, GRotoSourceHeight, LocalWidth, LocalHeight);

	checkf(LocalTexels.Num() == GRotoSourcePixelCount,
		TEXT("Unexpected rotoscope texel count (%d)"), LocalTexels.Num());

	const int32 DestW = FMath::Max(16, OutputWidth);
	const int32 DestH = FMath::Max(16, OutputHeight);
	const float ScaleX = static_cast<float>(LocalWidth) / static_cast<float>(DestW);
	const float ScaleY = static_cast<float>(LocalHeight) / static_cast<float>(DestH);

	TArray<FFloat16Color> PixelData;
	PixelData.SetNumUninitialized(DestW * DestH);

	TArray<uint8> ContentFontCharFlagsData;
	ContentFontCharFlagsData.SetNumUninitialized(GRotoSourcePixelCount * 4);

	TArray<uint16> GlyphXYWHData;
	GlyphXYWHData.SetNumUninitialized(GRotoSourcePixelCount * 4);

	TArray<uint8> FGBGColorData;
	FGBGColorData.SetNumUninitialized(GRotoSourcePixelCount * 2);

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

	for (int32 Index = 0; Index < GRotoSourcePixelCount; ++Index)
	{
		const FStarflightRotoTexel& Texel = LocalTexels[Index];

		const int32 ContentOffset = Index * 4;
		ContentFontCharFlagsData[ContentOffset + 0] = Texel.Content;
		ContentFontCharFlagsData[ContentOffset + 1] = Texel.FontNumber;
		ContentFontCharFlagsData[ContentOffset + 2] = Texel.Character;
		ContentFontCharFlagsData[ContentOffset + 3] = Texel.Flags;

		const int32 GlyphOffset = Index * 4;
		GlyphXYWHData[GlyphOffset + 0] = static_cast<uint16>(Texel.GlyphX);
		GlyphXYWHData[GlyphOffset + 1] = static_cast<uint16>(Texel.GlyphY);
		GlyphXYWHData[GlyphOffset + 2] = static_cast<uint16>(Texel.GlyphWidth);
		GlyphXYWHData[GlyphOffset + 3] = static_cast<uint16>(Texel.GlyphHeight);

		const int32 ColorOffset = Index * 2;
		FGBGColorData[ColorOffset + 0] = Texel.FGColor;
		FGBGColorData[ColorOffset + 1] = Texel.BGColor;
	}

	FTextureRenderTargetResource* Resource = TextUVRenderTarget->GameThread_GetRenderTargetResource();
	checkf(Resource, TEXT("TextUVRenderTarget resource missing"));

	FTextureResource* ContentResource = RotoResourceContentFontCharFlags->GetResource();
	checkf(ContentResource, TEXT("RotoResourceContentFontCharFlags missing resource"));
	FTextureResource* GlyphResource = RotoResourceGlyphXYWH->GetResource();
	checkf(GlyphResource, TEXT("RotoResourceGlyphXYWH missing resource"));
	FTextureResource* ColorResource = RotoResourceFGBGColor->GetResource();
	checkf(ColorResource, TEXT("RotoResourceFGBGColor missing resource"));

	TSharedPtr<TArray<FFloat16Color>, ESPMode::ThreadSafe> Buffer = MakeShared<TArray<FFloat16Color>, ESPMode::ThreadSafe>(MoveTemp(PixelData));
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ContentBuffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(ContentFontCharFlagsData));
	TSharedPtr<TArray<uint16>, ESPMode::ThreadSafe> GlyphBuffer = MakeShared<TArray<uint16>, ESPMode::ThreadSafe>(MoveTemp(GlyphXYWHData));
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> FGBGBuffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(FGBGColorData));

	const FTexture2DRHIRef ContentTextureRHI = ContentResource->GetTexture2DRHI();
	checkf(ContentTextureRHI.IsValid(), TEXT("Content texture RHI missing"));
	const FTexture2DRHIRef GlyphTextureRHI = GlyphResource->GetTexture2DRHI();
	checkf(GlyphTextureRHI.IsValid(), TEXT("Glyph texture RHI missing"));
	const FTexture2DRHIRef ColorTextureRHI = ColorResource->GetTexture2DRHI();
	checkf(ColorTextureRHI.IsValid(), TEXT("Color texture RHI missing"));

	ENQUEUE_RENDER_COMMAND(UpdateStarflightTextResources)(
		[TexResource = Resource,
			ContentTextureRHI,
			GlyphTextureRHI,
			ColorTextureRHI,
			Buffer,
			ContentBuffer,
			GlyphBuffer,
			FGBGBuffer,
			DestW,
			DestH](FRHICommandListImmediate& RHICmdList)
		{
			check(TexResource);
			check(ContentTextureRHI.IsValid());
			check(GlyphTextureRHI.IsValid());
			check(ColorTextureRHI.IsValid());

			FRHITexture2D* TextureRHI = TexResource->GetTexture2DRHI();
			check(TextureRHI);

			const uint32 SrcPitch = DestW * sizeof(FFloat16Color);
			const FUpdateTextureRegion2D Region(0, 0, 0, 0, DestW, DestH);
			RHICmdList.UpdateTexture2D(TextureRHI, 0, Region, SrcPitch, reinterpret_cast<const uint8*>(Buffer->GetData()));

			const FUpdateTextureRegion2D RotoRegion(0, 0, 0, 0, GRotoSourceWidth, GRotoSourceHeight);

			const uint32 ContentPitch = GRotoSourceWidth * sizeof(uint8) * 4;
			RHICmdList.UpdateTexture2D(ContentTextureRHI, 0, RotoRegion, ContentPitch, ContentBuffer->GetData());

			const uint32 GlyphPitch = GRotoSourceWidth * sizeof(uint16) * 4;
			RHICmdList.UpdateTexture2D(GlyphTextureRHI, 0, RotoRegion, GlyphPitch, reinterpret_cast<const uint8*>(GlyphBuffer->GetData()));

			const uint32 ColorPitch = GRotoSourceWidth * sizeof(uint8) * 2;
			RHICmdList.UpdateTexture2D(ColorTextureRHI, 0, RotoRegion, ColorPitch, FGBGBuffer->GetData());
		});

	ProcessedRevision = LocalRevision;
}

