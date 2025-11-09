#ifdef UpdateResource
#undef UpdateResource
#endif

#include "StarflightViewportComponent.h"
#include "StarflightBridge.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasTypes.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "GenerateMips.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
 #include "EngineUtils.h"
 #include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightHUD, Log, All);

// -----------------------------------------------------------------------------
// Output configuration (override via Build.cs or compiler defines if desired)
// - SF_OUTPUT_WIDTH:  output width in pixels (default 640)
// - SF_OUTPUT_HEIGHT: output height in pixels: 200 (no scanlines) or 400 (scanlines/doubling)
// - SF_SCANLINE_BLACK: when height == 400, 1 = black scanlines, 0 = line-doubling
// -----------------------------------------------------------------------------
#ifndef SF_OUTPUT_WIDTH
#define SF_OUTPUT_WIDTH 640
#endif
#ifndef SF_OUTPUT_HEIGHT
#define SF_OUTPUT_HEIGHT 200
#endif
#ifndef SF_SCANLINE_BLACK
#define SF_SCANLINE_BLACK 0
#endif
static_assert(SF_OUTPUT_WIDTH == 640, "This HUD assumes 640px output width.");
static_assert(SF_OUTPUT_HEIGHT == 200 || SF_OUTPUT_HEIGHT == 400, "SF_OUTPUT_HEIGHT must be 200 or 400.");

AStarflightHUD::AStarflightHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AStarflightHUD::BeginPlay()
{
	Super::BeginPlay();

    // Create upscaled RT (with mips) and an intermediate texture for CPU blit
    UpscaledRenderTarget = NewObject<UTextureRenderTarget2D>(this);
    UpscaledRenderTarget->ClearColor = FLinearColor::Black;
    UpscaledRenderTarget->bAutoGenerateMips = true;
    UpscaledRenderTarget->bCanCreateUAV = false;
    UpscaledRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
    UpscaledRenderTarget->InitAutoFormat(SF_OUTPUT_WIDTH, SF_OUTPUT_HEIGHT);
    UpscaledRenderTarget->UpdateResourceImmediate(true);

    UpscaledIntermediateTexture = UTexture2D::CreateTransient(SF_OUTPUT_WIDTH, SF_OUTPUT_HEIGHT, PF_B8G8R8A8);
    UpscaledIntermediateTexture->SRGB = true;
    UpscaledIntermediateTexture->Filter = TF_Trilinear;
    UpscaledIntermediateTexture->UpdateResource();

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

    // Restore editor-friendly input focus
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            FInputModeUIOnly Mode;
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = true;
        }
    }
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().ClearAllUserFocus(EFocusCause::SetDirectly);
        FSlateApplication::Get().ReleaseAllPointerCapture();
    }
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
    if (!UpscaledRenderTarget) return;

	TArray<uint8> LocalCopy;
    int32 LocalW, LocalH;
	{
		FScopeLock Lock(&FrameMutex);
		if (LatestFrame.Num() == 0) return;
		LocalCopy = LatestFrame;
		LocalW = Width;
		LocalH = Height;
	}

    // CPU format into a BGRA buffer as configured
    const int32 DstW = SF_OUTPUT_WIDTH;
    const int32 DstH = SF_OUTPUT_HEIGHT;
    TArray<uint8> Upscaled;
    Upscaled.SetNumZeroed(DstW * DstH * 4);

    // Compute horizontal integer scale to cover 640 exactly for common widths
    // 160 -> 4x, 320 -> 2x, 640 -> 1x
    const int32 ScaleX = FMath::Max(1, DstW / FMath::Max(1, LocalW));

    if (DstH == LocalH)
    {
        // 640x200: single line per source row, horizontal integer scaling only
        for (int32 sy = 0; sy < LocalH; ++sy)
        {
            const uint8* SrcRow = LocalCopy.GetData() + sy * LocalW * 4;
            uint8* DstRow = Upscaled.GetData() + sy * DstW * 4;

            for (int32 sx = 0; sx < LocalW; ++sx)
            {
                const uint8* SrcPx = SrcRow + sx * 4; // BGRA
                const int32 dx0 = sx * ScaleX;
                const int32 dx1 = FMath::Min(dx0 + ScaleX - 1, DstW - 1);
                for (int32 dx = dx0; dx <= dx1; ++dx)
                {
                    uint8* DstPx = DstRow + dx * 4;
                    DstPx[0] = SrcPx[0];
                    DstPx[1] = SrcPx[1];
                    DstPx[2] = SrcPx[2];
                    DstPx[3] = 255;
                }
            }
        }
    }
    else if (DstH == LocalH * 2)
    {
        // 640x400: either black scanlines or line-doubling
        for (int32 sy = 0; sy < LocalH; ++sy)
        {
            const int32 dy = sy * 2; // even rows
            const uint8* SrcRow = LocalCopy.GetData() + sy * LocalW * 4;
            uint8* DstRow0 = Upscaled.GetData() + dy * DstW * 4;
            uint8* DstRow1 = Upscaled.GetData() + (dy + 1) * DstW * 4;

            for (int32 sx = 0; sx < LocalW; ++sx)
            {
                const uint8* SrcPx = SrcRow + sx * 4; // BGRA
                const int32 dx0 = sx * ScaleX;
                const int32 dx1 = FMath::Min(dx0 + ScaleX - 1, DstW - 1);

                // write horizontally scaled pixels to two rows
                for (int32 dx = dx0; dx <= dx1; ++dx)
                {
                    uint8* DstPx0 = DstRow0 + dx * 4;
                    DstPx0[0] = SrcPx[0];
                    DstPx0[1] = SrcPx[1];
                    DstPx0[2] = SrcPx[2];
                    DstPx0[3] = 255;

                    uint8* DstPx1 = DstRow1 + dx * 4;
#if SF_SCANLINE_BLACK
                    DstPx1[0] = 0;
                    DstPx1[1] = 0;
                    DstPx1[2] = 0;
                    DstPx1[3] = 255;
#else
                    DstPx1[0] = SrcPx[0];
                    DstPx1[1] = SrcPx[1];
                    DstPx1[2] = SrcPx[2];
                    DstPx1[3] = 255;
#endif
                }
            }
        }
    }
    else
    {
        // Fallback: simple nearest-neighbor vertical scaling to DstH
        for (int32 dy = 0; dy < DstH; ++dy)
        {
            const int32 sy = FMath::Clamp((dy * LocalH) / FMath::Max(1, DstH), 0, LocalH - 1);
            const uint8* SrcRow = LocalCopy.GetData() + sy * LocalW * 4;
            uint8* DstRow = Upscaled.GetData() + dy * DstW * 4;
            for (int32 sx = 0; sx < LocalW; ++sx)
            {
                const uint8* SrcPx = SrcRow + sx * 4;
                const int32 dx0 = sx * ScaleX;
                const int32 dx1 = FMath::Min(dx0 + ScaleX - 1, DstW - 1);
                for (int32 dx = dx0; dx <= dx1; ++dx)
                {
                    uint8* DstPx = DstRow + dx * 4;
                    DstPx[0] = SrcPx[0];
                    DstPx[1] = SrcPx[1];
                    DstPx[2] = SrcPx[2]; 
                    DstPx[3] = 255;
                }
            }
        }
    }

