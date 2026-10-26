#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FoodProductDefinition.h"
#include "FoodCatalog.generated.h"

UCLASS(BlueprintType)
class LETTHEMCOOK_API UFoodCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Food")
    float GetBoxScale() const { return BoxScale; }

    const TArray<FFoodProductDefinition>& GetProducts() const { return Products; }
    const FFoodProductDefinition* FindProduct(FName ProductId) const;

    UFUNCTION(BlueprintPure, Category="Food")
    bool GetProduct(FName ProductId, FFoodProductDefinition& Product) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food", meta=(AllowPrivateAccess="true", ClampMin="0.01"))
    float BoxScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Food", meta=(AllowPrivateAccess="true", TitleProperty="{Id}: {Name}"))
    TArray<FFoodProductDefinition> Products;
};
