#include "InteractorComponent.h"
#include "GrabbableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
        PhysicsHandle->AddTickPrerequisiteComponent(this);
        PhysicsHandle->bSoftLinearConstraint = !bRigidHold;
        PhysicsHandle->bSoftAngularConstraint = !bRigidHold;
        PhysicsHandle->SetLinearStiffness(12000.f);
        PhysicsHandle->SetLinearDamping(800.f);
        PhysicsHandle->SetAngularStiffness(24000.f);
        PhysicsHandle->SetAngularDamping(1200.f);
        PhysicsHandle->SetInterpolationSpeed(FMath::Max(1.f, HoldInterpolationSpeed));
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
    bWasCCDEnabled = Body->BodyInstance.bUseCCD;
    Body->SetUseCCD(true);
    bExtractingItem = !IsClearOfHolder(Body);
    bReleasePending = false;
    PendingThrowSpeed = 0.f;
    GetOwner()->ForceNetUpdate();
    OnRep_HeldItem();
    return true;
}

void UInteractorComponent::ReleaseHeldItem() { ReleaseInternal(0.f); }

FVector UInteractorComponent::GetSafeHoldLocation(const UPrimitiveComponent* Body, const FVector& Location, const FQuat& Rotation) const
{
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
    if (!Capsule || !Body) return Location;
    // Enclose the item's entire bounds, including off-centre pivots. The sphere
    // also remains safe while the physics handle rotates the body between poses.
    const FBoxSphereBounds Bounds = Body->CalcBounds(FTransform(Rotation, Location, Body->GetComponentScale()));
    const FVector Up = Capsule->GetUpVector();
    const FVector Center = Capsule->GetComponentLocation();
    const float Segment = FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
    const FVector Closest = Center + Up * FMath::Clamp(FVector::DotProduct(Bounds.Origin - Center, Up), -Segment, Segment);
    const FVector Offset = Bounds.Origin - Closest;
    const float Clearance = Capsule->GetScaledCapsuleRadius() + Bounds.SphereRadius + 3.f;
    if (Offset.SizeSquared() >= FMath::Square(Clearance)) return Location;
    const FVector Direction = Offset.GetSafeNormal(UE_SMALL_NUMBER, GetOwner()->GetActorForwardVector());
    return Location + Closest + Direction * Clearance - Bounds.Origin;
}

bool UInteractorComponent::IsClearOfHolder(const UPrimitiveComponent* Body) const
{
    return !Body || GetSafeHoldLocation(Body, Body->GetComponentLocation(), Body->GetComponentQuat()).Equals(Body->GetComponentLocation(), 0.1f);
}

bool UInteractorComponent::CanRestoreHolderCollision(const UPrimitiveComponent* Body) const
{
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
    if (!Capsule || !Body) return true;
    // Routing uses conservative bounds, but contact beside a wall is safe to
    // release. Test the actual body geometry against a slightly padded capsule.
    return !Body->OverlapComponent(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), Capsule->GetCollisionShape(0.05f));
}

void UInteractorComponent::ReleaseInternal(float ThrowSpeed, bool bForce)
{
    if (!GetOwner()->HasAuthority()) return;
    UGrabbableComponent* Item = HeldItem;
    UPrimitiveComponent* Body = IsValid(Item) ? Item->GetPhysicsBody() : nullptr;
    // Keep the pair filtered until extraction is complete; restoring collision
    // inside the capsule would create a depenetration impulse and launch the pawn.
    if (Body && !bForce && !CanRestoreHolderCollision(Body) && PhysicsHandle && PhysicsHandle->GetGrabbedComponent() == Body)
    {
        bReleasePending = true;
        PendingThrowSpeed = ThrowSpeed;
        return;
    }
    bReleasePending = false;
    bExtractingItem = false;
    if (PhysicsHandle) PhysicsHandle->ReleaseComponent();
    if (Body) Body->SetUseCCD(bWasCCDEnabled);
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
        if (PawnBody && !bWasAlreadyMoveIgnored) PawnBody->IgnoreActorWhenMoving(CollisionIgnoredItem->GetOwner(), false);
    }
    if (CollisionExclusion) CollisionExclusion->BreakConstraint();
    CollisionIgnoredItem = HeldItem;
    bWasAlreadyMoveIgnored = false;
    bMovementTemporarilyIgnored = false;
    bMovementNeedsFullClearance = false;
    if (!IsValid(HeldItem) || !PawnBody) return;

    UPrimitiveComponent* Body = HeldItem->GetPhysicsBody();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    // Capture the support BEFORE detaching it. This must also work on the
    // autonomous client if its movement-base replication arrived first.
    bMovementNeedsFullClearance = Body && Character && Character->GetMovementBase() == Body;
    if (Body)
    {
        if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(PawnBody))
        {
            const FVector Up = Capsule->GetUpVector();
            const FVector Feet = Capsule->GetComponentLocation() - Up * Capsule->GetScaledCapsuleHalfHeight();
            const FBoxSphereBounds Bounds = Body->CalcBounds(Body->GetComponentTransform());
            const float Top = FVector::DotProduct(Bounds.Origin - Feet, Up) + FVector::DotProduct(Bounds.BoxExtent, Up.GetAbs());
            bMovementNeedsFullClearance |= Top <= 5.f && Body->OverlapComponent(Feet - Up * 3.f, FQuat::Identity,
                FCollisionShape::MakeSphere(Capsule->GetScaledCapsuleRadius() * 0.5f));
        }
    }
    bWasAlreadyMoveIgnored = PawnBody->GetMoveIgnoreActors().Contains(HeldItem->GetOwner());
    bMovementTemporarilyIgnored = bMovementNeedsFullClearance;
    if (bMovementTemporarilyIgnored) PawnBody->IgnoreActorWhenMoving(HeldItem->GetOwner(), true);
    if (Character && bMovementNeedsFullClearance && (Character->HasAuthority() || Character->IsLocallyControlled()))
    {
        // Clear the base first so the transition cannot impart its velocity.
        Character->SetBase(nullptr);
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Character->GetCharacterMovement()->bForceNextFloorCheck = true;
    }
    if (Body && CollisionExclusion)
    {
        CollisionExclusion->SetWorldLocation(Body->GetComponentLocation());
        CollisionExclusion->SetConstrainedComponents(PawnBody, NAME_None, Body, NAME_None);
    }
}

