#pragma once

#include "CoreMinimal.h"
#include "InteractableComponent.h"
#include "SimpleInteractableComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LETTHEMCOOK_API USimpleInteractableComponent : public UInteractableComponent
{
    GENERATED_BODY()

public:
    virtual void Interact_Implementation(UInteractorComponent* Interactor) override;
};
