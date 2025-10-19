#include "StarflightInputComponent.h"
#include "StarflightInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightInputComponent, Log, All);

UStarflightInputComponent::UStarflightInputComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UStarflightInputComponent::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogStarflightInputComponent, Error, TEXT("StarflightInputComponent must be attached to a Pawn"));
        return;
    }
    
    SetInputEnabled(true);
}

void UStarflightInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetInputEnabled(false);
    Super::EndPlay(EndPlayReason);
}

void UStarflightInputComponent::SetInputEnabled(bool bEnabled)
{
    if (!OwnerPawn)
    {
        return;
    }

    if (bEnabled)
    {
        if (!BoundInputComponent)
        {
            // Ensure the pawn is receiving input
            APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
            if (!PC)
            {
                PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            }
            if (PC)
            {
                OwnerPawn->EnableInput(PC);
            }

            // Create our own input component so bindings always exist
            BoundInputComponent = NewObject<UInputComponent>(OwnerPawn, TEXT("StarflightInputComponent_Input"));
            BoundInputComponent->RegisterComponent();
            BoundInputComponent->bBlockInput = false;
            // Push onto the PlayerController input stack so it actually receives key events
            if (PC)
            {
                PC->PushInputComponent(BoundInputComponent);
            }
            
            // Bind all possible keys
            // Note: Unreal doesn't have a simple "bind all keys" mechanism, so we bind common keys
            // For a more complete solution, you'd want to use Enhanced Input system
            
            TArray<FKey> KeysToBind = {
                // Letters
                EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G, EKeys::H,
                EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M, EKeys::N, EKeys::O, EKeys::P,
                EKeys::Q, EKeys::R, EKeys::S, EKeys::T, EKeys::U, EKeys::V, EKeys::W, EKeys::X,
                EKeys::Y, EKeys::Z,
                
                // Numbers
                EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
                EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine,
                
                // Arrows
                EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right,
                
                // Numpad
                EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
                EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven,
                EKeys::NumPadEight, EKeys::NumPadNine,
                
                // Function keys
                EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5,
                EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10,
                
                // Special keys
                EKeys::Enter, EKeys::Escape, EKeys::BackSpace, EKeys::Tab, EKeys::SpaceBar,
                EKeys::Home, EKeys::End, EKeys::PageUp, EKeys::PageDown,
                EKeys::Insert, EKeys::Delete,
                
                // Punctuation
                EKeys::Comma, EKeys::Period, EKeys::Slash, EKeys::Semicolon,
                EKeys::Apostrophe, EKeys::LeftBracket, EKeys::RightBracket,
                EKeys::Backslash, EKeys::Hyphen, EKeys::Equals, EKeys::Tilde,
                
                // Modifiers (for tracking state)
                EKeys::LeftShift, EKeys::RightShift,
                EKeys::LeftControl, EKeys::RightControl,
                EKeys::LeftAlt, EKeys::RightAlt,
            };
            
            for (const FKey& Key : KeysToBind)
            {
                FInputKeyBinding PressedBinding(Key, IE_Pressed);
                PressedBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Key]()
                {
                    OnAnyKeyPressed(Key);
                });
                BoundInputComponent->KeyBindings.Add(PressedBinding);
                
                FInputKeyBinding ReleasedBinding(Key, IE_Released);
                ReleasedBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Key]()
                {
                    OnAnyKeyReleased(Key);
                });
                BoundInputComponent->KeyBindings.Add(ReleasedBinding);
            }
            
            UE_LOG(LogStarflightInputComponent, Log, TEXT("Input enabled and bound %d keys"), KeysToBind.Num());
        }
    }
    else
    {
        // Unbind input
        if (BoundInputComponent)
        {
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                PC->PopInputComponent(BoundInputComponent);
            }
            BoundInputComponent->ClearActionBindings();
            BoundInputComponent->KeyBindings.Empty();
            BoundInputComponent->DestroyComponent();
            BoundInputComponent = nullptr;

            UE_LOG(LogStarflightInputComponent, Log, TEXT("Input disabled"));
        }
    }
}

void UStarflightInputComponent::OnAnyKeyPressed(FKey Key)
{
    // Track modifier state
    if (Key == EKeys::LeftShift || Key == EKeys::RightShift)
    {
        bShiftDown = true;
        return; // Don't send modifiers as keys
    }
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
    {
        bCtrlDown = true;
        return;
    }
    if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
    {
        bAltDown = true;
        return;
    }
    
    // Forward to emulator
    FStarflightInput::PushKey(Key, bShiftDown, bCtrlDown, bAltDown);
}

void UStarflightInputComponent::OnAnyKeyReleased(FKey Key)
{
    // Track modifier state
    if (Key == EKeys::LeftShift || Key == EKeys::RightShift)
    {
        bShiftDown = false;
    }
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
    {
        bCtrlDown = false;
    }
    if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
    {
        bAltDown = false;
    }
    
    // Note: DOS keyboard doesn't typically send key release events
    // The emulator only uses key press events
}

