#include "CustomerNPC.h"
#include "CustomerServicePoint.h"
#include "CustomerMealComponent.h"
#include "AIController.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACustomerNPC::ACustomerNPC()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .2f;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    GetCapsuleComponent()->InitCapsuleSize(34, 88);
    GetCharacterMovement()->MaxWalkSpeed = 180.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
    GetCharacterMovement()->bUseRVOAvoidance = true;
    bUseControllerRotationYaw = false;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CustomerMesh(TEXT("/Game/DataAssets/Character/Mesh/SM_Character.SM_Character"));
    static ConstructorHelpers::FObjectFinder<UBlendSpace> MovementBlend(TEXT("/Game/DataAssets/Character/Animation/BS_1D_Speed.BS_1D_Speed"));
    if (CustomerMesh.Succeeded()) GetMesh()->SetSkeletalMesh(CustomerMesh.Object);
    if (MovementBlend.Succeeded()) Locomotion = MovementBlend.Object;
    GetMesh()->SetRelativeLocation(FVector(0,0,-88));
    GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACustomerNPC::BeginPlay()
{
    Super::BeginPlay();
    if (Locomotion) GetMesh()->PlayAnimation(Locomotion, true);
}

bool ACustomerNPC::InitializeCustomer(ACustomerServicePoint* Point, const FVector& Exit, const TArray<FTabletDishItem>& Meal, int32 Payment)
{
    if (!HasAuthority() || ServicePoint || !IsValid(Point) || !UCustomerMealComponent::IsValidMeal(Meal) || Payment < 0) return false;
    ServicePoint = Point;
    ExitLocation = Exit;
    RequestedItems = Meal;
    RequestedPayment = Payment;
    QueueEnteredAt = GetWorld()->GetTimeSeconds();
    if (!GetController()) SpawnDefaultController();
    if (!Point->Enqueue(this)) { ServicePoint = nullptr; return false; }
    return true;
}

void ACustomerNPC::SetState(ECustomerState NewState)
{
    if (State == NewState) return;
    State = NewState;
    ForceNetUpdate();
    OnRep_State();
}

void ACustomerNPC::OnRep_State() { OnStateChanged.Broadcast(State); }

void ACustomerNPC::MoveToQueue(const FVector& Location, bool bAtFront)
{
    if (!HasAuthority() || State == ECustomerState::Leaving || State == ECustomerState::WaitingForFood) return;
    bFront = bAtFront;
    SetState(ECustomerState::Arriving);
    StartMove(Location);
}

void ACustomerNPC::StartMove(const FVector& Location)
{
    Destination = Location;
    bMoving = true;
    MoveStartedAt = GetWorld()->GetTimeSeconds();
    FailedMoveAttempts = 0;
    RequestMove();
}

void ACustomerNPC::RequestMove()
{
    NextMoveAttempt = GetWorld()->GetTimeSeconds() + 1;
    auto* AI = Cast<AAIController>(GetController());
    if (!AI) { ++FailedMoveAttempts; return; }
    const auto Result = AI->MoveToLocation(Destination, ArrivalRadius, false, true, true, false, nullptr, false);
    if (Result == EPathFollowingRequestResult::Failed) ++FailedMoveAttempts;
}

void ACustomerNPC::WaitForFood(const FTabletCustomerOrder& InOrder)
{
    if (!HasAuthority() || State == ECustomerState::Leaving) return;
    Order = InOrder;
    bMoving = false;
    if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    SetState(ECustomerState::WaitingForFood);
    ForceNetUpdate();
}

void ACustomerNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Locomotion)
        if (auto* Animation = GetMesh()->GetSingleNodeInstance())
        {
            const auto& Axis = Locomotion->GetBlendParameter(0);
            const float Speed = GetVelocity().Size2D();
            // Support the project's normalized axis and conventional speed-in-cm/s blend spaces.
            const float Input = Axis.Max <= 2.f ? Axis.Min + (Axis.Max-Axis.Min) * Speed / FMath::Max(1.f, AnimationReferenceSpeed) : Speed;
            Animation->SetBlendSpacePosition(FVector(FMath::Clamp(Input, Axis.Min, Axis.Max), 0, 0));
        }
    if (!HasAuthority()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (State != ECustomerState::Leaving && !IsValid(ServicePoint)) { Leave(); return; }
    if (State == ECustomerState::WaitingForFood)
    {
        const auto* GS = GetWorld()->GetGameState();
        const double ServerNow = GS ? GS->GetServerWorldTimeSeconds() : Now;
        if (Order.Deadline <= ServerNow) ServicePoint->ExpireCustomer(this);
        return;
    }
    if (State != ECustomerState::Leaving && MaximumQueueWait > 0 && Now - QueueEnteredAt > MaximumQueueWait) { Leave(); return; }
    if (!bMoving) return;
    if (FVector::DistSquared2D(GetActorLocation(), Destination) <= FMath::Square(ArrivalRadius + 8) &&
        FMath::Abs(GetActorLocation().Z - Destination.Z) <= GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 60)
    {
        bMoving = false;
        if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
        if (State == ECustomerState::Leaving) { Destroy(); return; }
        SetState(ECustomerState::InQueue);
        ServicePoint->CustomerReachedSlot(this);
        return;
    }
    if (Now - MoveStartedAt > NavigationTimeout || FailedMoveAttempts >= 3)
    {
        if (State == ECustomerState::Leaving) Destroy();
        else Leave();
        return;
    }
    if (Now >= NextMoveAttempt)
    {
        auto* AI = Cast<AAIController>(GetController());
        if (!AI || AI->GetMoveStatus() != EPathFollowingStatus::Moving) RequestMove();
    }
}

void ACustomerNPC::Leave()
{
    if (!HasAuthority() || State == ECustomerState::Leaving) return;
    SetState(ECustomerState::Leaving);
    if (IsValid(ServicePoint)) ServicePoint->RemoveCustomer(this);
    ServicePoint = nullptr;
    StartMove(ExitLocation);
    SetLifeSpan(FMath::Max(5.f, NavigationTimeout + 2));
}

void ACustomerNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority() && IsValid(ServicePoint)) ServicePoint->RemoveCustomer(this);
    Super::EndPlay(Reason);
}

void ACustomerNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACustomerNPC, State);
    DOREPLIFETIME(ACustomerNPC, ServicePoint);
    DOREPLIFETIME(ACustomerNPC, Order);
}
