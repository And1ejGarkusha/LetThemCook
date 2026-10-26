#pragma once

#include "CoreMinimal.h"
#include "FoodProductDefinition.generated.h"

class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct LETTHEMCOOK_API FFoodProductDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food")
    FName Id;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food", meta=(ClampMin="1"))
    int32 Price = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food")
    TObjectPtr<UStaticMesh> BoxMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food", meta=(ClampMin="0.01"))
    FVector MeshScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food")
    bool bOverrideCollisionExtent = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food", meta=(EditCondition="bOverrideCollisionExtent", ClampMin="1"))
    FVector CollisionExtent = FVector(25);
};
