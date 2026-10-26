#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryPallet.generated.h"

class ADeliveryBox;
class UBoxComponent;
class UStaticMeshComponent;
class UFoodCatalog;

UCLASS(Blueprintable)
class LETTHEMCOOK_API ADeliveryPallet : public AActor
{
    GENERATED_BODY()

public:
    ADeliveryPallet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Delivery", meta=(ClampMin="1", ClampMax="4"))
    int32 PlayerSlot = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery", meta=(ClampMin="0.1", Units="cm"))
    float SpawnClearance = 2.f;

    UFUNCTION(BlueprintPure, Category="Delivery")
    bool GetProductSpawnTransform(UFoodCatalog* Catalog, FName ProductId, FTransform& SpawnTransform) const;

    UFUNCTION(BlueprintPure, Category="Delivery")
    bool CanSpawnProduct(UFoodCatalog* Catalog, FName ProductId, TSubclassOf<ADeliveryBox> BoxClass) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<UStaticMeshComponent> PalletMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<UBoxComponent> SpawnSpace;

private:
    bool GetSurfaceBounds(FBox& Bounds) const;
};