#if 1
	// Dump images here
	{
		static bool bDumpDirReady = false;
		static FString DumpDir;
		if (!bDumpDirReady)
		{
			DumpDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("dump"));
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.CreateDirectoryTree(*DumpDir);
			bDumpDirReady = true;
		}

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> PngWriter = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (PngWriter.IsValid())
		{
			const int32 SrcW = DstW;
			const int32 SrcH = DstH;
			PngWriter->SetRaw(Upscaled.GetData(), Upscaled.Num(), SrcW, SrcH, ERGBFormat::BGRA, 8);

			const TArray64<uint8>& Compressed64 = PngWriter->GetCompressed(0);
			TArray<uint8> Compressed;
			Compressed.Append(Compressed64.GetData(), static_cast<int32>(Compressed64.Num()));

			const FString FileName = FString::Printf(TEXT("image_%05d.png"), static_cast<int32>(DumpCounter++));
			const FString FilePath = FPaths::Combine(DumpDir, FileName);
			FFileHelper::SaveArrayToFile(Compressed, *FilePath);
		}
	}
#endif


    // Upload CPU buffer directly to the RT on the render thread and generate mips
    if (FTextureRenderTargetResource* RTRes = UpscaledRenderTarget->GameThread_GetRenderTargetResource())
    {
        if (FRHITexture* RHITexture = RTRes->GetRenderTargetTexture())
        {
            TArray<uint8> BufferCopy = MoveTemp(Upscaled);
            const ERHIFeatureLevel::Type FeatureLevel = GetWorld()->GetFeatureLevel();
            ENQUEUE_RENDER_COMMAND(UpdateCRTTarget)(
                [RHITexture, BufferCopy = MoveTemp(BufferCopy), FeatureLevel](FRHICommandListImmediate& RHICmdList) mutable
                {
                    if (FRHITexture2D* Tex2D = RHITexture->GetTexture2D())
                    {
                        const uint32 SrcPitch = static_cast<uint32>(SF_OUTPUT_WIDTH) * 4u;
                        FUpdateTextureRegion2D Region(0, 0, 0, 0, static_cast<uint32>(SF_OUTPUT_WIDTH), static_cast<uint32>(SF_OUTPUT_HEIGHT));
                        RHICmdList.UpdateTexture2D(Tex2D, 0, Region, SrcPitch, BufferCopy.GetData());

                        FRDGBuilder GraphBuilder(RHICmdList);
                        FRDGTextureRef RDGTex = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHITexture, TEXT("CRT_RT")));
                        FGenerateMipsParams Params;
                        FGenerateMips::Execute(GraphBuilder, FeatureLevel, RDGTex, Params, EGenerateMipsPass::Compute);
                        GraphBuilder.Execute();
                    }
                }
            );
        }
    }

    PushTextureToMID();
}

