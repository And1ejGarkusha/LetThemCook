#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryBox.generated.h"

class UStaticMeshComponent;
class UGrabbableComponent;
class UBoxComponent;
class UFoodCatalog;
struct FFoodProductDefinition;

USTRUCT(BlueprintType)
struct LETTHEMCOOK_API FDeliveryBoxProduct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery")
    TObjectPtr<UFoodCatalog> Catalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Delivery")
    FName ProductId;
};

UCLASS(Blueprintable)
class LETTHEMCOOK_API ADeliveryBox : public AActor
{
    GENERATED_BODY()

public:
    ADeliveryBox();
    const UBoxComponent* GetCollisionBox() const { return CollisionBox; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnConstruction(const FTransform& Transform) override;

    static bool GetProductGeometry(const FFoodProductDefinition& Definition, float BoxScale, FVector& BoxExtent, FVector& MeshOffset);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Delivery", meta=(WorldContext="WorldContextObject"))
    static ADeliveryBox* SpawnProduct(const UObject* WorldContextObject, TSubclassOf<ADeliveryBox> BoxClass,
        UFoodCatalog* Catalog, FName ProductId, const FTransform& SpawnTransform);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Delivery")
    bool InitializeProduct(UFoodCatalog* Catalog, FName ProductId);

    UFUNCTION(BlueprintPure, Category="Delivery")
    FName GetProductId() const { return Product.ProductId; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Product, Category="Delivery", meta=(ExposeOnSpawn="true"))
    FDeliveryBoxProduct Product;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<UBoxComponent> CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<UGrabbableComponent> Grabbable;

private:
    UFUNCTION()
    void OnRep_Product();

    bool ApplyProduct();
};
