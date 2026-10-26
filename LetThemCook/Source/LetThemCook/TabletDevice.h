#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "TabletDevice.generated.h"

class UStaticMeshComponent;
class STabletScreen;
class APlayerController;
class UPoseableMeshComponent;
class USkeletalMeshComponent;

/** World-space Slate display with ordinary mouse hit testing. */
UCLASS()
class LETTHEMCOOK_API UTabletDisplayComponent : public UWidgetComponent
{
    GENERATED_BODY()
public:
    UTabletDisplayComponent();
};

/** Local camera-facing tablet. Cube is a replaceable placeholder, not a HUD overlay. */
UCLASS(NotPlaceable, Transient)
class LETTHEMCOOK_API ATabletDevice : public AActor
{
    GENERATED_BODY()
public:
    ATabletDevice();
    void Equip(APlayerController* Controller, const TSharedRef<STabletScreen>& Screen);
    void Stow();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    static FVector2D CalculateScreenSize(float Distance, float HorizontalFov, float AspectRatio);

    UPROPERTY(VisibleAnywhere, Category="Tablet") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, Category="Tablet") TObjectPtr<UTabletDisplayComponent> Display;
    UPROPERTY(VisibleAnywhere, Category="Tablet") TObjectPtr<UPoseableMeshComponent> Hands;

private:
    TWeakObjectPtr<APlayerController> ViewingController;
    TSharedPtr<STabletScreen> CurrentScreen;
    float EquipProgress = 0.0f;
    bool bEquipping = false;
    FIntPoint CachedDrawSize = FIntPoint::ZeroValue;
    TWeakObjectPtr<USkeletalMeshComponent> CharacterHands;
    bool bCharacterHandsWereVisible = true;
    uint8 OriginalAnimTickOption = 0;
    TArray<FTransform> ReferenceHandPose;
    FVector2D CachedGripSize = FVector2D::ZeroVector;
    bool bSimpleHandSkeleton = false;
    void PrepareHands();
    void PoseHands(const FVector2D& ScreenSize);
    void RestoreCharacterHands();
    void UpdatePose();
};
