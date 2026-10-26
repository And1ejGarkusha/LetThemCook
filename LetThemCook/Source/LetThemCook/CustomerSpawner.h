#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TabletOrdersComponent.h"
#include "CustomerSpawner.generated.h"

class ACustomerNPC;
class ACustomerServicePoint;
USTRUCT(BlueprintType)
struct FCustomerMenuOption
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTabletDishItem> Items;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="100000")) int32 Payment = 160;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float Weight = 1.f;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCustomerSpawned, ACustomerNPC*, Customer);

/** Authority-only weighted customer spawning. Assign cafe counters in DestinationPool. */
UCLASS()
class LETTHEMCOOK_API ACustomerSpawner : public AActor
{
    GENERATED_BODY()
public:
    ACustomerSpawner();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers") TSubclassOf<ACustomerNPC> CustomerClass;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Customers|Points") TArray<TObjectPtr<ACustomerServicePoint>> DestinationPool;
    /** Empty SpawnPoints uses the spawner's position. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Customers|Points") TArray<TObjectPtr<AActor>> SpawnPoints;
    /** Empty ExitPoints sends each customer back to its spawn position. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Customers|Points") TArray<TObjectPtr<AActor>> ExitPoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Spawning") bool bAutoStart = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Spawning", meta=(ClampMin="0")) float InitialDelay = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Spawning", meta=(ClampMin="0.5")) float SpawnInterval = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Spawning", meta=(ClampMin="1", ClampMax="100")) int32 MaxAlive = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Menu") TArray<FCustomerMenuOption> Menu;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Menu") bool bRandomizeBurgerComposition = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Menu", meta=(ClampMin="0", ClampMax="1")) float OmitIngredientChance = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customers|Spawning") int32 RandomSeed = 0;
    UPROPERTY(BlueprintAssignable, Category="Customers") FCustomerSpawned OnCustomerSpawned;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") void StartSpawning();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") void StopSpawning();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers") ACustomerNPC* SpawnCustomer();
    UFUNCTION(BlueprintPure, Category="Customers") int32 GetAliveCount() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TArray<TWeakObjectPtr<ACustomerNPC>> ActiveCustomers;
    FRandomStream Random;
    FTimerHandle SpawnTimer;
    void SpawnNext();
};
