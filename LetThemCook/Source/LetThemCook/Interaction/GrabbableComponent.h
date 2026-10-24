#pragma once

#include "CoreMinimal.h"
#include "InteractableComponent.h"
#include "GrabbableComponent.generated.h"

class UPrimitiveComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrabStateChanged, UInteractorComponent*, Interactor);

UCLASS(Blueprintable, ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UGrabbableComponent : public UInteractableComponent
{
    GENERATED_BODY()
public:
    UGrabbableComponent();

    /** Leave empty to use the actor's primitive root. The body must be the root for movement replication. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Grab")
    FComponentReference PhysicsBody;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Grab", meta=(ClampMin="0.1", Units="kg"))
    float MaximumMass = 50.f;

    /** Walking into a small item should push it rather than automatically climb it.
     *  This does not prevent landing or standing on the item after a jump. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Physics")
    bool bAllowCharacterStepUp = false;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Holder, Category="Interaction|Grab")
    TObjectPtr<UInteractorComponent> Holder;

    UPROPERTY(BlueprintAssignable, Category="Interaction|Grab")
    FGrabStateChanged OnGrabbed;

    UPROPERTY(BlueprintAssignable, Category="Interaction|Grab")
    FGrabStateChanged OnReleased;

    UFUNCTION(BlueprintPure, Category="Interaction|Grab")
    UPrimitiveComponent* GetPhysicsBody() const;

    UFUNCTION(BlueprintPure, Category="Interaction|Grab")
    bool IsPhysicsSetupValid() const;

    virtual bool CanInteract_Implementation(UInteractorComponent* Interactor) const override;
    virtual void Interact_Implementation(UInteractorComponent* Interactor) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    friend class UInteractorComponent;
    void SetHolder(UInteractorComponent* NewHolder);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION() void OnRep_Holder(UInteractorComponent* PreviousHolder);
};
