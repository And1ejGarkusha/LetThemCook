#include "CustomerServicePoint.h"
#include "CustomerNPC.h"
#include "CustomerMealComponent.h"
#include "TabletShopComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACustomerServicePoint::ACustomerServicePoint()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("CounterDirection"));
    Arrow->SetupAttachment(RootComponent);
    Arrow->ArrowColor = FColor(220,150,40);
    Arrow->ArrowSize = 2;
    ServingZone = CreateDefaultSubobject<UBoxComponent>(TEXT("ServingZone"));
    ServingZone->SetupAttachment(RootComponent);
    ServingZone->SetRelativeLocation(FVector(120,0,90));
    ServingZone->SetBoxExtent(FVector(70,100,70));
    ServingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ServingZone->SetCanEverAffectNavigation(false);
}

void ACustomerServicePoint::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) GetWorldTimerManager().SetTimer(SyncTimer, this, &ACustomerServicePoint::SyncOrderInboxes, .5f, true);
}

TArray<APlayerController*> ACustomerServicePoint::GetServingControllers() const
{
    TArray<APlayerController*> Result;
    const auto* GS = GetWorld()->GetGameState();
    if (!GS) return Result;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* PC = It->Get();
        if (!IsValid(PC) || !IsValid(PC->PlayerState)) continue;
        const bool bMember = AssignedPlayers.IsEmpty() ?
            (GS->PlayerArray.IsValidIndex(PlayerSlot) && GS->PlayerArray[PlayerSlot] == PC->PlayerState) : AssignedPlayers.Contains(PC->PlayerState);
        if (bMember && PC->FindComponentByClass<UTabletOrdersComponent>()) Result.Add(PC);
    }
    // Keep the wallet owner deterministic when teams have multiple controllers.
    if (!AssignedPlayers.IsEmpty()) Result.Sort([this](const APlayerController& A, const APlayerController& B)
        { return AssignedPlayers.IndexOfByKey(A.PlayerState) < AssignedPlayers.IndexOfByKey(B.PlayerState); });
    return Result;
}

void ACustomerServicePoint::SetServingPlayers(const TArray<APlayerState*>& Players)
{
    if (!HasAuthority()) return;
    ClearPublishedOrder();
    AssignedPlayers.Reset();
    for (auto* Player : Players) if (IsValid(Player)) AssignedPlayers.AddUnique(Player);
    SyncOrderInboxes();
    ForceNetUpdate();
}

bool ACustomerServicePoint::CanPlayerServe(APlayerController* Player) const
{
    return IsValid(Player) && GetServingControllers().Contains(Player);
}

bool ACustomerServicePoint::CanAcceptCustomer() const
{
    return !bClosing && Queue.Num() < FMath::Clamp(QueueCapacity, 1, 20) && !GetServingControllers().IsEmpty();
}

float ACustomerServicePoint::GetSelectionWeight() const
{
    return CanAcceptCustomer() ? (1.f + FMath::Clamp(Reputation, 0.f, 100.f)) / (1.f + Queue.Num()) : 0.f;
}

FVector ACustomerServicePoint::GetQueueLocation(int32 Index) const
{
    if (QueuePoints.IsValidIndex(Index) && IsValid(QueuePoints[Index])) return QueuePoints[Index]->GetActorLocation();
    return GetActorLocation() - GetActorForwardVector() * FMath::Max(100.f, QueueSpacing) * Index;
}

TArray<ACustomerNPC*> ACustomerServicePoint::GetQueue() const
{
    TArray<ACustomerNPC*> Result;
    for (const auto& Customer : Queue) if (IsValid(Customer)) Result.Add(Customer);
    return Result;
}

ACustomerNPC* ACustomerServicePoint::GetCurrentCustomer() const
{
    return Queue.IsEmpty() ? nullptr : Queue[0].Get();
}

bool ACustomerServicePoint::Enqueue(ACustomerNPC* Customer)
{
    if (!HasAuthority() || !IsValid(Customer) || Queue.Contains(Customer) || !CanAcceptCustomer()) return false;
    Queue.Add(Customer);
    UpdateQueue();
    OnRep_Queue();
    ForceNetUpdate();
    return true;
}

void ACustomerServicePoint::UpdateQueue()
{
    const auto Snapshot = Queue;
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
        if (IsValid(Snapshot[Index])) Snapshot[Index]->MoveToQueue(GetQueueLocation(Index), Index == 0);
}

void ACustomerServicePoint::CustomerReachedSlot(ACustomerNPC* Customer)
{
    if (!HasAuthority() || bClosing || GetCurrentCustomer() != Customer || !IsValid(Customer) ||
        Customer->State != ECustomerState::InQueue || FVector::DistSquared2D(Customer->GetActorLocation(), GetQueueLocation(0)) > FMath::Square(Customer->ArrivalRadius + 12)) return;
    const auto Controllers = GetServingControllers();
    if (Controllers.IsEmpty()) { Customer->Leave(); return; }
    auto* Inbox = Controllers[0]->FindComponentByClass<UTabletOrdersComponent>();
    const FGuid Id = Inbox->SubmitMealOrder(Customer->GetRequestedItems(), Customer->GetRequestedPayment(), FMath::Clamp(PatienceSeconds, 1.f, 180.f));
    const auto* Created = Inbox->FindOrder(Id);
    if (!Created) { Customer->Leave(); return; }
    Customer->WaitForFood(*Created);
    SyncOrderInboxes();
    OnOrderCreated.Broadcast(Customer, Customer->Order);
}

