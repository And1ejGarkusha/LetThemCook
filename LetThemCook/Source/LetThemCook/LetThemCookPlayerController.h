// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LetThemCookPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UTabletShopComponent;
class STabletScreen;
class ATabletDevice;

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

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnUnPossess() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tablet")
    TObjectPtr<UTabletShopComponent> TabletShop;

    TSharedPtr<STabletScreen> TabletScreen;
    UPROPERTY(Transient)
    TObjectPtr<ATabletDevice> TabletDevice;
    bool bTabletOpen = false;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

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
};
