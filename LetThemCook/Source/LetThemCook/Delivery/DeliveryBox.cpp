#include "DeliveryBox.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/GrabbableComponent.h"
#include "Food/Data/FoodCatalog.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryBox::ADeliveryBox()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    SetRootComponent(CollisionBox);
    CollisionBox->SetBoxExtent(FVector(25));
    CollisionBox->SetMobility(EComponentMobility::Movable);
    CollisionBox->SetCollisionProfileName(TEXT("PhysicsActor"));
    CollisionBox->SetSimulatePhysics(true);
    CollisionBox->BodyInstance.SetMassOverride(10.f);
    CollisionBox->SetUseCCD(true);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(CollisionBox);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetRelativeScale3D(FVector(0.5));
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);

    Grabbable = CreateDefaultSubobject<UGrabbableComponent>(TEXT("Grabbable"));
    Grabbable->DisplayName = NSLOCTEXT("Delivery", "Box", "Delivery box");
}

void ADeliveryBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADeliveryBox, Product);
}

void ADeliveryBox::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyProduct();
}

bool ADeliveryBox::GetProductGeometry(const FFoodProductDefinition& Definition, float BoxScale, FVector& BoxExtent, FVector& MeshOffset)
{
    if (!IsValid(Definition.BoxMesh) || !FMath::IsFinite(BoxScale) || BoxScale <= 0
        || Definition.MeshScale.ContainsNaN() || Definition.MeshScale.GetMin() <= 0)
        return false;

    const FBoxSphereBounds Bounds = Definition.BoxMesh->GetBounds();
    BoxExtent = (Definition.bOverrideCollisionExtent ? Definition.CollisionExtent : Bounds.BoxExtent * Definition.MeshScale) * BoxScale;
    MeshOffset = -Bounds.Origin * Definition.MeshScale * BoxScale;
    return !BoxExtent.ContainsNaN() && BoxExtent.GetMin() >= 1 && !MeshOffset.ContainsNaN();
}

bool ADeliveryBox::InitializeProduct(UFoodCatalog* Catalog, FName ProductId)
{
    if (!HasAuthority() || !IsValid(Catalog) || Grabbable->Holder
        || (HasActorBegunPlay() && !Product.ProductId.IsNone())) return false;

    const FFoodProductDefinition* Definition = Catalog->FindProduct(ProductId);
    FVector BoxExtent;
    FVector MeshOffset;
    if (!Definition || !GetProductGeometry(*Definition, Catalog->GetBoxScale(), BoxExtent, MeshOffset)) return false;

    Product.Catalog = Catalog;
    Product.ProductId = ProductId;
    ApplyProduct();
    ForceNetUpdate();
    return true;
}

ADeliveryBox* ADeliveryBox::SpawnProduct(const UObject* WorldContextObject, TSubclassOf<ADeliveryBox> BoxClass,
    UFoodCatalog* Catalog, FName ProductId, const FTransform& SpawnTransform)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || World->GetNetMode() == NM_Client || !BoxClass || BoxClass->HasAnyClassFlags(CLASS_Abstract)
        || !IsValid(Catalog) || SpawnTransform.ContainsNaN() || SpawnTransform.GetScale3D().GetMin() <= 0) return nullptr;

    const FFoodProductDefinition* Definition = Catalog->FindProduct(ProductId);
    FVector BoxExtent;
    FVector MeshOffset;
    if (!Definition || !GetProductGeometry(*Definition, Catalog->GetBoxScale(), BoxExtent, MeshOffset)) return nullptr;

    const FName CollisionProfile = BoxClass->GetDefaultObject<ADeliveryBox>()->CollisionBox->GetCollisionProfileName();
    if (World->OverlapBlockingTestByProfile(SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), CollisionProfile,
        FCollisionShape::MakeBox(BoxExtent * SpawnTransform.GetScale3D()))) return nullptr;

    ADeliveryBox* Box = World->SpawnActorDeferred<ADeliveryBox>(BoxClass, SpawnTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn, ESpawnActorScaleMethod::OverrideRootScale);
    if (!Box) return nullptr;
    if (!Box->InitializeProduct(Catalog, ProductId))
    {
        Box->Destroy();
        return nullptr;
    }

    Box->SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    Box->FinishSpawning(SpawnTransform);
    return IsValid(Box) && !Box->IsActorBeingDestroyed() ? Box : nullptr;
}

bool ADeliveryBox::ApplyProduct()
{
    const FFoodProductDefinition* Definition = IsValid(Product.Catalog) ? Product.Catalog->FindProduct(Product.ProductId) : nullptr;
    FVector BoxExtent;
    FVector MeshOffset;
    if (!Definition || !GetProductGeometry(*Definition, Product.Catalog->GetBoxScale(), BoxExtent, MeshOffset)) return false;

    CollisionBox->SetBoxExtent(BoxExtent);
    Mesh->SetStaticMesh(Definition->BoxMesh);
    Mesh->SetRelativeTransform(FTransform(FQuat::Identity, MeshOffset, Definition->MeshScale * Product.Catalog->GetBoxScale()));
    Grabbable->DisplayName = Definition->Name;
    return true;
}

void ADeliveryBox::OnRep_Product()
{
    ApplyProduct();
}
