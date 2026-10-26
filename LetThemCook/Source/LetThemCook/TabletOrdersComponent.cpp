#include "TabletOrdersComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CustomerMealComponent.h"

UTabletOrdersComponent::UTabletOrdersComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    const TCHAR* Keys[] = { TEXT("BunTop"), TEXT("BunBottom"), TEXT("Patty"), TEXT("Iceberg"),
        TEXT("Pickle"), TEXT("Tomato"), TEXT("Cheese"), TEXT("Fries"), TEXT("Cup"), TEXT("Orders"), TEXT("Locked") };
    for (const TCHAR* Key : Keys)
    {
        const FString Path = FString::Printf(TEXT("/Game/UI/Tablet/Orders/T_%s.T_%s"), Key, Key);
        ConstructorHelpers::FObjectFinder<UTexture2D> Texture(*Path);
        if (Texture.Succeeded()) Artwork.Add(FName(Key), Texture.Object);
    }
}

void UTabletOrdersComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        State.bReady = true;
        GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &UTabletOrdersComponent::ExpireOrders, .25f, true);
        NotifyChanged();
    }
}

void UTabletOrdersComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
    Super::EndPlay(Reason);
}

void UTabletOrdersComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UTabletOrdersComponent, State, COND_OwnerOnly);
}

double UTabletOrdersComponent::ServerTime() const
{
    if (const AGameStateBase* GameState = GetWorld()->GetGameState()) return GameState->GetServerWorldTimeSeconds();
    return GetWorld()->GetTimeSeconds();
}

FGuid UTabletOrdersComponent::SubmitOrder(ETabletDish Dish, FTabletBurgerComposition Burger, int32 Payment, float PatienceSeconds)
{
    FTabletDishItem Item;
    Item.Dish = Dish;
    Item.Burger = Burger;
    return SubmitMealOrder({Item}, Payment, PatienceSeconds);
}

FGuid UTabletOrdersComponent::SubmitMealOrder(const TArray<FTabletDishItem>& Items, int32 Payment, float PatienceSeconds)
{
    if (!GetOwner()->HasAuthority() || !State.bReady || State.Orders.Num() >= 32 || Payment < 0 ||
        !FMath::IsFinite(PatienceSeconds) || PatienceSeconds <= 0 || PatienceSeconds > 180 ||
        Items.IsEmpty() || Items.Num() > 16) return FGuid();
    int32 TotalQuantity = 0;
    for (const auto& Item : Items)
    {
        if (static_cast<uint8>(Item.Dish) > static_cast<uint8>(ETabletDish::Drink) || Item.Quantity < 1 || Item.Quantity > 20) return FGuid();
        TotalQuantity += Item.Quantity;
    }
    if (TotalQuantity > 100) return FGuid();
    FTabletCustomerOrder Order;
    Order.Id = FGuid::NewGuid();
    Order.Number = NextNumber++;
    Order.Items = Items;
    // Preserve the original single-dish fields for existing gameplay callers.
    Order.Dish = Items[0].Dish;
    Order.Burger = Items[0].Burger;
    Order.Payment = Payment;
    Order.Deadline = ServerTime() + PatienceSeconds;
    State.Orders.Add(Order);
    NotifyChanged();
    return Order.Id;
}

bool UTabletOrdersComponent::RemoveOrder(FGuid OrderId)
{
    if (!GetOwner()->HasAuthority()) return false;
    if (!State.Orders.RemoveAll([OrderId](const FTabletCustomerOrder& Order) { return Order.Id == OrderId; })) return false;
    NotifyChanged();
    return true;
}

bool UTabletOrdersComponent::PublishCustomerOrder(const FTabletCustomerOrder& Order)
{
    if (!GetOwner()->HasAuthority() || !State.bReady || !Order.Id.IsValid() || Order.Number <= 0 || Order.Payment < 0 ||
        !FMath::IsFinite(Order.Deadline) || Order.Deadline <= ServerTime() || Order.Deadline - ServerTime() > 180.1 ||
        !UCustomerMealComponent::IsValidMeal(GetItems(Order))) return false;
    if (FindOrder(Order.Id)) return true;
    if (State.Orders.Num() >= 32) return false;
    State.Orders.Add(Order);
    NextNumber = FMath::Max(NextNumber, Order.Number + 1);
    NotifyChanged();
    return true;
}

void UTabletOrdersComponent::ExpireOrders()
{
    const double Now = ServerTime();
    TArray<FTabletCustomerOrder> Expired;
    for (const auto& Order : State.Orders) if (Order.Deadline <= Now) Expired.Add(Order);
    if (Expired.IsEmpty()) return;
    State.Orders.RemoveAll([Now](const FTabletCustomerOrder& Order) { return Order.Deadline <= Now; });
    NotifyChanged();
    // Broadcast after removal so customer callbacks can safely submit/cancel other orders.
    for (const auto& Order : Expired) OnOrderExpired.Broadcast(Order);
}

void UTabletOrdersComponent::NotifyChanged()
{
    ++State.Revision;
    GetOwner()->ForceNetUpdate();
    OnOrdersChanged.Broadcast();
    OnChanged.Broadcast();
}

