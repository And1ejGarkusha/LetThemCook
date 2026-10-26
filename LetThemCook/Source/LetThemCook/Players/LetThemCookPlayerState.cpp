#include "LetThemCookPlayerState.h"
#include "Net/UnrealNetwork.h"

void ALetThemCookPlayerState::SetDeliverySlot(int32 Slot)
{
    if (!HasAuthority()) return;
    DeliverySlot = FMath::Clamp(Slot, 0, MaxDeliveryPlayers);
    ForceNetUpdate();
}

void ALetThemCookPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALetThemCookPlayerState, DeliverySlot);
}
