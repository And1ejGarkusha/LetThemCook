#pragma once

#include "CoreMinimal.h"

class ADeliveryBox;
class UFoodCatalog;
class UWorld;

enum class EDeliveryOrderResult : uint8
{
    Success,
    NoPallets,
    NotEnoughSpace,
    InvalidProduct,
    SpawnFailed
};

class LETTHEMCOOK_API FDeliveryOrder
{
public:
    static EDeliveryOrderResult TryDeliver(UWorld* World, UFoodCatalog* Catalog,
        TSubclassOf<ADeliveryBox> BoxClass, int32 PlayerSlot, const TArray<FName>& Products);
};
