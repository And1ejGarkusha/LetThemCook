// Copyright Epic Games, Inc. All Rights Reserved.

#include "LetThemCookGameMode.h"
#include "Players/LetThemCookPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ALetThemCookGameMode::ALetThemCookGameMode()
{
    PlayerStateClass = ALetThemCookPlayerState::StaticClass();
}

void ALetThemCookGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    ALetThemCookPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<ALetThemCookPlayerState>() : nullptr;
    if (!PlayerState || PlayerState->IsOnlyASpectator() || PlayerState->GetDeliverySlot() != 0) return;

    TSet<int32> UsedSlots;
    for (FConstPlayerControllerIterator Player = GetWorld()->GetPlayerControllerIterator(); Player; ++Player)
    {
        const APlayerController* Controller = Player->Get();
        if (!IsValid(Controller) || Controller == NewPlayer || Controller->IsActorBeingDestroyed()) continue;
        if (const ALetThemCookPlayerState* OtherState = Controller->GetPlayerState<ALetThemCookPlayerState>())
        {
            UsedSlots.Add(OtherState->GetDeliverySlot());
        }
    }

    for (int32 Slot = 1; Slot <= ALetThemCookPlayerState::MaxDeliveryPlayers; ++Slot)
    {
        if (!UsedSlots.Contains(Slot))
        {
            PlayerState->SetDeliverySlot(Slot);
            return;
        }
    }
}
