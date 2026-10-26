#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Delivery/DeliveryBox.h"
#include "Delivery/DeliveryPallet.h"
#include "Food/Data/FoodCatalog.h"
#include "Interaction/GrabbableComponent.h"
#include "Players/LetThemCookPlayerState.h"
#include "TabletShopComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

namespace
{
struct FDeliveryOrderTestState
{
    FAutomationTestBase* Test = nullptr;
    UWorld* Server = nullptr;
    UWorld* Client = nullptr;
    UTabletShopComponent* HostShop = nullptr;
    UTabletShopComponent* RemoteShop = nullptr;
    UTabletShopComponent* ClientShop = nullptr;
    UFoodCatalog* Catalog = nullptr;
    TSubclassOf<ADeliveryBox> BoxClass;
    TArray<ADeliveryPallet*> HostPallets;
    TArray<ADeliveryPallet*> RemotePallets;
    ADeliveryPallet* FreedPallet = nullptr;
    ADeliveryBox* MovedBox = nullptr;
    AActor* Blocker = nullptr;
    FTransform OriginalBoxTransform;
    FTransform SecondPalletTransform;
    double LastStepTime = 0;
    int32 Stage = 0;

    bool Check(bool Condition, const TCHAR* Message)
    {
        return Test->TestTrue(FString::Printf(TEXT("Stage %d: %s"), Stage, Message), Condition);
    }

    TArray<ADeliveryBox*> Boxes(UWorld* World) const
    {
        TArray<ADeliveryBox*> Result;
        for (TActorIterator<ADeliveryBox> Box(World); Box; ++Box)
            if (!Box->IsActorBeingDestroyed()) Result.Add(*Box);
        return Result;
    }

    ADeliveryPallet* NearestPallet(const ADeliveryBox* Box) const
    {
        ADeliveryPallet* Nearest = nullptr;
        double Distance = TNumericLimits<double>::Max();
        for (TActorIterator<ADeliveryPallet> Pallet(Server); Pallet; ++Pallet)
        {
            const double Candidate = FVector::DistSquared(Box->GetActorLocation(), Pallet->GetActorLocation());
            if (Candidate < Distance) { Nearest = *Pallet; Distance = Candidate; }
        }
        return Nearest;
    }

    bool CanSpawn(ADeliveryPallet* Pallet) const
    {
        return Pallet->CanSpawnProduct(Catalog, TEXT("Bun"), BoxClass);
    }

    bool Wallets(int32 HostBalance, int32 RemoteBalance)
    {
        return Test->TestEqual(TEXT("Host wallet"), HostShop->GetBalance(), HostBalance)
            && Test->TestEqual(TEXT("Remote server wallet"), RemoteShop->GetBalance(), RemoteBalance)
            && Test->TestEqual(TEXT("Replicated client wallet"), ClientShop->GetBalance(), RemoteBalance);
    }

    void DestroyRemoteBoxes()
    {
        for (ADeliveryBox* Box : Boxes(Server))
            if (NearestPallet(Box)->PlayerSlot == 2) Box->Destroy();
    }

