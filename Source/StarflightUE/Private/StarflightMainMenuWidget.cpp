// Copyright Epic Games, Inc. All Rights Reserved.

#include "StarflightMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "StarflightPlayerController.h"

UStarflightMainMenuWidget::UStarflightMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentSaveIndex(0)
	, bSaveGamesScanned(false)
	, bGamePaused(false)
{
}

void UStarflightMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button events
	if (StartGameButton)
	{
		StartGameButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnStartGameClicked);
	}

	if (AboutButton)
	{
		AboutButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnAboutClicked);
	}

	if (PauseResumeButton)
	{
		PauseResumeButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnPauseResumeClicked);
	}

	if (PreviousSaveButton)
	{
		PreviousSaveButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnPreviousSaveClicked);
	}

	if (NextSaveButton)
	{
		NextSaveButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnNextSaveClicked);
	}

	if (LoadSaveButton)
	{
		LoadSaveButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnLoadSaveClicked);
	}

	if (ConfirmLoadButton)
	{
		ConfirmLoadButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnConfirmLoadClicked);
	}

	if (CancelLoadButton)
	{
		CancelLoadButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnCancelLoadClicked);
	}

	if (CloseWelcomeButton)
	{
		CloseWelcomeButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnCloseWelcomeClicked);
	}

	if (WelcomeStartGameButton)
	{
		WelcomeStartGameButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnWelcomeStartGameClicked);
	}

	if (PerformanceMetricsButton)
	{
		PerformanceMetricsButton->OnClicked.AddDynamic(this, &UStarflightMainMenuWidget::OnPerformanceMetricsClicked);
	}

	if (RotoscopeCheckBox)
	{
		RotoscopeCheckBox->OnCheckStateChanged.AddDynamic(this, &UStarflightMainMenuWidget::OnRotoscopeCheckChanged);
	}

	if (EGAModeCheckBox)
	{
		EGAModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UStarflightMainMenuWidget::OnEGAModeCheckChanged);
	}

	// Initialize save game list
	RefreshSaveGameList();

	// Hide confirmation and welcome panels initially
	if (LoadConfirmationPanel)
	{
		LoadConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (WelcomePanel)
	{
		WelcomePanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Update display
	UpdateSaveGameDisplay();
}

void UStarflightMainMenuWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ============================================
// Button Callbacks
// ============================================

void UStarflightMainMenuWidget::OnStartGameClicked()
{
	StartNewGame();
}

void UStarflightMainMenuWidget::OnAboutClicked()
{
	ShowWelcomePanel();
}

void UStarflightMainMenuWidget::OnPauseResumeClicked()
{
	TogglePause();
}

void UStarflightMainMenuWidget::OnPreviousSaveClicked()
{
	if (SaveGameList.Num() > 0)
	{
		CurrentSaveIndex = (CurrentSaveIndex - 1 + SaveGameList.Num()) % SaveGameList.Num();
		UpdateSaveGameDisplay();
	}
}

void UStarflightMainMenuWidget::OnNextSaveClicked()
{
	if (SaveGameList.Num() > 0)
	{
		CurrentSaveIndex = (CurrentSaveIndex + 1) % SaveGameList.Num();
		UpdateSaveGameDisplay();
	}
}

void UStarflightMainMenuWidget::OnLoadSaveClicked()
{
	if (SaveGameList.IsValidIndex(CurrentSaveIndex))
	{
		ShowLoadConfirmation();
	}
}

void UStarflightMainMenuWidget::OnConfirmLoadClicked()
{
	LoadSavedGame(CurrentSaveIndex);
	HideLoadConfirmation();
}

void UStarflightMainMenuWidget::OnCancelLoadClicked()
{
	HideLoadConfirmation();
}

void UStarflightMainMenuWidget::OnCloseWelcomeClicked()
{
	HideWelcomePanel();
}

void UStarflightMainMenuWidget::OnWelcomeStartGameClicked()
{
	HideWelcomePanel();
	StartNewGame();
}

void UStarflightMainMenuWidget::OnPerformanceMetricsClicked()
{
	// Toggle performance metrics display
	// This would need to be implemented with the viewport component
	UE_LOG(LogTemp, Log, TEXT("Performance metrics button clicked"));
}

void UStarflightMainMenuWidget::OnRotoscopeCheckChanged(bool bIsChecked)
{
	// Update rotoscope mode in player controller
	AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetRotoscopeMode(bIsChecked);
	}
}

