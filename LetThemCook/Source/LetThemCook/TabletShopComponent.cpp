#include "TabletShopComponent.h"
#include "TabletDelivery.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

UTabletShopComponent::UTabletShopComponent()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Ingredients(TEXT("/Game/UI/Tablet/T_Ingredients.T_Ingredients"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Upgrades(TEXT("/Game/UI/Tablet/T_Upgrades.T_Upgrades"));
    IngredientsArtwork = Ingredients.Object;
    UpgradesArtwork = Upgrades.Object;
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    auto Add = [this](const TCHAR* Id, const FText& Name, int32 Price)
    {
        FTabletProduct Product;
        Product.Id = FName(Id); Product.Name = Name; Product.Price = Price;
        Products.Add(Product);
    };
    Add(TEXT("Bun"), NSLOCTEXT("Tablet", "Bun", "Булка"), 5);
    Add(TEXT("Patty"), NSLOCTEXT("Tablet", "Patty", "Котлета"), 15);
    Add(TEXT("Cheese"), NSLOCTEXT("Tablet", "Cheese", "Сыр"), 7);
    Add(TEXT("Sauce"), NSLOCTEXT("Tablet", "Sauce", "Соус"), 4);
    Add(TEXT("Tomato"), NSLOCTEXT("Tablet", "Tomato", "Нарезанные помидоры"), 6);
    Add(TEXT("Pickle"), NSLOCTEXT("Tablet", "Pickle", "Нарезанные огурцы"), 6);
    Add(TEXT("Sausage"), NSLOCTEXT("Tablet", "Sausage", "Сосиска"), 12);
    Add(TEXT("Fries"), NSLOCTEXT("Tablet", "Fries", "Заготовка картофеля"), 10);
    Add(TEXT("Drink"), NSLOCTEXT("Tablet", "Drink", "Основа напитка"), 8);
}

void UTabletShopComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority()) Balance = FMath::Max(0, StartingBalance);
}

void UTabletShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UTabletShopComponent, Balance);
    DOREPLIFETIME(UTabletShopComponent, Products);
}

void UTabletShopComponent::Purchase(FName ProductId, int32 Quantity)
{
    ServerPurchase(ProductId, Quantity);
}

bool UTabletShopComponent::CreditSale(int32 Amount)
{
    if (!GetOwner()->HasAuthority() || Amount < 0 || Amount > 100000) return false;
    Balance = int32(FMath::Min(int64(MAX_int32), int64(Balance) + Amount));
    GetOwner()->ForceNetUpdate();
    return true;
}

void UTabletShopComponent::ServerPurchase_Implementation(FName ProductId, int32 Quantity)
{
    FTabletOrderItem Item; Item.ProductId = ProductId; Item.Quantity = Quantity;
    ServerPurchaseCart_Implementation({Item});
}

void UTabletShopComponent::PurchaseCart(const TArray<FTabletOrderItem>& Items)
{
    ServerPurchaseCart(Items);
}

void UTabletShopComponent::ServerPurchaseCart_Implementation(const TArray<FTabletOrderItem>& Items)
{
    int64 Cost = 0;
    TSet<FName> Seen;
    if (Items.IsEmpty() || Items.Num() > Products.Num())
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "InvalidCart", "Недопустимый состав заказа."));
        return;
    }
    for (const FTabletOrderItem& Item : Items)
    {
        const FTabletProduct* Product = Products.FindByPredicate([&Item](const FTabletProduct& P) { return P.Id == Item.ProductId; });
        if (!Product || Product->Price <= 0 || Item.Quantity < 1 || Item.Quantity > 100 || Seen.Contains(Item.ProductId))
        {
            ClientPurchaseResult(false, NSLOCTEXT("Tablet", "InvalidOrder", "Недопустимый заказ."));
            return;
        }
        Seen.Add(Item.ProductId);
        Cost += int64(Product->Price) * Item.Quantity;
    }
    if (Cost > Balance)
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "NoMoney", "Недостаточно денег."));
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastPurchaseTime < 0.3)
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "TooFast", "Подождите перед следующим заказом."));
        return;
    }
    LastPurchaseTime = Now;
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn)
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "NoPawn", "Доставка недоступна: нет персонажа."));
        return;
    }
    FVector Location;
    bool bFoundPoint = false;
    const AGameStateBase* State = GetWorld()->GetGameState();
    const int32 Slot = State ? State->PlayerArray.IndexOfByKey(Controller->PlayerState) : INDEX_NONE;
    for (TActorIterator<ATabletDeliveryPoint> It(GetWorld()); It; ++It)
    {
        if (It->PlayerSlot == Slot)
        {
            Location = It->GetActorLocation() + FVector(0, 0, 45);
            bFoundPoint = true;
            break;
        }
    }
    if (!bFoundPoint)
    {
        // Prototype fallback: first order establishes a fixed delivery area.
        if (!bHasFallbackLocation)
        {
            FHitResult Floor;
            const FVector Probe = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 250;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(Pawn);
            if (!GetWorld()->LineTraceSingleByChannel(Floor, Probe, Probe - FVector(0, 0, 1000), ECC_Visibility, Params))
            {
                ClientPurchaseResult(false, NSLOCTEXT("Tablet", "NoFloor", "Нет места доставки. Добавьте TabletDeliveryPoint на карту."));
                return;
            }
            FallbackDeliveryLocation = Floor.ImpactPoint + FVector(0, 0, 25);
            bHasFallbackLocation = true;
        }
        Location = FallbackDeliveryLocation;
    }
    FActorSpawnParameters Spawn;
    Spawn.Owner = Controller;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    TArray<ATabletDeliveryBox*> Deliveries;
    for (const FTabletOrderItem& Item : Items)
    {
        ATabletDeliveryBox* Box = nullptr;
        for (int32 Index = 0; Index < 25 && !Box; ++Index)
        {
            const FVector Offset((Index % 5 - 2) * 55, (Index / 5 - 2) * 55, 0);
            Box = GetWorld()->SpawnActor<ATabletDeliveryBox>(Location + Offset, FRotator::ZeroRotator, Spawn);
        }
        if (!Box)
        {
            // Atomic delivery: undo partial spawns before returning an error.
            for (ATabletDeliveryBox* Created : Deliveries) Created->Destroy();
            ClientPurchaseResult(false, NSLOCTEXT("Tablet", "Full", "Место доставки занято. Освободите площадку."));
            return;
        }
        const FTabletProduct& Product = *Products.FindByPredicate([&Item](const FTabletProduct& P) { return P.Id == Item.ProductId; });
        Box->Initialize(Product.Id, Product.Name, Item.Quantity);
        Deliveries.Add(Box);
    }
    Balance -= int32(Cost);
    ClientPurchaseResult(true, FText::Format(NSLOCTEXT("Tablet", "CartDelivered", "Заказ доставлен. Коробок: {0}. Списано {1}."), FText::AsNumber(Deliveries.Num()), FText::AsNumber(Cost)));
}

void UTabletShopComponent::ClientPurchaseResult_Implementation(bool bSuccess, const FText& Message)
{
    LastPurchaseMessage = Message;
    bLastPurchaseSuccessful = bSuccess;
    OnPurchaseResult.Broadcast(bSuccess, Message);
}