    bool RunStage()
    {
        switch (Stage)
        {
        case 0:
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || World->WorldType != EWorldType::PIE) continue;
                if (World->GetNetMode() == NM_ListenServer) Server = World;
                if (World->GetNetMode() == NM_Client) Client = World;
            }
            if (!Check(Server && Client, TEXT("Run PIE as listen server with two players"))) return false;
            Catalog = LoadObject<UFoodCatalog>(nullptr, TEXT("/Game/DataAssets/Food/DA_FoodCatalog.DA_FoodCatalog"));
            BoxClass = LoadClass<ADeliveryBox>(nullptr, TEXT("/Game/Delivery/Blueprints/BP_DeliveryBox.BP_DeliveryBox_C"));
            for (FConstPlayerControllerIterator Player = Server->GetPlayerControllerIterator(); Player; ++Player)
            {
                APlayerController* Controller = Player->Get();
                UTabletShopComponent* Shop = Controller->FindComponentByClass<UTabletShopComponent>();
                if (Controller->IsLocalController()) HostShop = Shop; else RemoteShop = Shop;
            }
            ClientShop = Client->GetFirstPlayerController()->FindComponentByClass<UTabletShopComponent>();
            if (!Check(Catalog && BoxClass && HostShop && RemoteShop && ClientShop, TEXT("Catalog, shared box and both shops exist"))) return false;
            if (!Check(Boxes(Server).IsEmpty() && Wallets(500, 500), TEXT("No startup boxes or charges"))) return false;
            const ALetThemCookPlayerState* HostState = Cast<APlayerController>(HostShop->GetOwner())->GetPlayerState<ALetThemCookPlayerState>();
            const ALetThemCookPlayerState* RemoteState = Cast<APlayerController>(RemoteShop->GetOwner())->GetPlayerState<ALetThemCookPlayerState>();
            if (!Check(HostState && RemoteState && HostState->GetDeliverySlot() == 1 && RemoteState->GetDeliverySlot() == 2,
                TEXT("Host receives slot 1 and second player receives slot 2"))) return false;
            for (TActorIterator<ADeliveryPallet> Pallet(Server); Pallet; ++Pallet)
            {
                if (Pallet->PlayerSlot == 1) HostPallets.Add(*Pallet);
                if (Pallet->PlayerSlot == 2) RemotePallets.Add(*Pallet);
            }
            if (!Check(HostPallets.Num() == 2 && RemotePallets.Num() == 2, TEXT("Test map has two pallets per player"))) return false;
            for (int32 Slot = 1; Slot <= 2; ++Slot)
            {
                const TArray<ADeliveryPallet*>& Pallets = Slot == 1 ? HostPallets : RemotePallets;
                for (int32 Index = 0; Index < Pallets.Num(); ++Index)
                {
                    Pallets[Index]->FindComponentByClass<UStaticMeshComponent>()->SetMobility(EComponentMobility::Movable);
                    Pallets[Index]->SetActorLocation(FVector(6000 + Slot * 1000, 6000 + Index * 400, 500), false, nullptr, ETeleportType::TeleportPhysics);
                    if (!Check(CanSpawn(Pallets[Index]), TEXT("Relocated test pallet is clear"))) return false;
                }
            }
            FTabletOrderItem Bun; Bun.ProductId = TEXT("Bun"); Bun.Quantity = 1;
            FTabletOrderItem Pickle; Pickle.ProductId = TEXT("Pickle"); Pickle.Quantity = 1;
            HostShop->PurchaseCart({Bun, Pickle});
            ClientShop->Purchase(TEXT("Cheese"), 2);
            return true;
        }
        case 1:
        {
            if (!Wallets(489, 486) || !Check(Boxes(Server).Num() == 4 && Boxes(Client).Num() == 4, TEXT("Two simultaneous orders create four replicated boxes"))) return false;
            int32 Buns = 0, Pickles = 0, Cheese = 0;
            for (ADeliveryBox* Box : Boxes(Server))
            {
                const FName Product = Box->GetProductId();
                const int32 Slot = NearestPallet(Box)->PlayerSlot;
                if (!Check((Product == TEXT("Cheese") && Slot == 2) || (Product != TEXT("Cheese") && Slot == 1), TEXT("Each product is on its buyer's pallet"))) return false;
                Buns += Product == TEXT("Bun"); Pickles += Product == TEXT("Pickle"); Cheese += Product == TEXT("Cheese");
                FTransform Expected;
                if (!Check(NearestPallet(Box)->GetProductSpawnTransform(Catalog, Product, Expected)
                    && FMath::Abs(Box->GetActorLocation().Z - (Expected.GetLocation().Z - 2)) < 3,
                    TEXT("Different-height boxes rest on the pallet surface"))) return false;
                if (!Check(Box->FindComponentByClass<UGrabbableComponent>()->IsPhysicsSetupValid(), TEXT("Purchased boxes remain grabbable"))) return false;
            }
            if (!Check(Buns == 1 && Pickles == 1 && Cheese == 2, TEXT("Quantity means individual boxes, not cart lines"))) return false;
            DestroyRemoteBoxes();
            return true;
        }
        case 2:
            if (!Check(CanSpawn(RemotePallets[0]) && CanSpawn(RemotePallets[1]), TEXT("Other player's pallets are free"))) return false;
            HostShop->Purchase(TEXT("Bun"), 1);
            return true;
        case 3:
            if (!Wallets(489, 486) || !Check(!HostShop->WasLastPurchaseSuccessful() && Boxes(Server).Num() == 2, TEXT("Full own pallets reject purchase despite other player's free pallets"))) return false;
            MovedBox = Boxes(Server)[0];
            FreedPallet = NearestPallet(MovedBox);
            OriginalBoxTransform = MovedBox->GetActorTransform();
            Cast<UBoxComponent>(MovedBox->GetRootComponent())->SetSimulatePhysics(false);
            MovedBox->SetActorLocation(OriginalBoxTransform.GetLocation() + FVector(-500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
            return true;
        case 4:
            if (!Check(CanSpawn(FreedPallet) && Boxes(Server).Num() == 2, TEXT("Moving a box frees the pallet without automatic refill"))) return false;
            HostShop->Purchase(TEXT("Bun"), 2);
            return true;
        case 5:
            if (!Wallets(489, 486) || !Check(!HostShop->WasLastPurchaseSuccessful() && Boxes(Server).Num() == 2, TEXT("Whole order is rejected when only one of two boxes fits"))) return false;
            MovedBox->SetActorTransform(OriginalBoxTransform, false, nullptr, ETeleportType::TeleportPhysics);
            if (!Check(!CanSpawn(FreedPallet), TEXT("Returned box blocks the pallet"))) return false;
            HostShop->Purchase(TEXT("Bun"), 1);
            return true;
        case 6:
            if (!Wallets(489, 486) || !Check(!HostShop->WasLastPurchaseSuccessful(), TEXT("Returned box prevents purchase"))) return false;
            MovedBox->SetActorLocation(OriginalBoxTransform.GetLocation() + FVector(-500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
            HostShop->Purchase(TEXT("Bun"), 1);
            return true;
        case 7:
        {
            if (!Wallets(484, 486) || !Check(Boxes(Server).Num() == 3 && !CanSpawn(FreedPallet), TEXT("New purchase fills the freed pallet"))) return false;
            FTransform Spawn;
            RemotePallets[0]->GetProductSpawnTransform(Catalog, TEXT("Cheese"), Spawn);
            Blocker = Server->SpawnActor<AActor>();
            UBoxComponent* Collision = NewObject<UBoxComponent>(Blocker);
            Blocker->SetRootComponent(Collision);
            Collision->SetBoxExtent(FVector(10));
            Collision->SetCollisionProfileName(TEXT("BlockAll"));
            Collision->RegisterComponent();
            Blocker->SetActorLocation(Spawn.GetLocation());
            ClientShop->Purchase(TEXT("Cheese"), 2);
            return true;
        }
        case 8:
            if (!Wallets(484, 486) || !Check(!ClientShop->WasLastPurchaseSuccessful() && Boxes(Server).Num() == 3, TEXT("Foreign blocking object rejects entire client order"))) return false;
            Blocker->Destroy();
            ClientShop->Purchase(TEXT("Cheese"), 2);
            return true;
        case 9:
            if (!Wallets(484, 472) || !Check(Boxes(Server).Num() == 5, TEXT("Removing obstruction permits two-box client order"))) return false;
            DestroyRemoteBoxes();
            SecondPalletTransform = RemotePallets[1]->GetActorTransform();
            RemotePallets[1]->SetActorTransform(RemotePallets[0]->GetActorTransform(), false, nullptr, ETeleportType::TeleportPhysics);
            return true;
        case 10:
            if (!Check(CanSpawn(RemotePallets[0]) && CanSpawn(RemotePallets[1]), TEXT("Overlapping empty pallets individually pass preflight"))) return false;
            ClientShop->Purchase(TEXT("Cheese"), 2);
            return true;
        case 11:
            if (!Wallets(484, 472) || !Check(Boxes(Server).Num() == 3 && !ClientShop->WasLastPurchaseSuccessful()
                && ClientShop->GetLastPurchaseMessage().ToString().Contains(TEXT("отменён")), TEXT("Failure after first spawn rolls back all boxes without charging"))) return false;
            RemotePallets[1]->SetActorTransform(SecondPalletTransform, false, nullptr, ETeleportType::TeleportPhysics);
            Server->GetGameState()->PlayerArray.Swap(0, 1);
            ClientShop->Purchase(TEXT("Cheese"), 1);
            return true;
        case 12:
            if (!Wallets(484, 465) || !Check(Boxes(Server).Num() == 4, TEXT("Reordered PlayerArray does not change delivery ownership"))) return false;
            for (ADeliveryBox* Box : Boxes(Server))
                if (Box->GetProductId() == TEXT("Cheese") && !Check(NearestPallet(Box)->PlayerSlot == 2, TEXT("Client keeps slot 2 after PlayerArray reorder"))) return false;
            HostShop->Purchase(TEXT("Cheese"), 100);
            ClientShop->Purchase(TEXT("Cheese"), 100);
            return true;
        case 13:
            if (!Wallets(484, 465) || !Check(!HostShop->WasLastPurchaseSuccessful() && !ClientShop->WasLastPurchaseSuccessful(), TEXT("Unaffordable orders reject without charge"))) return false;
            for (ADeliveryPallet* Pallet : RemotePallets) Pallet->PlayerSlot = 4;
            ClientShop->Purchase(TEXT("Onion"), 1);
            return true;
        case 14:
            if (!Wallets(484, 465) || !Check(!ClientShop->WasLastPurchaseSuccessful() && Boxes(Server).Num() == 4
                && Boxes(Client).Num() == 4, TEXT("Missing own pallets rejects client order without fallback spawn"))) return false;
            for (ADeliveryPallet* Pallet : RemotePallets) Pallet->PlayerSlot = 2;
            return true;
        default:
            return false;
        }
    }
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRunDeliveryOrderScenario, TSharedRef<FDeliveryOrderTestState>, State);

bool FRunDeliveryOrderScenario::Update()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - State->LastStepTime < 1.2) return false;
    State->LastStepTime = Now;
    if (!State->RunStage())
    {
        UE_LOG(LogTemp, Error, TEXT("DELIVERY_ORDER_NETWORK_RESULT False stage=%d"), State->Stage);
        return true;
    }
    UE_LOG(LogTemp, Display, TEXT("DELIVERY_ORDER_NETWORK_STAGE %d passed"), State->Stage);
    if (++State->Stage == 15)
    {
        UE_LOG(LogTemp, Display, TEXT("DELIVERY_ORDER_NETWORK_RESULT True; two players; real client RPC; per-player pallets; box quantities; geometry; capacity; returned boxes; foreign obstruction; partial spawn rollback; stable slots; replicated wallets and boxes"));
        return true;
    }
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryOrderNetworkTest, "LetThemCook.Delivery.PalletOrders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FDeliveryOrderNetworkTest::RunTest(const FString& Parameters)
{
    TSharedRef<FDeliveryOrderTestState> State = MakeShared<FDeliveryOrderTestState>();
    State->Test = this;
    ADD_LATENT_AUTOMATION_COMMAND(FRunDeliveryOrderScenario(State));
    return true;
}

#endif