void UInteractorComponent::UpdateHeldMovementBlocking()
{
    if (!HeldItem || !bMovementTemporarilyIgnored) return;
    if (bMovementNeedsFullClearance ? IsClearOfHolder(HeldItem->GetPhysicsBody()) : CanRestoreHolderCollision(HeldItem->GetPhysicsBody()))
    {
        if (UPrimitiveComponent* PawnBody = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            if (!bWasAlreadyMoveIgnored) PawnBody->IgnoreActorWhenMoving(HeldItem->GetOwner(), false);
        }
        bMovementTemporarilyIgnored = false;
    }
}

bool UInteractorComponent::HoldPathCrossesHolder(const UPrimitiveComponent* Body, const FVector& Goal, const FQuat& Rotation) const
{
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
    if (!Capsule) return false;
    const FBoxSphereBounds StartBounds = Body->CalcBounds(Body->GetComponentTransform());
    const FBoxSphereBounds EndBounds = Body->CalcBounds(FTransform(Rotation, Goal, Body->GetComponentScale()));
    const FVector Axis = Capsule->GetUpVector() * FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
    FVector OnPath, OnCapsule;
    FMath::SegmentDistToSegmentSafe(StartBounds.Origin, EndBounds.Origin,
        Capsule->GetComponentLocation() - Axis, Capsule->GetComponentLocation() + Axis, OnPath, OnCapsule);
    const float Radius = Capsule->GetScaledCapsuleRadius() + FMath::Max(StartBounds.SphereRadius, EndBounds.SphereRadius) + 2.f;
    return FVector::DistSquared(OnPath, OnCapsule) < FMath::Square(Radius);
}

