// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "LetThemCookPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UTabletShopComponent;
class UTabletOrdersComponent;
class ACustomerServicePoint;
class STabletScreen;
class ATabletDevice;
class UInputAction;
struct FGeometry;
struct FKeyEvent;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class LETTHEMCOOK_API ALetThemCookPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ALetThemCookPlayerController();

public:
    UFUNCTION(Exec)
    void ToggleTablet();
    void CloseTablet();

    UFUNCTION(BlueprintPure, Category="Tablet")
    bool IsTabletOpen() const { return bTabletOpen; }

    UFUNCTION(BlueprintPure, Category="Tablet|Orders")
    UTabletOrdersComponent* GetTabletOrders() const { return TabletOrders; }

    /** Interaction gameplay calls this with the held cooked-food actor. Server rechecks distance and contents. */
    UFUNCTION(BlueprintCallable, Category="Customers")
    void ServeCustomer(ACustomerServicePoint* Counter, AActor* Food);

    /** Console: TabletDebugOrder [burger|custom|fries|drink|combo] [seconds]. Development builds only. */
    UFUNCTION(Exec)
    void TabletDebugOrder(FString Preset = TEXT("burger"), float Seconds = 180.f);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input|Customers")
    TObjectPtr<UInputAction> ServeCustomerAction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input|Customers")
    TObjectPtr<UInputMappingContext> CustomerInputContext;
    void OnServeCustomerPressed();
    AActor* GetHeldCustomerFood() const;
    UFUNCTION(Server, Reliable)
    void ServerServeHeldCustomerFood();
    double LastCustomerServeTime = -1;
    UFUNCTION(Server, Reliable)
    void ServerServeCustomer(ACustomerServicePoint* Counter, AActor* Food);
    UFUNCTION(Server, Reliable)
    void ServerTabletDebugOrder(const FString& Preset, float Seconds);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnUnPossess() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tablet")
    TObjectPtr<UTabletShopComponent> TabletShop;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tablet")
    TObjectPtr<UTabletOrdersComponent> TabletOrders;

    TSharedPtr<STabletScreen> TabletScreen;
    UPROPERTY(Transient)
    TObjectPtr<ATabletDevice> TabletDevice;
    bool bTabletOpen = false;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

    UPROPERTY(EditDefaultsOnly, Category="Input|Tablet")
    TObjectPtr<UInputAction> ToggleTabletAction;

    UPROPERTY(EditDefaultsOnly, Category="Input|Tablet")
    TObjectPtr<UInputAction> CloseTabletAction;

    UPROPERTY(EditDefaultsOnly, Category="Input|Tablet")
    TObjectPtr<UInputMappingContext> TabletMappingContext;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Optional local feedback; can be replaced with the project's own widget. */
	UPROPERTY(EditDefaultsOnly, Category="Interaction|UI")
	bool bShowInteractionFeedback = true;

	UPROPERTY(EditDefaultsOnly, Category="Interaction|UI")
	TSubclassOf<UUserWidget> InteractionFeedbackWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> InteractionFeedbackWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
    bool IsTabletCommandKey(FKey Key) const;
    FReply ForwardTabletKey(const FKeyEvent& Event, EInputEvent InputEvent);
    FReply HandleTabletKeyDown(const FGeometry& Geometry, const FKeyEvent& Event);
    FReply HandleTabletKeyUp(const FGeometry& Geometry, const FKeyEvent& Event);
    FText GetTabletCloseHint() const;
};