void ACustomerServicePoint::ClearPublishedOrder()
{
    if (PublishedOrderId.IsValid())
        for (auto& WeakInbox : PublishedInboxes)
            if (auto* Inbox = WeakInbox.Get()) Inbox->RemoveOrder(PublishedOrderId);
    PublishedInboxes.Reset();
    PublishedOrderId.Invalidate();
}

void ACustomerServicePoint::SyncOrderInboxes()
{
    if (!HasAuthority() || bClosing) return;
    Queue.RemoveAll([](const TObjectPtr<ACustomerNPC>& Customer) { return !IsValid(Customer); });
    auto* Customer = GetCurrentCustomer();
    if (!Customer || Customer->State != ECustomerState::WaitingForFood) return;
    const auto Controllers = GetServingControllers();
    if (Controllers.IsEmpty()) { Customer->Leave(); return; }
    const FGuid Id = Customer->Order.Id;
    if (PublishedOrderId.IsValid() && PublishedOrderId != Id) ClearPublishedOrder();
    PublishedOrderId = Id;
    TArray<TWeakObjectPtr<UTabletOrdersComponent>> Current;
    for (auto* PC : Controllers)
    {
        auto* Inbox = PC->FindComponentByClass<UTabletOrdersComponent>();
        if (Inbox->PublishCustomerOrder(Customer->Order)) Current.Add(Inbox);
    }
    for (auto& Previous : PublishedInboxes)
        if (Previous.IsValid() && !Current.Contains(Previous)) Previous->RemoveOrder(Id);
    PublishedInboxes = MoveTemp(Current);
}

void ACustomerServicePoint::RemoveCustomer(ACustomerNPC* Customer)
{
    if (!HasAuthority() || bClosing) return;
    if (GetCurrentCustomer() == Customer)
    {
        // Include the primary inbox even if this happens before the first synchronization tick.
        if (!PublishedOrderId.IsValid() && Customer && Customer->Order.Id.IsValid())
        {
            PublishedOrderId = Customer->Order.Id;
            for (auto* PC : GetServingControllers()) PublishedInboxes.AddUnique(PC->FindComponentByClass<UTabletOrdersComponent>());
        }
        ClearPublishedOrder();
    }
    if (Queue.Remove(Customer) > 0)
    {
        UpdateQueue();
        OnRep_Queue();
        ForceNetUpdate();
    }
}

void ACustomerServicePoint::ExpireCustomer(ACustomerNPC* Customer)
{
    if (!HasAuthority() || GetCurrentCustomer() != Customer || !IsValid(Customer) || Customer->State != ECustomerState::WaitingForFood) return;
    Reputation = FMath::Clamp(Reputation - ReputationLoss, 0.f, 100.f);
    Customer->Leave();
    ForceNetUpdate();
}

bool ACustomerServicePoint::TryServeMeal(APlayerController* Player, AActor* Food)
{
    auto* Customer = GetCurrentCustomer();
    if (!HasAuthority() || !CanPlayerServe(Player) || !IsValid(Player->GetPawn()) || !IsValid(Food) || !Customer ||
        Customer->State != ECustomerState::WaitingForFood) return false;
    const auto* GS = GetWorld()->GetGameState();
    const double Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    if (Customer->Order.Deadline <= Now) { ExpireCustomer(Customer); return false; }
    const FVector Counter = GetQueueLocation(0);
    const float Range = FMath::Clamp(ServingDistance, 50.f, 500.f);
    if (FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Counter) > FMath::Square(Range) ||
        FVector::DistSquared(Food->GetActorLocation(), Counter) > FMath::Square(Range) ||
        FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Food->GetActorLocation()) > FMath::Square(Range)) return false;
    auto* Meal = Food->FindComponentByClass<UCustomerMealComponent>();
    const auto Controllers = GetServingControllers();
    auto* Wallet = Controllers.IsEmpty() ? nullptr : Controllers[0]->FindComponentByClass<UTabletShopComponent>();
    if (!Meal || !Wallet || !Meal->Matches(UTabletOrdersComponent::GetItems(Customer->Order)) || !Meal->Consume()) return false;
    const int32 Payment = Customer->Order.Payment;
    Wallet->CreditSale(Payment);
    TotalRevenue += Payment;
    Reputation = FMath::Clamp(Reputation + ReputationGain, 0.f, 100.f);
    // Remove the customer/order before notifying gameplay, so callbacks cannot fulfill it twice.
    Customer->Leave();
    Food->Destroy();
    ForceNetUpdate();
    OnCustomerServed.Broadcast(Customer, Player, Payment);
    return true;
}

void ACustomerServicePoint::OnRep_Queue() { OnQueueChanged.Broadcast(this); }

void ACustomerServicePoint::EndPlay(const EEndPlayReason::Type Reason)
{
    bClosing = true;
    GetWorldTimerManager().ClearTimer(SyncTimer);
    if (HasAuthority())
    {
        ClearPublishedOrder();
        const auto Snapshot = Queue;
        Queue.Reset();
        for (const auto& Customer : Snapshot) if (IsValid(Customer)) Customer->Leave();
    }
    Super::EndPlay(Reason);
}

void ACustomerServicePoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACustomerServicePoint, Queue);
    DOREPLIFETIME(ACustomerServicePoint, PlayerSlot);
    DOREPLIFETIME(ACustomerServicePoint, CafeId);
    DOREPLIFETIME(ACustomerServicePoint, AssignedPlayers);
    DOREPLIFETIME(ACustomerServicePoint, Reputation);
    DOREPLIFETIME(ACustomerServicePoint, TotalRevenue);
}
