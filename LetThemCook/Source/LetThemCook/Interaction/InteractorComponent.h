#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractorComponent.generated.h"

class UInteractableComponent;
class UGrabbableComponent;
class UPhysicsHandleComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionFocusChanged, UInteractableComponent*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeldItemChanged, UGrabbableComponent*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionRejected, const FText&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionPromptChanged, const FText&, Prompt);

UCLASS(Blueprintable, ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UInteractorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInteractorComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FComponentReference ViewComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="1", Units="cm"))
    float InteractionDistance = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Grab", meta=(ClampMin="30", Units="cm"))
    float HoldDistance = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Grab", meta=(ClampMin="50", Units="cm"))
    float BreakDistance = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Throw", meta=(ClampMin="0.05", Units="s"))
    float HoldThreshold = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Throw", meta=(ClampMin="0.05", Units="s"))
    float FullChargeTime = 1.f;

    /** Throw strength is velocity change (cm/s), independent of mass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Throw", meta=(ClampMin="0"))
    float MinimumThrowSpeed = 400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Throw", meta=(ClampMin="0"))
    float MaximumThrowSpeed = 1400.f;

    UPROPERTY(BlueprintAssignable, Category="Interaction") FInteractionFocusChanged OnFocusChanged;
    UPROPERTY(BlueprintAssignable, Category="Interaction") FHeldItemChanged OnHeldItemChanged;
    UPROPERTY(BlueprintAssignable, Category="Interaction") FInteractionRejected OnRequestRejected;
    UPROPERTY(BlueprintAssignable, Category="Interaction") FInteractionPromptChanged OnPromptChanged;

    UFUNCTION(BlueprintCallable, Category="Interaction") void BeginInteraction();
    UFUNCTION(BlueprintCallable, Category="Interaction") void EndInteraction();
    UFUNCTION(BlueprintCallable, Category="Interaction") void CancelInteraction();
    UFUNCTION(BlueprintPure, Category="Interaction") UInteractableComponent* GetFocusedTarget() const;
    UFUNCTION(BlueprintPure, Category="Interaction") UGrabbableComponent* GetHeldItem() const { return HeldItem; }
    UFUNCTION(BlueprintPure, Category="Interaction") FText GetInteractionPrompt() const;
    UFUNCTION(BlueprintPure, Category="Interaction") float GetThrowCharge() const;
    UFUNCTION(BlueprintPure, Category="Interaction") bool IsChargingThrow() const;
    UFUNCTION(BlueprintPure, Category="Interaction") void GetInteractionView(FVector& Location, FRotator& Rotation) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction") bool TryGrab(UGrabbableComponent* Item);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction") void ReleaseHeldItem();

    static float CalculateCharge(float Duration, float Threshold, float ChargeTime);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(ReplicatedUsing=OnRep_HeldItem) TObjectPtr<UGrabbableComponent> HeldItem;
    UPROPERTY(Transient) TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;
    UPROPERTY(Transient) TObjectPtr<UPhysicsConstraintComponent> CollisionExclusion;
    TWeakObjectPtr<UGrabbableComponent> CollisionIgnoredItem;
    TWeakObjectPtr<UInteractableComponent> FocusedTarget;
    TWeakObjectPtr<UInteractableComponent> PressedTarget;
    TWeakObjectPtr<UGrabbableComponent> PressedHeldItem;
    FQuat GrabRotationOffset = FQuat::Identity;
    FText PreviousPrompt;
    bool bExtractingItem = false;
    bool bReleasePending = false;
    float PendingThrowSpeed = 0.f;
    bool bLocalPressed = false;
    bool bServerPressed = false;
    bool bWasAlreadyMoveIgnored = false;
    double LocalPressTime = 0.;
    double ServerPressTime = 0.;
    double LastServerPressTime = -100.;

    bool IsLocallyControlled() const;
    UInteractableComponent* TraceTarget(FHitResult* OutHit = nullptr) const;
    void UpdateCollisionExclusion();
    FVector GetSafeHoldLocation(const UPrimitiveComponent* Body, const FVector& Location, const FQuat& Rotation) const;
    bool IsClearOfHolder(const UPrimitiveComponent* Body) const;
    void ReleaseInternal(float ThrowSpeed, bool bForce = false);
    UFUNCTION() void OnRep_HeldItem();
    UFUNCTION(Server, Reliable) void ServerBeginInteraction(UInteractableComponent* Candidate);
    UFUNCTION(Server, Reliable) void ServerEndInteraction();
    UFUNCTION(Server, Reliable) void ServerCancelInteraction();
    UFUNCTION(Client, Reliable) void ClientReject(const FText& Reason);
};