void AStarflightHUD::FillTextureSolid(const FColor& Color)
{
    if (!UpscaledIntermediateTexture || !UpscaledRenderTarget) return;

    const int32 LocalW = SF_OUTPUT_WIDTH;
    const int32 LocalH = SF_OUTPUT_HEIGHT;
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

    if (FTextureRenderTargetResource* RTRes = UpscaledRenderTarget->GameThread_GetRenderTargetResource())
    {
        FRHITexture* RHITexture = RTRes->GetRenderTargetTexture();
        TArray<uint8> BufferCopy = MoveTemp(Bytes);
        const ERHIFeatureLevel::Type FeatureLevel = GetWorld()->GetFeatureLevel();
        ENQUEUE_RENDER_COMMAND(UpdateCRTTargetSolid)(
            [RHITexture, BufferCopy = MoveTemp(BufferCopy), FeatureLevel](FRHICommandListImmediate& RHICmdList) mutable
            {
                if (FRHITexture2D* Tex2D = RHITexture->GetTexture2D())
                {
                    const uint32 SrcPitch = static_cast<uint32>(SF_OUTPUT_WIDTH) * 4u;
                    FUpdateTextureRegion2D Region(0, 0, 0, 0, static_cast<uint32>(SF_OUTPUT_WIDTH), static_cast<uint32>(SF_OUTPUT_HEIGHT));
                    RHICmdList.UpdateTexture2D(Tex2D, 0, Region, SrcPitch, BufferCopy.GetData());

                    FRDGBuilder GraphBuilder(RHICmdList);
                    FRDGTextureRef RDGTex = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHITexture, TEXT("CRT_RT")));
                    FGenerateMipsParams Params;
                    FGenerateMips::Execute(GraphBuilder, FeatureLevel, RDGTex, Params, EGenerateMipsPass::Compute);
                    GraphBuilder.Execute();
                }
            }
        );
    }
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
    if (ScreenMID.IsValid() && UpscaledRenderTarget)
    {
        ScreenMID->SetTextureParameterValue(TextureParamName, UpscaledRenderTarget);
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

    if (!ScreenMID.IsValid() && UpscaledRenderTarget && Canvas)
	{
		// Draw texture fullscreen
        FCanvasTileItem TileItem(FVector2D(0, 0), UpscaledRenderTarget->GetResource(), FVector2D(Canvas->SizeX, Canvas->SizeY), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(TileItem);
		
        //UE_LOG(LogStarflightHUD, Log, TEXT("Drew RT %dx%d to canvas %dx%d"), UpscaledRenderTarget->SizeX, UpscaledRenderTarget->SizeY, (int)Canvas->SizeX, (int)Canvas->SizeY);
	}
    else if (!ScreenMID.IsValid())
	{
        UE_LOG(LogStarflightHUD, Warning, TEXT("DrawHUD called but UpscaledRenderTarget=%p Canvas=%p"), UpscaledRenderTarget, (void*)Canvas);
	}
}