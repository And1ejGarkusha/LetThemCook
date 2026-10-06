// Copyright Epic Games, Inc. All Rights Reserved.


#include "LetThemCookPlayerController.h"
#include "TabletShopComponent.h"
#include "STabletScreen.h"
#include "TabletDevice.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "LetThemCookCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "LetThemCook.h"
#include "Widgets/Input/SVirtualJoystick.h"

ALetThemCookPlayerController::ALetThemCookPlayerController()
{
	TabletShop = CreateDefaultSubobject<UTabletShopComponent>(TEXT("TabletShop"));
	// set the player camera manager class
	PlayerCameraManagerClass = ALetThemCookCameraManager::StaticClass();
}

void ALetThemCookPlayerController::BeginPlay()
{
	Super::BeginPlay();


	
	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogLetThemCook, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ALetThemCookPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
    if (IsLocalPlayerController() && InputComponent)
    {
        InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ALetThemCookPlayerController::ToggleTablet);
        InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALetThemCookPlayerController::CloseTablet);
    }

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ALetThemCookPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}


void ALetThemCookPlayerController::ToggleTablet()
{
    if (!IsLocalPlayerController()) return;
    if (bTabletOpen)
    {
        CloseTablet();
        return;
    }
    if (!GetPawn() || !GetLocalPlayer() || !TabletShop) return;
    if (!IsValid(TabletDevice))
    {
        FActorSpawnParameters Spawn;
        Spawn.Owner = GetPawn();
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TabletDevice = GetWorld()->SpawnActor<ATabletDevice>(Spawn);
    }
    if (!TabletDevice) return;

    TabletScreen = SNew(STabletScreen)
        .Shop(TabletShop)
        .OnClose(FSimpleDelegate::CreateUObject(this, &ALetThemCookPlayerController::CloseTablet));
    TabletDevice->Equip(this, TabletScreen.ToSharedRef());
    FlushPressedKeys();
    bTabletOpen = true;
    bShowMouseCursor = true;
    SetIgnoreLookInput(true);
    SetIgnoreMoveInput(true);
    // Hardware hit testing forwards mouse events into the world-space display.
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(TabletScreen);
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}

void ALetThemCookPlayerController::CloseTablet()
{
    if (!bTabletOpen) return;
    bTabletOpen = false;
    if (IsValid(TabletDevice)) TabletDevice->Stow();
    TabletScreen.Reset();
    FlushPressedKeys();
    bShowMouseCursor = false;
    SetIgnoreLookInput(false);
    SetIgnoreMoveInput(false);
    SetInputMode(FInputModeGameOnly());
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

bool ALetThemCookPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (bTabletOpen)
    {
        if (Params.Event == IE_Pressed && (Params.Key == EKeys::T || Params.Key == EKeys::Escape))
            CloseTablet();
        // Slate has already handled UI events; block remaining gameplay keys (including jump).
        return true;
    }
    return Super::InputKey(Params);
}

void ALetThemCookPlayerController::OnUnPossess()
{
    CloseTablet();
    if (IsValid(TabletDevice)) TabletDevice->Destroy();
    TabletDevice = nullptr;
    Super::OnUnPossess();
}

void ALetThemCookPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseTablet();
    if (IsValid(TabletDevice)) TabletDevice->Destroy();
    TabletDevice = nullptr;
    Super::EndPlay(EndPlayReason);
}
