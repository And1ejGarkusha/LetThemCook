#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Food/Data/FoodProductDefinition.h"
#include "TabletShopComponent.generated.h"

class UFoodCatalog;
class ADeliveryBox;

DECLARE_MULTICAST_DELEGATE_TwoParams(FTabletPurchaseResult, bool, const FText&);
DECLARE_MULTICAST_DELEGATE(FTabletCatalogChanged);

USTRUCT()
struct FTabletOrderItem
{
    GENERATED_BODY()
    UPROPERTY() FName ProductId;
    UPROPERTY() int32 Quantity = 0;
};

/** Server owns the wallet, validates orders and creates deliveries. */
UCLASS(ClassGroup=(Tablet), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UTabletShopComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTabletShopComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintPure, Category="Tablet")
    const TArray<FFoodProductDefinition>& GetProducts() const;
    UFUNCTION(BlueprintPure, Category="Tablet")
    int32 GetBalance() const { return Balance; }
    UFUNCTION(BlueprintPure, Category="Tablet")
    const FText& GetLastPurchaseMessage() const { return LastPurchaseMessage; }
    UFUNCTION(BlueprintPure, Category="Tablet")
    bool WasLastPurchaseSuccessful() const { return bLastPurchaseSuccessful; }
    void Purchase(FName ProductId, int32 Quantity);
    void PurchaseCart(const TArray<FTabletOrderItem>& Items);
    FTabletPurchaseResult OnPurchaseResult;
    FTabletCatalogChanged OnCatalogChanged;

protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditDefaultsOnly, Category="Tablet", meta=(ClampMin="0")) int32 StartingBalance = 500;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Catalog, Category="Tablet")
    TObjectPtr<UFoodCatalog> Catalog;
    UPROPERTY(EditDefaultsOnly, Category="Delivery")
    TSubclassOf<ADeliveryBox> DeliveryBoxClass;
    UPROPERTY(Replicated) int32 Balance = 0;
    UFUNCTION(Server, Reliable) void ServerPurchase(FName ProductId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerPurchaseCart(const TArray<FTabletOrderItem>& Items);
    // Hard references keep native Slate artwork included in packaged builds.
    UPROPERTY() TObjectPtr<class UTexture2D> IngredientsArtwork;
    UPROPERTY() TObjectPtr<class UTexture2D> UpgradesArtwork;
    UFUNCTION(Client, Reliable) void ClientPurchaseResult(bool bSuccess, const FText& Message);
private:
    UFUNCTION()
    void OnRep_Catalog();

    FText LastPurchaseMessage;
    bool bLastPurchaseSuccessful = true;
    double LastPurchaseTime = -1.0;
    bool bProcessingOrder = false;
};