void UStarflightMainMenuWidget::OnEGAModeCheckChanged(bool bIsChecked)
{
	// Update EGA/CGA mode in player controller
	AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetEGAMode(bIsChecked);
	}
}

// ============================================
// Save Game Management
// ============================================

void UStarflightMainMenuWidget::RefreshSaveGameList()
{
	SaveGameList.Empty();
	
	FString SaveDir = GetSaveGamesDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Find all save files (look for .starflight files)
	TArray<FString> FoundFiles;
	IFileManager& FileManager = IFileManager::Get();
	FileManager.FindFiles(FoundFiles, *(SaveDir / TEXT("*.starflight")), true, false);

	// Sort by modification time (newest first)
	FoundFiles.Sort([&PlatformFile, &SaveDir](const FString& A, const FString& B) {
		FString PathA = SaveDir / A;
		FString PathB = SaveDir / B;
		FDateTime TimeA = PlatformFile.GetTimeStamp(*PathA);
		FDateTime TimeB = PlatformFile.GetTimeStamp(*PathB);
		return TimeA > TimeB;
	});

	// Load metadata for each save
	for (const FString& FileName : FoundFiles)
	{
		FString FullPath = SaveDir / FileName;
		FStarflightSaveGameInfo Info;
		Info.SaveFileName = FullPath;
		Info.Timestamp = PlatformFile.GetTimeStamp(*FullPath);

		// Extract PNG screenshot from save file
		TArray<uint8> PNGData = ExtractPNGFromSaveFile(FullPath);
		if (PNGData.Num() > 0)
		{
			Info.Screenshot = CreateTextureFromPNG(PNGData);
		}

		SaveGameList.Add(Info);
	}

	bSaveGamesScanned = true;
	CurrentSaveIndex = 0;
}

FString UStarflightMainMenuWidget::GetSaveGamesDirectory() const
{
	// Use the project's Saved/SaveGames directory
	return FPaths::ProjectSavedDir() / TEXT("SaveGames");
}

