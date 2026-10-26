#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "LetThemCookPlayerState.generated.h"

UCLASS()
class LETTHEMCOOK_API ALetThemCookPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxDeliveryPlayers = 4;

    UFUNCTION(BlueprintPure, Category="Delivery")
    int32 GetDeliverySlot() const { return DeliverySlot; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    friend class ALetThemCookGameMode;

    void SetDeliverySlot(int32 Slot);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Delivery", meta=(AllowPrivateAccess="true"))
    int32 DeliverySlot = 0;
};
