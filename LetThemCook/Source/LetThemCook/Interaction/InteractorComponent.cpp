#include "InteractorComponent.h"
#include "GrabbableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UInteractorComponent::UInteractorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    SetIsReplicatedByDefault(true);
}

void UInteractorComponent::BeginPlay()
{
    Super::BeginPlay();
    PhysicsHandle = GetOwner()->FindComponentByClass<UPhysicsHandleComponent>();
    if (!PhysicsHandle && GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("Interaction: add a PhysicsHandleComponent to %s to enable grabbing."), *GetOwner()->GetName());
    }
    CollisionExclusion = NewObject<UPhysicsConstraintComponent>(GetOwner());
    CollisionExclusion->RegisterComponent();
    CollisionExclusion->SetDisableCollision(true);
    CollisionExclusion->SetLinearXLimit(ELinearConstraintMotion::LCM_Free, 0.f);
    CollisionExclusion->SetLinearYLimit(ELinearConstraintMotion::LCM_Free, 0.f);
    CollisionExclusion->SetLinearZLimit(ELinearConstraintMotion::LCM_Free, 0.f);
    CollisionExclusion->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.f);
    CollisionExclusion->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.f);
    CollisionExclusion->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.f);
    if (PhysicsHandle)
    {
        PhysicsHandle->SetLinearStiffness(1500.f);
        PhysicsHandle->SetLinearDamping(200.f);
        PhysicsHandle->SetAngularStiffness(1500.f);
        PhysicsHandle->SetAngularDamping(200.f);
        PhysicsHandle->SetInterpolationSpeed(12.f);
    }
}

bool UInteractorComponent::IsLocallyControlled() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn && Pawn->IsLocallyControlled();
}

void UInteractorComponent::GetInteractionView(FVector& Location, FRotator& Rotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const bool bExplicitView = !ViewComponent.ComponentProperty.IsNone() || !ViewComponent.PathToComponent.IsEmpty();
    const USceneComponent* View = bExplicitView ? Cast<USceneComponent>(ViewComponent.GetComponent(GetOwner())) : nullptr;
    if (!View) View = GetOwner()->FindComponentByClass<UCameraComponent>();
    Location = View ? View->GetComponentLocation() : (Pawn ? Pawn->GetPawnViewLocation() : GetOwner()->GetActorLocation());
    Rotation = Pawn ? Pawn->GetViewRotation() : (View ? View->GetComponentRotation() : GetOwner()->GetActorRotation());
}

UInteractableComponent* UInteractorComponent::TraceTarget(FHitResult* OutHit) const
{
    if (!GetWorld()) return nullptr;
    FVector Origin; FRotator Rotation;
    GetInteractionView(Origin, Rotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), false, GetOwner());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Rotation.Vector() * FMath::Max(1.f, InteractionDistance), TraceChannel, Params)) return nullptr;
    if (OutHit) *OutHit = Hit;
    AActor* Actor = Hit.GetActor();
    return Actor ? Actor->FindComponentByClass<UInteractableComponent>() : nullptr;
}

UInteractableComponent* UInteractorComponent::GetFocusedTarget() const { return FocusedTarget.Get(); }

float UInteractorComponent::CalculateCharge(float Duration, float Threshold, float ChargeTime)
{
    return FMath::Clamp((Duration - FMath::Max(0.f, Threshold)) / FMath::Max(0.05f, ChargeTime), 0.f, 1.f);
}

bool UInteractorComponent::IsChargingThrow() const
{
    return HeldItem && bLocalPressed && GetWorld() && GetWorld()->GetTimeSeconds() - LocalPressTime >= HoldThreshold;
}

float UInteractorComponent::GetThrowCharge() const
{
    return HeldItem && bLocalPressed && GetWorld()
        ? CalculateCharge(GetWorld()->GetTimeSeconds() - LocalPressTime, HoldThreshold, FullChargeTime) : 0.f;
}

FText UInteractorComponent::GetInteractionPrompt() const
{
    if (IsValid(HeldItem)) return FText::FromString(IsChargingThrow() ? TEXT("Release E: throw") : TEXT("E: drop  |  Hold E: charge throw"));
    UInteractableComponent* Target = FocusedTarget.Get();
    if (!Target) return FText::GetEmpty();
    if (!Target->CanInteract(const_cast<UInteractorComponent*>(this)))
        return FText::Format(FText::FromString(TEXT("{0} - unavailable")), Target->DisplayName);
    return FText::Format(FText::FromString(TEXT("E: {0} - {1}")), Target->ActionText, Target->DisplayName);
}

