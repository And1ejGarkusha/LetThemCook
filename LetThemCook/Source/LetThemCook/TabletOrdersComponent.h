#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TabletOrdersComponent.generated.h"

UENUM(BlueprintType)
enum class ETabletDish : uint8 { Burger, Fries, Drink };

USTRUCT(BlueprintType)
struct FTabletBurgerComposition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIceberg = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPickle = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTomato = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCheese = true;
};

USTRUCT(BlueprintType)
struct FTabletDishItem
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ETabletDish Dish = ETabletDish::Burger;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTabletBurgerComposition Burger;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="20")) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FTabletCustomerOrder
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid Id;
    UPROPERTY(BlueprintReadOnly) int32 Number = 0;
    UPROPERTY(BlueprintReadOnly) ETabletDish Dish = ETabletDish::Burger;
    UPROPERTY(BlueprintReadOnly) FTabletBurgerComposition Burger;
    UPROPERTY(BlueprintReadOnly) TArray<FTabletDishItem> Items;
    UPROPERTY(BlueprintReadOnly) int32 Payment = 0;
    UPROPERTY(BlueprintReadOnly) double Deadline = 0;
};

USTRUCT()
struct FTabletOrdersState
{
    GENERATED_BODY()
    UPROPERTY() bool bReady = false;
    UPROPERTY() int32 Revision = 0;
    UPROPERTY() TArray<FTabletCustomerOrder> Orders;
};

struct FTabletRecipePart
{
    FName Artwork;
    FText Name;
    FText Description;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTabletOrdersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTabletOrderExpired, const FTabletCustomerOrder&, Order);

/** Per-player order inbox. Customer gameplay submits/removes orders on the server. */
UCLASS(ClassGroup=(Tablet), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UTabletOrdersComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTabletOrdersComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Call when the customer reaches the order counter, after the queue wait.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Tablet|Orders")
    FGuid SubmitOrder(ETabletDish Dish, FTabletBurgerComposition Burger, int32 Payment, float PatienceSeconds = 180.f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Tablet|Orders")
    FGuid SubmitMealOrder(const TArray<FTabletDishItem>& Items, int32 Payment, float PatienceSeconds = 180.f);

    /** Mirror a cafe order into another team member's inbox with the same ID and deadline. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Tablet|Orders")
    bool PublishCustomerOrder(const FTabletCustomerOrder& Order);

    // Customer gameplay validates the served food and handles payment before removing the order.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Tablet|Orders")
    bool RemoveOrder(FGuid OrderId);

    UFUNCTION(BlueprintPure, Category="Tablet|Orders")
    TArray<FTabletCustomerOrder> GetActiveOrders() const { return State.Orders; }

    UFUNCTION(BlueprintPure, Category="Tablet|Orders")
    int32 GetSecondsRemaining(const FTabletCustomerOrder& Order) const;

    UFUNCTION(BlueprintCallable, Category="Tablet|Orders") void RefreshOrders();
    UPROPERTY(BlueprintAssignable, Category="Tablet|Orders") FTabletOrdersChanged OnOrdersChanged;
    UPROPERTY(BlueprintAssignable, Category="Tablet|Orders") FTabletOrderExpired OnOrderExpired;
    FSimpleMulticastDelegate OnChanged;

    bool IsReady() const { return State.bReady; }
    const TArray<FTabletCustomerOrder>& GetOrders() const { return State.Orders; }
    const FTabletCustomerOrder* FindOrder(FGuid Id) const;
    class UTexture2D* GetArtwork(FName Key) const;
    static FText DishName(ETabletDish Dish);
    static FText OrderTitle(const FTabletCustomerOrder& Order);
    static TArray<FTabletDishItem> GetItems(const FTabletCustomerOrder& Order);
    static FText ItemDescription(const FTabletDishItem& Item);
    static TArray<FTabletRecipePart> ItemRecipe(const FTabletDishItem& Item);
    static FText Description(const FTabletCustomerOrder& Order);
    static TArray<FTabletRecipePart> Recipe(const FTabletCustomerOrder& Order);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(ReplicatedUsing=OnRep_State) FTabletOrdersState State;
    UPROPERTY() TMap<FName, TObjectPtr<class UTexture2D>> Artwork;
    UFUNCTION() void OnRep_State();
    UFUNCTION(Server, Reliable) void ServerRefreshOrders();
private:
    double ServerTime() const;
    void ExpireOrders();
    void NotifyChanged();
    FTimerHandle ExpiryTimer;
    int32 NextNumber = 1;
    double LastRefreshTime = -1;
};