void UStarflightMainMenuWidget::UpdateSaveGameDisplay()
{
	if (SaveGameList.Num() == 0)
	{
		// No saves available
		if (NoSavesText)
		{
			NoSavesText->SetVisibility(ESlateVisibility::Visible);
		}
		if (SaveGameScreenshot)
		{
			SaveGameScreenshot->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (SaveGameTimestamp)
		{
			SaveGameTimestamp->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (LoadSaveButton)
		{
			LoadSaveButton->SetIsEnabled(false);
		}
		if (PreviousSaveButton)
		{
			PreviousSaveButton->SetIsEnabled(false);
		}
		if (NextSaveButton)
		{
			NextSaveButton->SetIsEnabled(false);
		}
	}
	else
	{
		// Display current save
		if (NoSavesText)
		{
			NoSavesText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (SaveGameScreenshot)
		{
			SaveGameScreenshot->SetVisibility(ESlateVisibility::Visible);
		}
		if (SaveGameTimestamp)
		{
			SaveGameTimestamp->SetVisibility(ESlateVisibility::Visible);
		}
		if (LoadSaveButton)
		{
			LoadSaveButton->SetIsEnabled(true);
		}
		if (PreviousSaveButton)
		{
			PreviousSaveButton->SetIsEnabled(SaveGameList.Num() > 1);
		}
		if (NextSaveButton)
		{
			NextSaveButton->SetIsEnabled(SaveGameList.Num() > 1);
		}

		if (SaveGameList.IsValidIndex(CurrentSaveIndex))
		{
			const FStarflightSaveGameInfo& CurrentSave = SaveGameList[CurrentSaveIndex];

			// Update screenshot
			if (SaveGameScreenshot && CurrentSave.Screenshot)
			{
				SaveGameScreenshot->SetBrushFromTexture(CurrentSave.Screenshot);
			}

			// Update timestamp
			if (SaveGameTimestamp)
			{
				FString TimeStr = CurrentSave.Timestamp.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
				SaveGameTimestamp->SetText(FText::FromString(TimeStr));
			}
		}
	}
}

void UStarflightMainMenuWidget::ShowLoadConfirmation()
{
	if (LoadConfirmationPanel && SaveGameList.IsValidIndex(CurrentSaveIndex))
	{
		const FStarflightSaveGameInfo& CurrentSave = SaveGameList[CurrentSaveIndex];

		// Show large screenshot
		if (ConfirmationScreenshot && CurrentSave.Screenshot)
		{
			ConfirmationScreenshot->SetBrushFromTexture(CurrentSave.Screenshot);
		}

		// Show timestamp
		if (ConfirmationTimestamp)
		{
			FString TimeStr = CurrentSave.Timestamp.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
			ConfirmationTimestamp->SetText(FText::FromString(TimeStr));
		}

		LoadConfirmationPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UStarflightMainMenuWidget::HideLoadConfirmation()
{
	if (LoadConfirmationPanel)
	{
		LoadConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UStarflightMainMenuWidget::ShowWelcomePanel()
{
	if (WelcomePanel)
	{
		WelcomePanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UStarflightMainMenuWidget::HideWelcomePanel()
{
	if (WelcomePanel)
	{
		WelcomePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ============================================
// Game Control
// ============================================

void UStarflightMainMenuWidget::StartNewGame()
{
	AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->StartStarflightGame(FString());
		OnGameStarted();

		// Hide this menu
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UStarflightMainMenuWidget::LoadSavedGame(int32 SaveIndex)
{
	if (SaveGameList.IsValidIndex(SaveIndex))
	{
		AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
		if (PC)
		{
			const FStarflightSaveGameInfo& SaveInfo = SaveGameList[SaveIndex];
			PC->StartStarflightGame(SaveInfo.SaveFileName);
			OnGameLoaded();

			// Hide this menu
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UStarflightMainMenuWidget::TogglePause()
{
	bGamePaused = !bGamePaused;

	AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetGamePaused(bGamePaused);
	}

	// Update button text
	if (PauseResumeText)
	{
		PauseResumeText->SetText(FText::FromString(bGamePaused ? TEXT("Resume Game") : TEXT("Pause Game")));
	}

	OnPauseStateChanged(bGamePaused);
}

bool UStarflightMainMenuWidget::IsGameRunning() const
{
	AStarflightPlayerController* PC = Cast<AStarflightPlayerController>(GetOwningPlayer());
	if (PC)
	{
		return PC->IsGameRunning();
	}
	return false;
}

bool UStarflightMainMenuWidget::IsGamePaused() const
{
	return bGamePaused;
}

// ============================================
// Helper Functions
// ============================================

UTexture2D* UStarflightMainMenuWidget::CreateTextureFromPNG(const TArray<uint8>& PNGData)
{
	if (PNGData.Num() == 0)
	{
		return nullptr;
	}

	// Load the image wrapper module
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(PNGData.GetData(), PNGData.Num()))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to decompress PNG data"));
		return nullptr;
	}

	// Get the raw image data
	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get raw image data"));
		return nullptr;
	}

	// Create texture
	int32 Width = ImageWrapper->GetWidth();
	int32 Height = ImageWrapper->GetHeight();

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create texture"));
		return nullptr;
	}

	// Copy data to texture
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

	Texture->UpdateResource();

	return Texture;
}

TArray<uint8> UStarflightMainMenuWidget::ExtractPNGFromSaveFile(const FString& SaveFilePath)
{
	TArray<uint8> Result;

	// Load the entire save file
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *SaveFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load save file: %s"), *SaveFilePath);
		return Result;
	}

	// Look for PNG signature (89 50 4E 47 0D 0A 1A 0A)
	const uint8 PNGSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	int32 PNGStart = -1;

	for (int32 i = 0; i < FileData.Num() - 8; ++i)
	{
		bool bMatch = true;
		for (int32 j = 0; j < 8; ++j)
		{
			if (FileData[i + j] != PNGSignature[j])
			{
				bMatch = false;
				break;
			}
		}

		if (bMatch)
		{
			PNGStart = i;
			break;
		}
	}

	if (PNGStart == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("No PNG found in save file: %s"), *SaveFilePath);
		return Result;
	}

	// Look for PNG end signature (IEND chunk)
	const uint8 IENDSignature[] = { 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };
	int32 PNGEnd = -1;

	for (int32 i = PNGStart; i < FileData.Num() - 8; ++i)
	{
		bool bMatch = true;
		for (int32 j = 0; j < 8; ++j)
		{
			if (FileData[i + j] != IENDSignature[j])
			{
				bMatch = false;
				break;
			}
		}

		if (bMatch)
		{
			PNGEnd = i + 8; // Include the IEND signature
			break;
		}
	}

	if (PNGEnd == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Incomplete PNG in save file: %s"), *SaveFilePath);
		return Result;
	}

	// Extract PNG data
	int32 PNGSize = PNGEnd - PNGStart;
	Result.SetNum(PNGSize);
	FMemory::Memcpy(Result.GetData(), &FileData[PNGStart], PNGSize);

	return Result;
}

