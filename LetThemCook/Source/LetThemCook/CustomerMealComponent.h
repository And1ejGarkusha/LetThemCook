#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TabletOrdersComponent.h"
#include "CustomerMealComponent.generated.h"

/** Attach to the actor produced by the cooker, or to a tray containing the complete meal. */
UCLASS(ClassGroup=(Customers), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API UCustomerMealComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCustomerMealComponent();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Customers|Food")
    bool InitializeMeal(const TArray<FTabletDishItem>& InItems);
    UFUNCTION(BlueprintPure, Category="Customers|Food") bool IsReady() const { return bReady && !bConsumed; }
    UFUNCTION(BlueprintPure, Category="Customers|Food") TArray<FTabletDishItem> GetMealItems() const { return Items; }
    bool Matches(const TArray<FTabletDishItem>& Requested) const;
    bool Consume();
    static bool IsValidMeal(const TArray<FTabletDishItem>& Meal);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    UPROPERTY(Replicated) TArray<FTabletDishItem> Items;
    UPROPERTY(Replicated) bool bReady = false;
    UPROPERTY(Replicated) bool bConsumed = false;
};
