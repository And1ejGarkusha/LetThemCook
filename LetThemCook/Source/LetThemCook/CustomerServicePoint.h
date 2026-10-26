#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TabletOrdersComponent.h"
#include "CustomerServicePoint.generated.h"

class ACustomerNPC;
class APlayerState;
class APlayerController;
class ACustomerServicePoint;
class UBoxComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCustomerQueueChanged, ACustomerServicePoint*, Counter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCustomerOrderCreated, ACustomerNPC*, Customer, const FTabletCustomerOrder&, Order);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCustomerServed, ACustomerNPC*, Customer, APlayerController*, Player, int32, Payment);

/** Place at the front of a cafe queue. Local -X is the default direction of the queue. */
UCLASS()
class LETTHEMCOOK_API ACustomerServicePoint : public AActor
{
    GENERATED_BODY()
public:
    ACustomerServicePoint();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Customers") FName CafeId = TEXT("Cafe");
    /** Used when AssignedPlayers is empty. Same slot convention as TabletDeliveryPoint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Customers", meta=(ClampMin="0")) int32 PlayerSlot = 0;
    /** Lobby/game mode assigns the team here. First player owns the cafe wallet; all see orders. */
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Customers") TArray<TObjectPtr<APlayerState>> AssignedPlayers;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") void SetServingPlayers(const TArray<APlayerState*>& Players);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Queue", meta=(ClampMin="1", ClampMax="20")) int32 QueueCapacity = 5;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Customers|Queue") TArray<TObjectPtr<AActor>> QueuePoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Queue", meta=(ClampMin="100")) float QueueSpacing = 140.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Orders", meta=(ClampMin="1", ClampMax="180")) float PatienceSeconds = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Customers|Economy", meta=(ClampMin="0", ClampMax="100")) float Reputation = 50.f;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Customers|Economy") int64 TotalRevenue = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Economy", meta=(ClampMin="0")) float ReputationGain = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Economy", meta=(ClampMin="0")) float ReputationLoss = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Serving", meta=(ClampMin="50", ClampMax="500")) float ServingDistance = 300.f;
    /** Editor marker for the counter's serving area. Hand-off is initiated by Enhanced Input. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Customers|Serving") TObjectPtr<UBoxComponent> ServingZone;
    UPROPERTY(BlueprintAssignable, Category="Customers") FCustomerQueueChanged OnQueueChanged;
    UPROPERTY(BlueprintAssignable, Category="Customers") FCustomerOrderCreated OnOrderCreated;
    UPROPERTY(BlueprintAssignable, Category="Customers") FCustomerServed OnCustomerServed;
    UFUNCTION(BlueprintPure, Category="Customers") TArray<ACustomerNPC*> GetQueue() const;
    UFUNCTION(BlueprintPure, Category="Customers") ACustomerNPC* GetCurrentCustomer() const;
    UFUNCTION(BlueprintPure, Category="Customers") bool CanAcceptCustomer() const;
    UFUNCTION(BlueprintPure, Category="Customers") bool CanPlayerServe(APlayerController* Player) const;
    /** Server-side entry point for the interaction component/cooker. No payment for incorrect food. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") bool TryServeMeal(APlayerController* Player, AActor* Food);
    bool Enqueue(ACustomerNPC* Customer);
    void RemoveCustomer(ACustomerNPC* Customer);
    void CustomerReachedSlot(ACustomerNPC* Customer);
    void ExpireCustomer(ACustomerNPC* Customer);
    float GetSelectionWeight() const;
    FVector GetQueueLocation(int32 Index) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Queue) TArray<TObjectPtr<ACustomerNPC>> Queue;
    TArray<TWeakObjectPtr<UTabletOrdersComponent>> PublishedInboxes;
    FGuid PublishedOrderId;
    FTimerHandle SyncTimer;
    bool bClosing = false;
    TArray<APlayerController*> GetServingControllers() const;
    void UpdateQueue();
    void SyncOrderInboxes();
    void ClearPublishedOrder();
    UFUNCTION() void OnRep_Queue();
};