FVector UInteractorComponent::ConstrainHoldTargetToWorld(UPrimitiveComponent* Body, const FVector& Goal, FQuat& Rotation) const
{
    FComponentQueryParams Params(SCENE_QUERY_STAT(HoldTargetSweep), GetOwner());
    Params.AddIgnoredActor(Body->GetOwner());
    const FVector Start = Body->GetComponentLocation();
    TArray<FHitResult> Hits;
    GetWorld()->ComponentSweepMulti(Hits, Body, Start, Goal, Rotation, Params);
    // Avoid rotating into adjacent geometry. Translation still follows the wall.
    if (Hits.ContainsByPredicate([](const FHitResult& Hit) { return Hit.bBlockingHit && Hit.bStartPenetrating; }))
    {
        Rotation = Body->GetComponentQuat();
    }
    FVector Result = Start;
    FVector Remaining = Goal - Start;
    for (int32 Iteration = 0; Iteration < 2 && !Remaining.IsNearlyZero(); ++Iteration)
    {
        Hits.Reset();
        GetWorld()->ComponentSweepMulti(Hits, Body, Result, Result + Remaining, Rotation, Params);
        const FHitResult* Block = nullptr;
        for (const FHitResult& Hit : Hits)
        {
            if (!Hit.bBlockingHit) continue;
            if (Hit.bStartPenetrating && FVector::DotProduct(Remaining, Hit.Normal) >= -0.01f) continue;
            if (!Block || Hit.Time < Block->Time) Block = &Hit;
        }
        if (!Block) return Result + Remaining;
        const float Fraction = Block->bStartPenetrating ? 0.f : FMath::Max(0.f, Block->Time - 0.01f);
        Result += Remaining * Fraction;
        Remaining = FVector::VectorPlaneProject(Remaining * (1.f - Fraction), Block->Normal);
    }
    return Result;
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
    UpdateHeldMovementBlocking();
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
            ReleaseInternal(0.f, true);
            return;
        }
        FVector Origin; FRotator Rotation;
        GetInteractionView(Origin, Rotation);
        FVector Target = Origin + Rotation.Vector() * FMath::Clamp(HoldDistance, 30.f, InteractionDistance);
        if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
        {
            // Looking down must not request an unreachable target under the feet.
            const FBoxSphereBounds Bounds = Body->CalcBounds(FTransform(Rotation.Quaternion() * GrabRotationOffset, Target, Body->GetComponentScale()));
            const FVector Up = Capsule->GetUpVector();
            const float Height = FVector::DotProduct(Bounds.Origin - Capsule->GetComponentLocation(), Up);
            const float MinimumHeight = -Capsule->GetScaledCapsuleHalfHeight() + Bounds.SphereRadius + 3.f;
            Target += Up * FMath::Max(0.f, MinimumHeight - Height);
        }
        if (FVector::DistSquared(Body->GetComponentLocation(), Target) > FMath::Square(FMath::Max(50.f, BreakDistance)))
        {
            ReleaseInternal(0.f);
            return;
        }
        if (bReleasePending && CanRestoreHolderCollision(Body))
        {
            ReleaseInternal(PendingThrowSpeed);
            return;
        }
        FQuat TargetRotation = Rotation.Quaternion() * GrabRotationOffset;
        const FVector Current = Body->GetComponentLocation();
        const FVector SafeCurrent = GetSafeHoldLocation(Body, Current, Body->GetComponentQuat());
        if (IsClearOfHolder(Body)) bExtractingItem = false;
        if (!bExtractingItem && !Current.Equals(SafeCurrent, 0.1f))
        {
            // Character movement can overtake a lagging physics handle. Correct
            // only that overlap, without applying forces back into the character.
            const FVector Normal = (SafeCurrent - Current).GetSafeNormal();
            FComponentQueryParams Params(SCENE_QUERY_STAT(HeldItemClearance), GetOwner());
            Params.AddIgnoredActor(Body->GetOwner());
            TArray<FHitResult> Hits;
            GetWorld()->ComponentSweepMulti(Hits, Body, Current, SafeCurrent, Body->GetComponentQuat(), Params);
            float Fraction = 1.f;
            for (const FHitResult& Hit : Hits)
            {
                if (Hit.bBlockingHit) Fraction = FMath::Min(Fraction, Hit.bStartPenetrating ? 0.f : FMath::Max(0.f, Hit.Time - 0.01f));
            }
            Body->SetWorldLocation(FMath::Lerp(Current, SafeCurrent, Fraction), false, nullptr, ETeleportType::TeleportPhysics);
            const FVector Velocity = Body->GetPhysicsLinearVelocity();
            Body->SetPhysicsLinearVelocity(Velocity - Normal * FMath::Min(0.f, FVector::DotProduct(Velocity, Normal)));
        }
        const FVector Goal = GetSafeHoldLocation(Body, Target, TargetRotation);
        FVector Next = Goal;
        if (bExtractingItem || HoldPathCrossesHolder(Body, Goal, TargetRotation))
        {
            // Limit only the route around the capsule. Ordinary tracking goes
            // directly to the aim target, without advancing from the lagging body.
            const float Step = FMath::Max(100.f, ExtractionSpeed) * FMath::Clamp(DeltaTime, 0.f, 0.05f);
            Next = GetSafeHoldLocation(Body, SafeCurrent + (Goal - SafeCurrent).GetClampedToMaxSize(Step), TargetRotation);
            if ((Next - SafeCurrent).SizeSquared() < 0.01f && FVector::DistSquared(Goal, SafeCurrent) > 100.f)
            {
                const FVector Normal = (SafeCurrent - GetOwner()->GetActorLocation()).GetSafeNormal();
                FVector Tangent = FVector::VectorPlaneProject(GetOwner()->GetActorForwardVector(), Normal).GetSafeNormal();
                if (Tangent.IsNearlyZero()) Tangent = FVector::CrossProduct(Normal, GetOwner()->GetActorRightVector()).GetSafeNormal();
                Next = GetSafeHoldLocation(Body, SafeCurrent + Tangent * Step, TargetRotation);
            }
        }
        Next = ConstrainHoldTargetToWorld(Body, Next, TargetRotation);
        if (!bExtractingItem && !GetSafeHoldLocation(Body, Next, TargetRotation).Equals(Next, 0.1f))
        {
            Next = Body->GetComponentLocation();
            TargetRotation = Body->GetComponentQuat();
        }
        PhysicsHandle->SetTargetLocationAndRotation(Next, TargetRotation.Rotator());
    }
}

void UInteractorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority()) ReleaseInternal(0.f, true);
    if (CollisionExclusion) CollisionExclusion->DestroyComponent();
    Super::EndPlay(Reason);
}

void UInteractorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UInteractorComponent, HeldItem);
}
