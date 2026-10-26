#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TabletShopComponent.generated.h"

USTRUCT(BlueprintType)
struct FTabletProduct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 Price = 1;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FTabletPurchaseResult, bool, const FText&);

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
    const TArray<FTabletProduct>& GetProducts() const { return Products; }
    int32 GetBalance() const { return Balance; }
    const FText& GetLastPurchaseMessage() const { return LastPurchaseMessage; }
    bool WasLastPurchaseSuccessful() const { return bLastPurchaseSuccessful; }
    void Purchase(FName ProductId, int32 Quantity);
    void PurchaseCart(const TArray<FTabletOrderItem>& Items);
    UFUNCTION(BlueprintPure, Category="Tablet|Economy") int32 GetWalletBalance() const { return Balance; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Tablet|Economy") bool CreditSale(int32 Amount);
    FTabletPurchaseResult OnPurchaseResult;

protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditDefaultsOnly, Category="Tablet", meta=(ClampMin="0")) int32 StartingBalance = 500;
    UPROPERTY(EditDefaultsOnly, Replicated, Category="Tablet") TArray<FTabletProduct> Products;
    UPROPERTY(Replicated) int32 Balance = 0;
    UFUNCTION(Server, Reliable) void ServerPurchase(FName ProductId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerPurchaseCart(const TArray<FTabletOrderItem>& Items);
    // Hard references keep native Slate artwork included in packaged builds.
    UPROPERTY() TObjectPtr<class UTexture2D> IngredientsArtwork;
    UPROPERTY() TObjectPtr<class UTexture2D> UpgradesArtwork;
    UFUNCTION(Client, Reliable) void ClientPurchaseResult(bool bSuccess, const FText& Message);
private:
    FText LastPurchaseMessage;
    bool bLastPurchaseSuccessful = true;
    double LastPurchaseTime = -1.0;
    FVector FallbackDeliveryLocation = FVector::ZeroVector;
    bool bHasFallbackLocation = false;
};
