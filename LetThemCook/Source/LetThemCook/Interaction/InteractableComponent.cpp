#include "InteractableComponent.h"
#include "InteractorComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UInteractableComponent::UInteractableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    DisplayName = FText::FromString(TEXT("Object"));
    ActionText = FText::FromString(TEXT("Use"));
}

void UInteractableComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner()->GetIsReplicated())
    {
        UE_LOG(LogTemp, Warning, TEXT("Interaction: %s needs Replicates enabled for multiplayer."), *GetOwner()->GetName());
    }
    TArray<UInteractableComponent*> Components;
    GetOwner()->GetComponents(Components);
    if (Components.Num() > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Interaction: %s has multiple interactables; use exactly one."), *GetOwner()->GetName());
    }
}

bool UInteractableComponent::CanInteract_Implementation(UInteractorComponent* Interactor) const
{
    return bInteractionEnabled && IsValid(Interactor) && !Interactor->GetHeldItem();
}

void UInteractableComponent::Interact_Implementation(UInteractorComponent* Interactor)
{
    if (GetOwner()->HasAuthority() && CanInteract(Interactor))
    {
        OnInteracted.Broadcast(Interactor);
    }
}

void UInteractableComponent::SetInteractionEnabled(bool bEnabled)
{
    if (GetOwner()->HasAuthority())
    {
        bInteractionEnabled = bEnabled;
        GetOwner()->ForceNetUpdate();
        OnRep_Enabled();
    }
}

void UInteractableComponent::OnRep_Enabled() {}

void UInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UInteractableComponent, bInteractionEnabled);
}
