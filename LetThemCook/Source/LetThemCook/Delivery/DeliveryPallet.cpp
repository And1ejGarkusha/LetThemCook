#include "DeliveryPallet.h"
#include "DeliveryBox.h"
#include "Food/Data/FoodCatalog.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryPallet::ADeliveryPallet()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    PalletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PalletMesh"));
    PalletMesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PalletAsset(TEXT("/Game/DataAssets/Food/SM_Create_Lid.SM_Create_Lid"));
    if (PalletAsset.Succeeded()) PalletMesh->SetStaticMesh(PalletAsset.Object);
    PalletMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PalletMesh->CanCharacterStepUpOn = ECB_No;

    SpawnSpace = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnSpace"));
    SpawnSpace->SetupAttachment(RootComponent);
    SpawnSpace->SetRelativeLocation(FVector(0, 0, 50));
    SpawnSpace->SetBoxExtent(FVector(27));
    SpawnSpace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnSpace->SetGenerateOverlapEvents(false);
    SpawnSpace->ShapeColor = FColor::Green;

}

void ADeliveryPallet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADeliveryPallet, PlayerSlot);
}

bool ADeliveryPallet::GetSurfaceBounds(FBox& Bounds) const
{
    const UStaticMesh* PalletAsset = PalletMesh->GetStaticMesh();
    if (!PalletAsset) return false;

    const FBox MeshBounds = PalletAsset->GetBoundingBox();
    const FTransform SpawnFrame(SpawnSpace->GetComponentQuat(), SpawnSpace->GetComponentLocation());
    Bounds = FBox(ForceInit);
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point(
            (Corner & 1) ? MeshBounds.Max.X : MeshBounds.Min.X,
            (Corner & 2) ? MeshBounds.Max.Y : MeshBounds.Min.Y,
            (Corner & 4) ? MeshBounds.Max.Z : MeshBounds.Min.Z);
        Bounds += SpawnFrame.InverseTransformPosition(PalletMesh->GetComponentTransform().TransformPosition(Point));
    }
    return Bounds.IsValid && !Bounds.GetExtent().ContainsNaN();
}

bool ADeliveryPallet::GetProductSpawnTransform(UFoodCatalog* Catalog, FName ProductId, FTransform& SpawnTransform) const
{
    const FFoodProductDefinition* Product = IsValid(Catalog) ? Catalog->FindProduct(ProductId) : nullptr;
    FVector BoxExtent;
    FVector MeshOffset;
    FBox SurfaceBounds;
    if (!Product || !ADeliveryBox::GetProductGeometry(*Product, Catalog->GetBoxScale(), BoxExtent, MeshOffset)
        || !GetSurfaceBounds(SurfaceBounds) || !FMath::IsFinite(SpawnClearance)) return false;

    const FVector Location = SpawnSpace->GetComponentLocation() + SpawnSpace->GetUpVector()
        * (SurfaceBounds.Max.Z + BoxExtent.Z + FMath::Max(0.1f, SpawnClearance));
    SpawnTransform = FTransform(SpawnSpace->GetComponentQuat(), Location);
    return true;
}

bool ADeliveryPallet::CanSpawnProduct(UFoodCatalog* Catalog, FName ProductId, TSubclassOf<ADeliveryBox> BoxClass) const
{
    if (!GetWorld() || !BoxClass || BoxClass->HasAnyClassFlags(CLASS_Abstract)) return false;
    const UBoxComponent* BoxCollision = BoxClass->GetDefaultObject<ADeliveryBox>()->GetCollisionBox();
    if (!BoxCollision || !BoxCollision->IsQueryCollisionEnabled()) return false;

    FTransform SpawnTransform;
    FBox SurfaceBounds;
    if (!GetProductSpawnTransform(Catalog, ProductId, SpawnTransform) || !GetSurfaceBounds(SurfaceBounds)) return false;

    const FVector ReservedExtent = SpawnSpace->GetScaledBoxExtent();
    const FVector SurfaceExtent = SurfaceBounds.GetExtent();
    const FVector OccupancyExtent(FMath::Max(SurfaceExtent.X, ReservedExtent.X),
        FMath::Max(SurfaceExtent.Y, ReservedExtent.Y), FMath::Max(1.0, ReservedExtent.Z));
    const FTransform SpawnFrame(SpawnSpace->GetComponentQuat(), SpawnSpace->GetComponentLocation());
    FVector OccupancyCenter = SurfaceBounds.GetCenter();
    OccupancyCenter.Z = SurfaceBounds.Max.Z + OccupancyExtent.Z + 0.1;
    const FName CollisionProfile = BoxCollision->GetCollisionProfileName();
    if (GetWorld()->OverlapBlockingTestByProfile(SpawnFrame.TransformPosition(OccupancyCenter),
        SpawnFrame.GetRotation(), CollisionProfile, FCollisionShape::MakeBox(OccupancyExtent))) return false;

    FVector BoxExtent;
    FVector MeshOffset;
    const FFoodProductDefinition* Product = Catalog->FindProduct(ProductId);
    return Product && ADeliveryBox::GetProductGeometry(*Product, Catalog->GetBoxScale(), BoxExtent, MeshOffset)
        && !GetWorld()->OverlapBlockingTestByProfile(SpawnTransform.GetLocation(), SpawnTransform.GetRotation(),
            CollisionProfile, FCollisionShape::MakeBox(BoxExtent));
}
