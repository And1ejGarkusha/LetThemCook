#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

class UInteractorComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionExecuted, UInteractorComponent*, Interactor);

/** Add one Interactable OR Grabbable to an actor. Gameplay events run on the server. */
UCLASS(Blueprintable, ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UInteractableComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInteractableComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FText ActionText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Enabled, Category="Interaction")
    bool bInteractionEnabled = true;

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FInteractionExecuted OnInteracted;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Interaction")
    bool CanInteract(UInteractorComponent* Interactor) const;
    virtual bool CanInteract_Implementation(UInteractorComponent* Interactor) const;

    UFUNCTION(BlueprintNativeEvent, Category="Interaction")
    void Interact(UInteractorComponent* Interactor);
    virtual void Interact_Implementation(UInteractorComponent* Interactor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
    void SetInteractionEnabled(bool bEnabled);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnRep_Enabled();
};
