#include "DeliveryOrder.h"
#include "DeliveryBox.h"
#include "DeliveryPallet.h"
#include "Food/Data/FoodCatalog.h"
#include "EngineUtils.h"
#include "Engine/World.h"

EDeliveryOrderResult FDeliveryOrder::TryDeliver(UWorld* World, UFoodCatalog* Catalog,
    TSubclassOf<ADeliveryBox> BoxClass, int32 PlayerSlot, const TArray<FName>& Products)
{
    if (!World || World->GetNetMode() == NM_Client || !IsValid(Catalog) || !BoxClass
        || PlayerSlot <= 0 || Products.IsEmpty()) return EDeliveryOrderResult::InvalidProduct;

    TArray<ADeliveryPallet*> Pallets;
    for (TActorIterator<ADeliveryPallet> Pallet(World); Pallet; ++Pallet)
    {
        if (!Pallet->IsActorBeingDestroyed() && Pallet->PlayerSlot == PlayerSlot) Pallets.Add(*Pallet);
    }
    if (Pallets.IsEmpty()) return EDeliveryOrderResult::NoPallets;
    if (Products.Num() > Pallets.Num()) return EDeliveryOrderResult::NotEnoughSpace;

    Pallets.Sort([](const ADeliveryPallet& Left, const ADeliveryPallet& Right)
    {
        return Left.GetName() < Right.GetName();
    });

    TArray<TArray<int32>> SuitablePallets;
    for (FName ProductId : Products)
    {
        const FFoodProductDefinition* Definition = Catalog->FindProduct(ProductId);
        FVector BoxExtent;
        FVector MeshOffset;
        if (!Definition || !ADeliveryBox::GetProductGeometry(*Definition, Catalog->GetBoxScale(), BoxExtent, MeshOffset))
            return EDeliveryOrderResult::InvalidProduct;

        TArray<int32>& Suitable = SuitablePallets.AddDefaulted_GetRef();
        for (int32 Index = 0; Index < Pallets.Num(); ++Index)
        {
            if (Pallets[Index]->CanSpawnProduct(Catalog, ProductId, BoxClass)) Suitable.Add(Index);
        }
    }

    TArray<int32> PalletProducts;
    PalletProducts.Init(INDEX_NONE, Pallets.Num());
    TFunction<bool(int32, TSet<int32>&)> AssignPallet = [&](int32 ProductIndex, TSet<int32>& Visited)
    {
        for (int32 PalletIndex : SuitablePallets[ProductIndex])
        {
            if (Visited.Contains(PalletIndex)) continue;
            Visited.Add(PalletIndex);
            if (PalletProducts[PalletIndex] == INDEX_NONE || AssignPallet(PalletProducts[PalletIndex], Visited))
            {
                PalletProducts[PalletIndex] = ProductIndex;
                return true;
            }
        }
        return false;
    };

    for (int32 ProductIndex = 0; ProductIndex < Products.Num(); ++ProductIndex)
    {
        TSet<int32> Visited;
        if (!AssignPallet(ProductIndex, Visited)) return EDeliveryOrderResult::NotEnoughSpace;
    }

    TArray<ADeliveryBox*> CreatedBoxes;
    for (int32 PalletIndex = 0; PalletIndex < Pallets.Num(); ++PalletIndex)
    {
        const int32 ProductIndex = PalletProducts[PalletIndex];
        if (ProductIndex == INDEX_NONE) continue;

        const FName ProductId = Products[ProductIndex];
        ADeliveryPallet* Pallet = Pallets[PalletIndex];
        FTransform SpawnTransform;
        ADeliveryBox* Box = nullptr;
        if (IsValid(Pallet) && !Pallet->IsActorBeingDestroyed()
            && Pallet->CanSpawnProduct(Catalog, ProductId, BoxClass)
            && Pallet->GetProductSpawnTransform(Catalog, ProductId, SpawnTransform))
        {
            Box = ADeliveryBox::SpawnProduct(World, BoxClass, Catalog, ProductId, SpawnTransform);
        }

        if (!Box)
        {
            for (ADeliveryBox* Created : CreatedBoxes)
            {
                if (IsValid(Created)) Created->Destroy();
            }
            return EDeliveryOrderResult::SpawnFailed;
        }
        CreatedBoxes.Add(Box);
    }

    return EDeliveryOrderResult::Success;
}