void UTabletOrdersComponent::OnRep_State() { OnOrdersChanged.Broadcast(); OnChanged.Broadcast(); }
void UTabletOrdersComponent::RefreshOrders() { ServerRefreshOrders(); }
void UTabletOrdersComponent::ServerRefreshOrders_Implementation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (LastRefreshTime >= 0 && Now - LastRefreshTime < .5) return;
    LastRefreshTime = Now;
    NotifyChanged();
}

int32 UTabletOrdersComponent::GetSecondsRemaining(const FTabletCustomerOrder& Order) const
{
    return FMath::Max(0, FMath::CeilToInt(Order.Deadline - ServerTime()));
}

const FTabletCustomerOrder* UTabletOrdersComponent::FindOrder(FGuid Id) const
{
    return State.Orders.FindByPredicate([Id](const FTabletCustomerOrder& Order) { return Order.Id == Id; });
}

UTexture2D* UTabletOrdersComponent::GetArtwork(FName Key) const { return Artwork.FindRef(Key); }

FText UTabletOrdersComponent::DishName(ETabletDish Dish)
{
    switch (Dish)
    {
    case ETabletDish::Fries: return FText::FromString(TEXT("Картофель фри"));
    case ETabletDish::Drink: return FText::FromString(TEXT("Напиток"));
    default: return FText::FromString(TEXT("Бургер"));
    }
}

FText UTabletOrdersComponent::Description(const FTabletCustomerOrder& Order)
{
    const auto Items = GetItems(Order);
    if (Items.Num() == 1) return ItemDescription(Items[0]);
    TArray<FString> Names;
    for (const auto& Item : Items)
        Names.Add(FString::Printf(TEXT("%s × %d"), *DishName(Item.Dish).ToString(), Item.Quantity));
    return FText::FromString(FString::Join(Names, TEXT(" · ")));
}

TArray<FTabletDishItem> UTabletOrdersComponent::GetItems(const FTabletCustomerOrder& Order)
{
    if (!Order.Items.IsEmpty()) return Order.Items;
    FTabletDishItem Item;
    Item.Dish = Order.Dish;
    Item.Burger = Order.Burger;
    return {Item};
}

FText UTabletOrdersComponent::OrderTitle(const FTabletCustomerOrder& Order)
{
    const auto Items = GetItems(Order);
    FString Title = DishName(Items[0].Dish).ToString();
    if (Items[0].Quantity > 1) Title += FString::Printf(TEXT(" × %d"), Items[0].Quantity);
    if (Items.Num() > 1) Title += FString::Printf(TEXT(" + ещё %d"), Items.Num() - 1);
    return FText::FromString(Title);
}

FText UTabletOrdersComponent::ItemDescription(const FTabletDishItem& Item)
{
    if (Item.Dish == ETabletDish::Fries) return FText::FromString(TEXT("Средняя порция"));
    if (Item.Dish == ETabletDish::Drink) return FText::FromString(TEXT("Средний · закрытый стакан"));
    TArray<FString> Excluded;
    if (!Item.Burger.bCheese) Excluded.Add(TEXT("сыра"));
    if (!Item.Burger.bPickle) Excluded.Add(TEXT("огурца"));
    if (!Item.Burger.bTomato) Excluded.Add(TEXT("помидора"));
    if (!Item.Burger.bIceberg) Excluded.Add(TEXT("айсберга"));
    if (Excluded.IsEmpty()) return FText::FromString(TEXT("Полный состав"));
    if (Excluded.Num() == 1) return FText::FromString(TEXT("Без ") + Excluded[0]);
    const FString Last = Excluded.Pop();
    return FText::FromString(TEXT("Без ") + FString::Join(Excluded, TEXT(", ")) + TEXT(" и ") + Last);
}

TArray<FTabletRecipePart> UTabletOrdersComponent::Recipe(const FTabletCustomerOrder& Order)
{
    return ItemRecipe(GetItems(Order)[0]);
}

TArray<FTabletRecipePart> UTabletOrdersComponent::ItemRecipe(const FTabletDishItem& Item)
{
    TArray<FTabletRecipePart> Parts;
    auto Add = [&Parts](const TCHAR* Key, const TCHAR* Name, const TCHAR* Details)
    { Parts.Add({FName(Key), FText::FromString(Name), FText::FromString(Details)}); };
    if (Item.Dish == ETabletDish::Fries) Add(TEXT("Fries"), TEXT("Картофель фри"), TEXT("Средняя порция"));
    else if (Item.Dish == ETabletDish::Drink) Add(TEXT("Cup"), TEXT("Напиток"), TEXT("Средний · стакан с крышкой"));
    else
    {
        Add(TEXT("BunTop"), TEXT("Булка"), TEXT("верхняя и нижняя"));
        if (Item.Burger.bIceberg) Add(TEXT("Iceberg"), TEXT("Айсберг"), TEXT("1 порция"));
        if (Item.Burger.bPickle) Add(TEXT("Pickle"), TEXT("Огурец"), TEXT("3 ломтика"));
        if (Item.Burger.bTomato) Add(TEXT("Tomato"), TEXT("Помидор"), TEXT("2 ломтика"));
        if (Item.Burger.bCheese) Add(TEXT("Cheese"), TEXT("Сыр"), TEXT("1 плоская пластинка"));
        Add(TEXT("Patty"), TEXT("Котлета"), TEXT("1 шт."));
        Add(TEXT("BunBottom"), TEXT("Булка нижняя"), TEXT("1 шт."));
    }
    return Parts;
}
