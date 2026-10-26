// Copyright Epic Games, Inc. All Rights Reserved.


#include "LetThemCookPlayerController.h"
#include "TabletShopComponent.h"
#include "STabletScreen.h"
#include "TabletDevice.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "InputMappingContext.h"
#include "LetThemCookCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "LetThemCook.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Widgets/SViewport.h"
#include "Interaction/UI/InteractionPromptWidget.h"
#include "UObject/ConstructorHelpers.h"

ALetThemCookPlayerController::ALetThemCookPlayerController()
{
	TabletShop = CreateDefaultSubobject<UTabletShopComponent>(TEXT("TabletShop"));
	// set the player camera manager class
	PlayerCameraManagerClass = ALetThemCookCameraManager::StaticClass();
	InteractionFeedbackWidgetClass = UInteractionPromptWidget::StaticClass();
    static ConstructorHelpers::FObjectFinder<UInputAction> ToggleAction(TEXT("/Game/Input/Actions/IA_ToggleTablet.IA_ToggleTablet"));
    static ConstructorHelpers::FObjectFinder<UInputAction> CloseAction(TEXT("/Game/Input/Actions/IA_CloseTablet.IA_CloseTablet"));
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> TabletContext(TEXT("/Game/Input/IMC_Tablet.IMC_Tablet"));
    ToggleTabletAction = ToggleAction.Object;
    CloseTabletAction = CloseAction.Object;
    TabletMappingContext = TabletContext.Object;
}

void ALetThemCookPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && bShowInteractionFeedback && InteractionFeedbackWidgetClass)
	{
		InteractionFeedbackWidget = CreateWidget<UUserWidget>(this, InteractionFeedbackWidgetClass);
		if (InteractionFeedbackWidget)
		{
			InteractionFeedbackWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			InteractionFeedbackWidget->AddToPlayerScreen(20);
		}
	}


	
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
    if (IsLocalPlayerController())
    {
        if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
        {
            if (ToggleTabletAction) EnhancedInput->BindAction(ToggleTabletAction, ETriggerEvent::Started, this, &ALetThemCookPlayerController::ToggleTablet);
            if (CloseTabletAction) EnhancedInput->BindAction(CloseTabletAction, ETriggerEvent::Started, this, &ALetThemCookPlayerController::CloseTablet);
        }
    }

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (TabletMappingContext) Subsystem->AddMappingContext(TabletMappingContext, 1);
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
        .OnCommandKeyDown(FOnKeyDown::CreateUObject(this, &ALetThemCookPlayerController::HandleTabletKeyDown))
        .OnCommandKeyUp(FOnKeyDown::CreateUObject(this, &ALetThemCookPlayerController::HandleTabletKeyUp))
        .CloseHint_UObject(this, &ALetThemCookPlayerController::GetTabletCloseHint)
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
    if (FSlateApplication::IsInitialized() && GetLocalPlayer() && GetLocalPlayer()->ViewportClient)
    {
        const TSharedPtr<FSlateUser> User = GetLocalPlayer()->GetSlateUser();
        const TSharedPtr<SViewport> Viewport = GetLocalPlayer()->ViewportClient->GetGameViewportWidget();
        if (User.IsValid() && Viewport.IsValid())
            FSlateApplication::Get().SetUserFocus(User->GetUserIndex(), Viewport, EFocusCause::SetDirectly);
    }
}

bool ALetThemCookPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Event == IE_Repeat && IsTabletCommandKey(Params.Key)) return true;
    if (bTabletOpen && !IsTabletCommandKey(Params.Key)) return true;
    return Super::InputKey(Params);
}

bool ALetThemCookPlayerController::IsTabletCommandKey(FKey Key) const
{
    const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
    return Subsystem && ((ToggleTabletAction && Subsystem->QueryKeysMappedToAction(ToggleTabletAction).Contains(Key))
        || (CloseTabletAction && Subsystem->QueryKeysMappedToAction(CloseTabletAction).Contains(Key)));
}

FReply ALetThemCookPlayerController::ForwardTabletKey(const FKeyEvent& Event, EInputEvent InputEvent)
{
    if (!bTabletOpen || !IsTabletCommandKey(Event.GetKey())) return FReply::Unhandled();
    const ULocalPlayer* LocalPlayer = GetLocalPlayer();
    FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
    InputKey(FInputKeyEventArgs(Viewport, Event.GetInputDeviceId(), Event.GetKey(), InputEvent,
        InputEvent == IE_Released ? 0.f : 1.f, false, Event.GetEventTimestamp()));
    return FReply::Handled();
}

FReply ALetThemCookPlayerController::HandleTabletKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    return ForwardTabletKey(Event, Event.IsRepeat() ? IE_Repeat : IE_Pressed);
}

FReply ALetThemCookPlayerController::HandleTabletKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    return ForwardTabletKey(Event, IE_Released);
}

FText ALetThemCookPlayerController::GetTabletCloseHint() const
{
    TArray<FKey> Keys;
    if (const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (ToggleTabletAction) Keys = Subsystem->QueryKeysMappedToAction(ToggleTabletAction);
        if (CloseTabletAction)
            for (const FKey& Key : Subsystem->QueryKeysMappedToAction(CloseTabletAction)) Keys.AddUnique(Key);
    }

    TArray<FString> Names;
    for (const FKey& Key : Keys) Names.Add(Key.GetDisplayName().ToString());
    return Names.IsEmpty() ? NSLOCTEXT("Tablet", "Close", "Закрыть")
        : FText::Format(NSLOCTEXT("Tablet", "CloseHint", "Закрыть [{0}]"), FText::FromString(FString::Join(Names, TEXT(" / "))));
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
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (TabletMappingContext) Subsystem->RemoveMappingContext(TabletMappingContext);
    }
    if (InteractionFeedbackWidget)
    {
        InteractionFeedbackWidget->RemoveFromParent();
        InteractionFeedbackWidget = nullptr;
    }
    CloseTablet();
    if (IsValid(TabletDevice)) TabletDevice->Destroy();
    TabletDevice = nullptr;
    Super::EndPlay(EndPlayReason);
}
