#include "CustomerSpawner.h"
#include "CustomerNPC.h"
#include "CustomerServicePoint.h"
#include "CustomerMealComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACustomerSpawner::ACustomerSpawner()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
    Arrow->SetupAttachment(RootComponent);
    Arrow->ArrowColor = FColor(60,200,100);
    CustomerClass = ACustomerNPC::StaticClass();
    auto Add = [this](FName Name, std::initializer_list<ETabletDish> Dishes, int32 Price, float Weight)
    {
        FCustomerMenuOption Option;
        Option.Name = Name;
        Option.Payment = Price;
        Option.Weight = Weight;
        for (auto Dish : Dishes) { FTabletDishItem Item; Item.Dish = Dish; Option.Items.Add(Item); }
        Menu.Add(Option);
    };
    Add(TEXT("Burger"), {ETabletDish::Burger}, 160, 4);
    Add(TEXT("Fries"), {ETabletDish::Fries}, 70, 1);
    Add(TEXT("Drink"), {ETabletDish::Drink}, 50, 1);
    Add(TEXT("Combo"), {ETabletDish::Burger, ETabletDish::Fries, ETabletDish::Drink}, 280, 2);
}

void ACustomerSpawner::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(RandomSeed ? RandomSeed : FMath::Rand());
    if (bAutoStart) StartSpawning();
}

void ACustomerSpawner::StartSpawning()
{
    if (!HasAuthority() || GetWorldTimerManager().IsTimerActive(SpawnTimer)) return;
    GetWorldTimerManager().SetTimer(SpawnTimer, this, &ACustomerSpawner::SpawnNext,
        FMath::Max(.5f, SpawnInterval), true, FMath::Max(.01f, InitialDelay));
}

void ACustomerSpawner::StopSpawning()
{
    if (HasAuthority()) GetWorldTimerManager().ClearTimer(SpawnTimer);
}

void ACustomerSpawner::SpawnNext() { SpawnCustomer(); }

int32 ACustomerSpawner::GetAliveCount() const
{
    int32 Count = 0;
    for (const auto& Customer : ActiveCustomers) if (Customer.IsValid()) ++Count;
    return Count;
}

ACustomerNPC* ACustomerSpawner::SpawnCustomer()
{
    if (!HasAuthority() || !CustomerClass || CustomerClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
    ActiveCustomers.RemoveAll([](const TWeakObjectPtr<ACustomerNPC>& Customer) { return !Customer.IsValid(); });
    if (ActiveCustomers.Num() >= FMath::Clamp(MaxAlive, 1, 100)) return nullptr;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return nullptr;
    struct FCandidate { ACustomerServicePoint* Point; float Weight; };
    TArray<FCandidate> Candidates;
    float TotalWeight = 0;
    for (const auto& Point : DestinationPool)
    {
        if (!IsValid(Point) || Candidates.ContainsByPredicate([&Point](const FCandidate& C) { return C.Point == Point; })) continue;
        const float Weight = Point->GetSelectionWeight();
        FNavLocation Projected;
        if (!FMath::IsFinite(Weight) || Weight <= 0 || !Nav->ProjectPointToNavigation(Point->GetQueueLocation(Point->GetQueue().Num()), Projected, FVector(100,100,250))) continue;
        Candidates.Add({Point, Weight});
        TotalWeight += Weight;
    }
    if (Candidates.IsEmpty()) return nullptr;
    float Choice = Random.FRandRange(0, TotalWeight);
    ACustomerServicePoint* Destination = Candidates.Last().Point;
    for (const auto& Candidate : Candidates) if ((Choice -= Candidate.Weight) <= 0) { Destination = Candidate.Point; break; }
    TArray<const FCustomerMenuOption*> AvailableMenu;
    TotalWeight = 0;
    for (const auto& Option : Menu)
        if (FMath::IsFinite(Option.Weight) && Option.Weight > 0 && Option.Payment >= 0 && Option.Payment <= 100000 && UCustomerMealComponent::IsValidMeal(Option.Items))
        { AvailableMenu.Add(&Option); TotalWeight += Option.Weight; }
    if (AvailableMenu.IsEmpty()) return nullptr;
    Choice = Random.FRandRange(0, TotalWeight);
    const FCustomerMenuOption* Selected = AvailableMenu.Last();
    for (const auto* Option : AvailableMenu) if ((Choice -= Option->Weight) <= 0) { Selected = Option; break; }
    TArray<AActor*> ValidSpawns;
    for (const auto& Point : SpawnPoints) if (IsValid(Point)) ValidSpawns.Add(Point);
    AActor* SpawnPoint = ValidSpawns.IsEmpty() ? this : ValidSpawns[Random.RandRange(0, ValidSpawns.Num()-1)];
    FNavLocation SpawnLocation;
    if (!Nav->ProjectPointToNavigation(SpawnPoint->GetActorLocation(), SpawnLocation, FVector(150,150,300))) return nullptr;
    FNavLocation QueueLocation;
    if (!Nav->ProjectPointToNavigation(Destination->GetQueueLocation(Destination->GetQueue().Num()), QueueLocation, FVector(100,100,250))) return nullptr;
    const FVector PathStart = SpawnLocation.Location;
    const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), PathStart, QueueLocation.Location);
    if (!Path || !Path->IsValid() || Path->IsPartial()) return nullptr;
    TArray<AActor*> ValidExits;
    for (const auto& Point : ExitPoints) if (IsValid(Point)) ValidExits.Add(Point);
    FVector Exit = SpawnLocation.Location;
    if (!ValidExits.IsEmpty()) Exit = ValidExits[Random.RandRange(0, ValidExits.Num()-1)]->GetActorLocation();
    const auto* Defaults = CustomerClass->GetDefaultObject<ACustomerNPC>();
    const FVector SpawnCenter = SpawnLocation.Location + FVector(0,0,Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()+2);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    auto* Customer = GetWorld()->SpawnActor<ACustomerNPC>(CustomerClass, SpawnCenter, SpawnPoint->GetActorRotation(), Params);
    if (!Customer) return nullptr;
    auto Items = Selected->Items;
    if (bRandomizeBurgerComposition) for (auto& Item : Items) if (Item.Dish == ETabletDish::Burger)
    {
        const float Chance = FMath::Clamp(OmitIngredientChance, 0.f, 1.f);
        Item.Burger.bIceberg = Item.Burger.bIceberg && Random.FRand() >= Chance;
        Item.Burger.bPickle = Item.Burger.bPickle && Random.FRand() >= Chance;
        Item.Burger.bTomato = Item.Burger.bTomato && Random.FRand() >= Chance;
        Item.Burger.bCheese = Item.Burger.bCheese && Random.FRand() >= Chance;
    }
    if (!Customer->InitializeCustomer(Destination, Exit, Items, Selected->Payment)) { Customer->Destroy(); return nullptr; }
    ActiveCustomers.Add(Customer);
    OnCustomerSpawned.Broadcast(Customer);
    return Customer;
}

void ACustomerSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(SpawnTimer);
    if (HasAuthority()) for (const auto& Customer : ActiveCustomers) if (Customer.IsValid()) Customer->Leave();
    Super::EndPlay(Reason);
}