void UInteractorComponent::BeginInteraction()
{
    if (!IsLocallyControlled() || bLocalPressed) return;
    bLocalPressed = true;
    LocalPressTime = GetWorld()->GetTimeSeconds();
    ServerBeginInteraction(TraceTarget());
}

void UInteractorComponent::EndInteraction()
{
    if (!bLocalPressed) return;
    bLocalPressed = false;
    ServerEndInteraction();
}

void UInteractorComponent::CancelInteraction()
{
    bLocalPressed = false;
    if (IsLocallyControlled()) ServerCancelInteraction();
}

void UInteractorComponent::ServerBeginInteraction_Implementation(UInteractableComponent* Candidate)
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (bServerPressed || Now - LastServerPressTime < 0.075) return;
    LastServerPressTime = Now;
    bServerPressed = true;
    ServerPressTime = Now;
    PressedHeldItem = HeldItem;
    PressedTarget = nullptr;
    if (!HeldItem && IsValid(Candidate) && Candidate == TraceTarget()) PressedTarget = Candidate;
}

void UInteractorComponent::ServerEndInteraction_Implementation()
{
    if (!bServerPressed) return;
    bServerPressed = false;
    const float Duration = GetWorld()->GetTimeSeconds() - ServerPressTime;
    if (IsValid(HeldItem) && PressedHeldItem.Get() == HeldItem)
    {
        const float Charge = CalculateCharge(Duration, HoldThreshold, FullChargeTime);
        ReleaseInternal(Duration >= HoldThreshold ? FMath::Lerp(MinimumThrowSpeed, FMath::Max(MinimumThrowSpeed, MaximumThrowSpeed), Charge) : 0.f);
    }
    else if (!HeldItem && !PressedHeldItem.IsValid())
    {
        UInteractableComponent* Target = PressedTarget.Get();
        if (Target && Target == TraceTarget() && Target->CanInteract(this)) Target->Interact(this);
        else
        {
            ClientReject(FText::FromString(TEXT("Target unavailable or out of reach")));
        }
    }
    PressedTarget.Reset();
    PressedHeldItem.Reset();
}

void UInteractorComponent::ServerCancelInteraction_Implementation()
{
    bServerPressed = false;
    PressedTarget.Reset();
    PressedHeldItem.Reset();
}

void UInteractorComponent::ClientReject_Implementation(const FText& Reason) { OnRequestRejected.Broadcast(Reason); }

bool UInteractorComponent::TryGrab(UGrabbableComponent* Item)
{
    if (!GetOwner()->HasAuthority() || !PhysicsHandle || HeldItem || !IsValid(Item) || !Item->CanInteract(this)) return false;
    FHitResult Hit;
    if (TraceTarget(&Hit) != Item) return false;
    UPrimitiveComponent* Body = Item->GetPhysicsBody();
    if (!Body || Hit.GetComponent() != Body) return false;

    // Reserve before doing any physics work; all competing requests execute on the server thread.
    Item->SetHolder(this);
    HeldItem = Item;
    Item->GetOwner()->FlushNetDormancy();
    Body->WakeAllRigidBodies();
    FVector Origin; FRotator Rotation;
    GetInteractionView(Origin, Rotation);
    GrabRotationOffset = Rotation.Quaternion().Inverse() * Body->GetComponentQuat();
    PhysicsHandle->GrabComponentAtLocationWithRotation(Body, NAME_None, Body->GetComponentLocation(), Body->GetComponentRotation());
    if (PhysicsHandle->GetGrabbedComponent() != Body)
    {
        HeldItem = nullptr;
        Item->SetHolder(nullptr);
        return false;
    }
    GetOwner()->ForceNetUpdate();
    OnRep_HeldItem();
    return true;
}

void UInteractorComponent::ReleaseHeldItem() { ReleaseInternal(0.f); }

