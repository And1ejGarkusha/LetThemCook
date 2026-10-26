// Copyright Epic Games, Inc. All Rights Reserved.


#include "LetThemCookPlayerController.h"
#include "TabletShopComponent.h"
#include "TabletOrdersComponent.h"
#include "CustomerServicePoint.h"
#include "CustomerNPC.h"
#include "CustomerMealComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
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
    TabletOrders = CreateDefaultSubobject<UTabletOrdersComponent>(TEXT("TabletOrders"));
    ServeCustomerAction = CreateDefaultSubobject<UInputAction>(TEXT("ServeCustomerAction"));
    ServeCustomerAction->ValueType = EInputActionValueType::Boolean;
    CustomerInputContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("CustomerInputContext"));
    CustomerInputContext->MapKey(ServeCustomerAction, EKeys::RightMouseButton);
	// set the player camera manager class
	PlayerCameraManagerClass = ALetThemCookCameraManager::StaticClass();
}

void ALetThemCookPlayerController::ServeCustomer(ACustomerServicePoint* Counter, AActor* Food)
{
    ServerServeCustomer(Counter, Food);
}

void ALetThemCookPlayerController::ServerServeCustomer_Implementation(ACustomerServicePoint* Counter, AActor* Food)
{
    if (IsValid(Counter) && IsValid(Food) && Food == GetHeldCustomerFood()) Counter->TryServeMeal(this, Food);
}

AActor* ALetThemCookPlayerController::GetHeldCustomerFood() const
{
    const APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn)) return nullptr;
    TArray<UPhysicsHandleComponent*> Handles;
    ControlledPawn->GetComponents(Handles);
    // Gleb_Interaction uses this engine component on the server to hold physical items.
    for (const auto* Handle : Handles)
        if (const auto* Body = Handle->GetGrabbedComponent())
            if (auto* Food = Body->GetOwner())
                if (const auto* Meal = Food->FindComponentByClass<UCustomerMealComponent>(); Meal && Meal->IsReady()) return Food;
    // Also support games that attach the carried tray directly to the pawn/hand socket.
    TArray<AActor*> Attached;
    ControlledPawn->GetAttachedActors(Attached, true, true);
    for (auto* Food : Attached)
        if (IsValid(Food))
            if (const auto* Meal = Food->FindComponentByClass<UCustomerMealComponent>(); Meal && Meal->IsReady()) return Food;
    return nullptr;
}

void ALetThemCookPlayerController::OnServeCustomerPressed()
{
    if (IsLocalPlayerController() && !bTabletOpen && GetPawn()) ServerServeHeldCustomerFood();
}

void ALetThemCookPlayerController::ServerServeHeldCustomerFood_Implementation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (LastCustomerServeTime >= 0 && Now - LastCustomerServeTime < .15) return;
    LastCustomerServeTime = Now;
    auto* Food = GetHeldCustomerFood();
    if (!Food) return;
    FVector Eyes; FRotator View;
    GetPawn()->GetActorEyesViewPoint(Eyes, View);
    ACustomerServicePoint* Best = nullptr;
    float BestScore = -1;
    for (TActorIterator<ACustomerServicePoint> It(GetWorld()); It; ++It)
    {
        auto* Counter = *It;
        auto* Customer = Counter->GetCurrentCustomer();
        if (!Counter->CanPlayerServe(this) || !IsValid(Customer) || Customer->State != ECustomerState::WaitingForFood ||
            FVector::DistSquared(GetPawn()->GetActorLocation(), Counter->GetQueueLocation(0)) > FMath::Square(FMath::Clamp(Counter->ServingDistance, 50.f, 500.f))) continue;
        const FVector Target = Customer->GetActorLocation() + FVector(0,0,20);
        const FVector Offset = Target - Eyes;
        const float Aim = FVector::DotProduct(View.Vector(), Offset.GetSafeNormal());
        if (Aim < .92f) continue;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CustomerServeAim), false, GetPawn());
        Params.AddIgnoredActor(Food);
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Target, ECC_Visibility, Params) && Hit.GetActor() != Customer) continue;
        const float Score = Aim - Offset.Size() * .0001f;
        if (Score > BestScore) { Best = Counter; BestScore = Score; }
    }
    if (Best) Best->TryServeMeal(this, Food);
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
    if (IsLocalPlayerController())
        if (auto* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
            Enhanced->BindAction(ServeCustomerAction, ETriggerEvent::Started, this, &ALetThemCookPlayerController::OnServeCustomerPressed);
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
			Subsystem->AddMappingContext(CustomerInputContext, 0);
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
        .Orders(TabletOrders)
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

void ALetThemCookPlayerController::TabletDebugOrder(FString Preset, float Seconds)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    Preset = Preset.ToLower();
    if ((Preset != TEXT("burger") && Preset != TEXT("custom") && Preset != TEXT("fries") && Preset != TEXT("drink") && Preset != TEXT("combo")) ||
        !FMath::IsFinite(Seconds) || Seconds <= 0 || Seconds > 180)
    {
        ClientMessage(TEXT("Usage: TabletDebugOrder [burger|custom|fries|drink|combo] [seconds: 1..180]"));
        return;
    }
    ServerTabletDebugOrder(Preset, Seconds);
    if (IsLocalController())
    {
        if (!bTabletOpen) ToggleTablet();
        if (TabletScreen.IsValid()) TabletScreen->ShowOrders();
    }
#endif
}

void ALetThemCookPlayerController::ServerTabletDebugOrder_Implementation(const FString& Preset, float Seconds)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!TabletOrders || !FMath::IsFinite(Seconds) || Seconds <= 0 || Seconds > 180) return;
    ETabletDish Dish = ETabletDish::Burger;
    FTabletBurgerComposition Burger;
    int32 Payment = 160;
    if (Preset == TEXT("custom")) { Burger.bCheese = false; Burger.bPickle = false; Payment = 125; }
    else if (Preset == TEXT("fries")) { Dish = ETabletDish::Fries; Payment = 70; }
    else if (Preset == TEXT("drink")) { Dish = ETabletDish::Drink; Payment = 50; }
    else if (Preset != TEXT("burger") && Preset != TEXT("combo")) return;

    FGuid Id;
    if (Preset == TEXT("combo"))
    {
        TArray<FTabletDishItem> Items;
        for (ETabletDish MealDish : {ETabletDish::Burger, ETabletDish::Fries, ETabletDish::Drink})
        {
            FTabletDishItem Item;
            Item.Dish = MealDish;
            Items.Add(Item);
        }
        Id = TabletOrders->SubmitMealOrder(Items, 280, Seconds);
    }
    else Id = TabletOrders->SubmitOrder(Dish, Burger, Payment, Seconds);
    if (const FTabletCustomerOrder* Order = TabletOrders->FindOrder(Id))
    {
        const FString Message = FString::Printf(TEXT("Tablet debug order %03d added: %s, %.0f seconds"), Order->Number, *Preset, Seconds);
        UE_LOG(LogLetThemCook, Display, TEXT("%s"), *Message);
        ClientMessage(Message);
    }
    else ClientMessage(TEXT("Could not add debug order: inbox is not ready or its 32-order limit has been reached."));
#endif
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
    if (const auto* Local = GetLocalPlayer())
        if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local))
            Subsystem->RemoveMappingContext(CustomerInputContext);
    CloseTablet();
    if (IsValid(TabletDevice)) TabletDevice->Destroy();
    TabletDevice = nullptr;
    Super::EndPlay(EndPlayReason);
}
