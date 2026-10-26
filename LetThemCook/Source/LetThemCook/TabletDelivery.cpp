#include "TabletDelivery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATabletDeliveryPoint::ATabletDeliveryPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    UStaticMeshComponent* Pallet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pallet"));
    SetRootComponent(Pallet);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
    if (Cube.Succeeded()) Pallet->SetStaticMesh(Cube.Object);
    Pallet->SetRelativeScale3D(FVector(1.8, 1.8, 0.12));
    Pallet->SetCollisionProfileName(TEXT("BlockAll"));
}

ATabletDeliveryBox::ATabletDeliveryBox()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
    SetRootComponent(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
    if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetRelativeScale3D(FVector(0.45, 0.45, 0.45));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Contents"));
    Label->SetupAttachment(Mesh);
    Label->SetRelativeLocation(FVector(51, 0, 20));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(22);
    Label->SetTextRenderColor(FColor::White);
}

void ATabletDeliveryBox::Initialize(FName InProductId, const FText& InProductName, int32 InQuantity)
{
    check(HasAuthority());
    ProductId = InProductId;
    ProductName = InProductName;
    Quantity = InQuantity;
    OnRep_Contents();
    ForceNetUpdate();
}

void ATabletDeliveryBox::OnRep_Contents()
{
    Label->SetText(FText::Format(NSLOCTEXT("Tablet", "BoxLabel", "{0}\nx{1}"), ProductName, FText::AsNumber(Quantity)));
}

void ATabletDeliveryBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATabletDeliveryBox, ProductId);
    DOREPLIFETIME(ATabletDeliveryBox, ProductName);
    DOREPLIFETIME(ATabletDeliveryBox, Quantity);
}
