#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TabletDelivery.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Place one in each kitchen. PlayerSlot follows the server's PlayerArray order. */
UCLASS()
class LETTHEMCOOK_API ATabletDeliveryPoint : public AActor
{
    GENERATED_BODY()
public:
    ATabletDeliveryPoint();
    UPROPERTY(EditAnywhere, Category="Delivery", meta=(ClampMin="0")) int32 PlayerSlot = 0;
};

/** One physical container per purchase, retaining product and quantity. */
UCLASS()
class LETTHEMCOOK_API ATabletDeliveryBox : public AActor
{
    GENERATED_BODY()
public:
    ATabletDeliveryBox();
    void Initialize(FName InProductId, const FText& InProductName, int32 InQuantity);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Contents, Category="Delivery") FName ProductId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Contents, Category="Delivery") FText ProductName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Contents, Category="Delivery") int32 Quantity = 0;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UFUNCTION() void OnRep_Contents();
};
