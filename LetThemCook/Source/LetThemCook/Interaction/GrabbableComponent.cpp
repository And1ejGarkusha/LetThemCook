#include "GrabbableComponent.h"
#include "InteractorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"

UGrabbableComponent::UGrabbableComponent()
{
    ActionText = FText::FromString(TEXT("Pick up"));
}

UPrimitiveComponent* UGrabbableComponent::GetPhysicsBody() const
{
    if (!GetOwner()) return nullptr;
    if (UActorComponent* Explicit = PhysicsBody.GetComponent(GetOwner()))
    {
        return Cast<UPrimitiveComponent>(Explicit);
    }
    if (!PhysicsBody.ComponentProperty.IsNone() || !PhysicsBody.PathToComponent.IsEmpty()) return nullptr;
    return Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
}

bool UGrabbableComponent::IsPhysicsSetupValid() const
{
    const UPrimitiveComponent* Body = GetPhysicsBody();
    return Body && Body == GetOwner()->GetRootComponent()
        && Body->Mobility == EComponentMobility::Movable
        && Body->IsSimulatingPhysics() && Body->IsPhysicsCollisionEnabled()
        && Body->GetMass() <= MaximumMass
        && GetOwner()->GetIsReplicated() && GetOwner()->IsReplicatingMovement();
}

void UGrabbableComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UPrimitiveComponent* Body = GetPhysicsBody())
    {
        Body->CanCharacterStepUpOn = bAllowCharacterStepUp ? ECB_Yes : ECB_No;
    }
    if (!IsPhysicsSetupValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Interaction: %s cannot be grabbed. Require a movable primitive ROOT, physics + collision, mass <= %.1f kg, Replicates and Replicate Movement."), *GetOwner()->GetName(), MaximumMass);
    }
}

bool UGrabbableComponent::CanInteract_Implementation(UInteractorComponent* Interactor) const
{
    return Super::CanInteract_Implementation(Interactor) && !Holder && IsPhysicsSetupValid();
}

void UGrabbableComponent::Interact_Implementation(UInteractorComponent* Interactor)
{
    if (GetOwner()->HasAuthority() && CanInteract(Interactor))
    {
        Interactor->TryGrab(this);
    }
}

void UGrabbableComponent::SetHolder(UInteractorComponent* NewHolder)
{
    UInteractorComponent* Previous = Holder;
    Holder = NewHolder;
    GetOwner()->ForceNetUpdate();
    OnRep_Holder(Previous);
}

void UGrabbableComponent::OnRep_Holder(UInteractorComponent* PreviousHolder)
{
    if (Holder) OnGrabbed.Broadcast(Holder);
    else if (PreviousHolder) OnReleased.Broadcast(PreviousHolder);
}

void UGrabbableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority() && IsValid(Holder)) Holder->ReleaseHeldItem();
    Super::EndPlay(Reason);
}

void UGrabbableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGrabbableComponent, Holder);
}