void UInteractorComponent::ReleaseInternal(float ThrowSpeed)
{
    if (!GetOwner()->HasAuthority()) return;
    UGrabbableComponent* Item = HeldItem;
    UPrimitiveComponent* Body = IsValid(Item) ? Item->GetPhysicsBody() : nullptr;
    if (PhysicsHandle) PhysicsHandle->ReleaseComponent();
    HeldItem = nullptr;
    OnRep_HeldItem();
    if (IsValid(Item))
    {
        Item->SetHolder(nullptr);
        Item->GetOwner()->ForceNetUpdate();
    }
    if (Body)
    {
        Body->WakeAllRigidBodies();
        if (ThrowSpeed > 0.f)
        {
            FVector Origin; FRotator Rotation;
            GetInteractionView(Origin, Rotation);
            Body->AddImpulse(Rotation.Vector() * ThrowSpeed, NAME_None, true);
        }
    }
    GetOwner()->ForceNetUpdate();
}

void UInteractorComponent::UpdateCollisionExclusion()
{
    if (CollisionIgnoredItem.Get() == HeldItem && (!HeldItem || CollisionIgnoredItem.IsValid())) return;
    UPrimitiveComponent* PawnBody = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
    if (CollisionIgnoredItem.IsValid())
    {
        AActor* PreviousActor = CollisionIgnoredItem->GetOwner();
        if (PawnBody && !bWasAlreadyMoveIgnored) PawnBody->IgnoreActorWhenMoving(PreviousActor, false);
    }
    if (CollisionExclusion) CollisionExclusion->BreakConstraint();
    CollisionIgnoredItem = HeldItem;
    bWasAlreadyMoveIgnored = false;
    if (IsValid(HeldItem) && PawnBody)
    {
        UPrimitiveComponent* Body = HeldItem->GetPhysicsBody();
        bWasAlreadyMoveIgnored = PawnBody->GetMoveIgnoreActors().Contains(HeldItem->GetOwner());
        PawnBody->IgnoreActorWhenMoving(HeldItem->GetOwner(), true);
        if (Body && CollisionExclusion)
        {
            // A fully free joint filters only this body/capsule pair, not every Pawn channel.
            CollisionExclusion->SetWorldLocation(Body->GetComponentLocation());
            CollisionExclusion->SetConstrainedComponents(PawnBody, NAME_None, Body, NAME_None);
        }
    }
}

void UInteractorComponent::OnRep_HeldItem()
{
    UpdateCollisionExclusion();
    OnHeldItemChanged.Broadcast(HeldItem);
}

void UInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateCollisionExclusion();
    if (IsLocallyControlled())
    {
        UInteractableComponent* NewFocus = HeldItem ? nullptr : TraceTarget();
        if (FocusedTarget.Get() != NewFocus)
        {
            FocusedTarget = NewFocus;
            OnFocusChanged.Broadcast(NewFocus);
        }
        FText Prompt = GetInteractionPrompt();
        if (!PreviousPrompt.EqualTo(Prompt))
        {
            PreviousPrompt = Prompt;
            OnPromptChanged.Broadcast(Prompt);
        }
    }
    if (GetOwner()->HasAuthority() && HeldItem)
    {
        UPrimitiveComponent* Body = IsValid(HeldItem) ? HeldItem->GetPhysicsBody() : nullptr;
        if (!Body || !PhysicsHandle || PhysicsHandle->GetGrabbedComponent() != Body || !HeldItem->IsPhysicsSetupValid())
        {
            ReleaseInternal(0.f);
            return;
        }
        FVector Origin; FRotator Rotation;
        GetInteractionView(Origin, Rotation);
        const FVector Target = Origin + Rotation.Vector() * FMath::Clamp(HoldDistance, 30.f, InteractionDistance);
        if (FVector::DistSquared(Body->GetComponentLocation(), Target) > FMath::Square(FMath::Max(50.f, BreakDistance)))
        {
            ReleaseInternal(0.f);
            return;
        }
        PhysicsHandle->SetTargetLocationAndRotation(Target, (Rotation.Quaternion() * GrabRotationOffset).Rotator());
    }
}

void UInteractorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority()) ReleaseInternal(0.f);
    if (CollisionExclusion) CollisionExclusion->DestroyComponent();
    Super::EndPlay(Reason);
}

void UInteractorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UInteractorComponent, HeldItem);
}
