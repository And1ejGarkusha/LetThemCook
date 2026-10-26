#include "SimpleInteractableComponent.h"
#include "GameFramework/Actor.h"

void USimpleInteractableComponent::Interact_Implementation(UInteractorComponent* Interactor)
{
    if (CanInteract(Interactor) && GetOwner()->HasAuthority())
    {
        OnInteracted.Broadcast(Interactor);
    }
}
