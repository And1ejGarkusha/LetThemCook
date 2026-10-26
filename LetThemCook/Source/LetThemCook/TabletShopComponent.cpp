#include "TabletShopComponent.h"
#include "Delivery/DeliveryBox.h"
#include "Delivery/DeliveryOrder.h"
#include "Players/LetThemCookPlayerState.h"
#include "Food/Data/FoodCatalog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
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
    static ConstructorHelpers::FObjectFinder<UFoodCatalog> FoodCatalog(TEXT("/Game/DataAssets/Food/DA_FoodCatalog.DA_FoodCatalog"));
    Catalog = FoodCatalog.Object;
    static ConstructorHelpers::FClassFinder<ADeliveryBox> BoxBlueprint(TEXT("/Game/Delivery/Blueprints/BP_DeliveryBox"));
    DeliveryBoxClass = BoxBlueprint.Class;
}

const TArray<FFoodProductDefinition>& UTabletShopComponent::GetProducts() const
{
    static const TArray<FFoodProductDefinition> EmptyProducts;
    return IsValid(Catalog) ? Catalog->GetProducts() : EmptyProducts;
}

void UTabletShopComponent::OnRep_Catalog()
{
    OnCatalogChanged.Broadcast();
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
    DOREPLIFETIME(UTabletShopComponent, Catalog);
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
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bProcessingOrder)
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "OrderPending", "Предыдущий заказ ещё обрабатывается."));
        return;
    }
    TGuardValue<bool> OrderGuard(bProcessingOrder, true);
    const TArray<FFoodProductDefinition>& Products = GetProducts();
    int64 Cost = 0;
    TSet<FName> Seen;
    TArray<FName> OrderedBoxes;
    if (Items.IsEmpty() || Items.Num() > Products.Num())
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "InvalidCart", "Недопустимый состав заказа."));
        return;
    }
    for (const FTabletOrderItem& Item : Items)
    {
        const FFoodProductDefinition* Product = Products.FindByPredicate([&Item](const FFoodProductDefinition& P) { return P.Id == Item.ProductId; });
        if (!Product || Product->Price <= 0 || Item.Quantity < 1 || Item.Quantity > 100 || Seen.Contains(Item.ProductId))
        {
            ClientPurchaseResult(false, NSLOCTEXT("Tablet", "InvalidOrder", "Недопустимый заказ."));
            return;
        }
        Seen.Add(Item.ProductId);
        Cost += int64(Product->Price) * Item.Quantity;
        for (int32 BoxIndex = 0; BoxIndex < Item.Quantity; ++BoxIndex) OrderedBoxes.Add(Item.ProductId);
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
    const ALetThemCookPlayerState* PlayerState = Controller->GetPlayerState<ALetThemCookPlayerState>();
    if (!PlayerState || PlayerState->GetDeliverySlot() == 0)
    {
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "NoDeliverySlot", "Игроку не назначен номер для доставки."));
        return;
    }

    const EDeliveryOrderResult Result = FDeliveryOrder::TryDeliver(GetWorld(), Catalog, DeliveryBoxClass,
        PlayerState->GetDeliverySlot(), OrderedBoxes);
    switch (Result)
    {
    case EDeliveryOrderResult::NoPallets:
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "NoPallets", "На карте нет ваших паллет для доставки."));
        return;
    case EDeliveryOrderResult::NotEnoughSpace:
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "PalletsFull", "Недостаточно свободных паллет для всего заказа."));
        return;
    case EDeliveryOrderResult::InvalidProduct:
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "InvalidDeliveryProduct", "Не удалось подготовить коробки из каталога."));
        return;
    case EDeliveryOrderResult::SpawnFailed:
        ClientPurchaseResult(false, NSLOCTEXT("Tablet", "DeliveryCancelled", "Место доставки занято. Заказ отменён без списания денег."));
        return;
    case EDeliveryOrderResult::Success:
        break;
    }

    Balance -= int32(Cost);
    ClientPurchaseResult(true, FText::Format(NSLOCTEXT("Tablet", "CartDelivered", "Заказ доставлен. Коробок: {0}. Списано {1}."), FText::AsNumber(OrderedBoxes.Num()), FText::AsNumber(Cost)));
}

void UTabletShopComponent::ClientPurchaseResult_Implementation(bool bSuccess, const FText& Message)
{
    LastPurchaseMessage = Message;
    bLastPurchaseSuccessful = bSuccess;
    OnPurchaseResult.Broadcast(bSuccess, Message);
}
