#include "CustomerMealComponent.h"
#include "Net/UnrealNetwork.h"

UCustomerMealComponent::UCustomerMealComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool UCustomerMealComponent::IsValidMeal(const TArray<FTabletDishItem>& Meal)
{
    if (Meal.IsEmpty() || Meal.Num() > 16) return false;
    int32 Count = 0;
    for (const auto& Item : Meal)
    {
        if (uint8(Item.Dish) > uint8(ETabletDish::Drink) || Item.Quantity < 1 || Item.Quantity > 20) return false;
        Count += Item.Quantity;
    }
    return Count <= 100;
}

bool UCustomerMealComponent::InitializeMeal(const TArray<FTabletDishItem>& InItems)
{
    if (!GetOwner()->HasAuthority() || bReady || !IsValidMeal(InItems)) return false;
    Items = InItems;
    bReady = true;
    GetOwner()->ForceNetUpdate();
    return true;
}

bool UCustomerMealComponent::Matches(const TArray<FTabletDishItem>& Requested) const
{
    if (!IsReady() || !IsValidMeal(Requested)) return false;
    auto Quantities = [](const TArray<FTabletDishItem>& Meal)
    {
        TMap<uint8, int32> Result;
        for (const auto& Item : Meal)
        {
            uint8 Key = uint8(Item.Dish) << 4;
            if (Item.Dish == ETabletDish::Burger)
                Key |= uint8(Item.Burger.bIceberg) | (uint8(Item.Burger.bPickle) << 1) |
                    (uint8(Item.Burger.bTomato) << 2) | (uint8(Item.Burger.bCheese) << 3);
            Result.FindOrAdd(Key) += Item.Quantity;
        }
        return Result;
    };
    const auto Actual = Quantities(Items);
    const auto Expected = Quantities(Requested);
    if (Actual.Num() != Expected.Num()) return false;
    for (const auto& Entry : Expected)
        if (Actual.FindRef(Entry.Key) != Entry.Value) return false;
    return true;
}

bool UCustomerMealComponent::Consume()
{
    if (!GetOwner()->HasAuthority() || !IsReady()) return false;
    bConsumed = true;
    GetOwner()->ForceNetUpdate();
    return true;
}

void UCustomerMealComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCustomerMealComponent, Items);
    DOREPLIFETIME(UCustomerMealComponent, bReady);
    DOREPLIFETIME(UCustomerMealComponent, bConsumed);
}
