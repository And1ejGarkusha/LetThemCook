#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TabletOrdersComponent.h"
#include "CustomerNPC.generated.h"

class ACustomerServicePoint;
class UBlendSpace;
UENUM(BlueprintType)
enum class ECustomerState : uint8 { Arriving, InQueue, WaitingForFood, Leaving };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCustomerStateChanged, ECustomerState, State);

/** Server-controlled customer; movement, appearance and order state replicate to spectators/players. */
UCLASS()
class LETTHEMCOOK_API ACustomerNPC : public ACharacter
{
    GENERATED_BODY()
public:
    ACustomerNPC();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_State, Category="Customers") ECustomerState State = ECustomerState::Arriving;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Customers") TObjectPtr<ACustomerServicePoint> ServicePoint;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Customers") FTabletCustomerOrder Order;
    UPROPERTY(BlueprintAssignable, Category="Customers") FCustomerStateChanged OnStateChanged;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Movement", meta=(ClampMin="5", ClampMax="80")) float ArrivalRadius = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Movement", meta=(ClampMin="5")) float NavigationTimeout = 30.f;
    /** Optional queue patience, separate from the food deadline. Zero disables it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Movement", meta=(ClampMin="0")) float MaximumQueueWait = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Appearance") TObjectPtr<UBlendSpace> Locomotion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Appearance", meta=(ClampMin="1")) float AnimationReferenceSpeed = 400.f;
    bool InitializeCustomer(ACustomerServicePoint* Point, const FVector& Exit, const TArray<FTabletDishItem>& Meal, int32 Payment);
    void MoveToQueue(const FVector& Location, bool bAtFront);
    void WaitForFood(const FTabletCustomerOrder& InOrder);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") void Leave();
    const TArray<FTabletDishItem>& GetRequestedItems() const { return RequestedItems; }
    int32 GetRequestedPayment() const { return RequestedPayment; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TArray<FTabletDishItem> RequestedItems;
    int32 RequestedPayment = 0;
    FVector ExitLocation = FVector::ZeroVector;
    FVector Destination = FVector::ZeroVector;
    double MoveStartedAt = 0;
    double QueueEnteredAt = 0;
    double NextMoveAttempt = 0;
    bool bMoving = false;
    bool bFront = false;
    int32 FailedMoveAttempts = 0;
    void SetState(ECustomerState NewState);
    void StartMove(const FVector& Location);
    void RequestMove();
    UFUNCTION() void OnRep_State();
};
