#include "TabletDevice.h"
#include "STabletScreen.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/SViewport.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "LetThemCookCharacter.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Math/RotationMatrix.h"

UTabletDisplayComponent::UTabletDisplayComponent()
{
    bReceiveHardwareInput = true;
    SetWidgetSpace(EWidgetSpace::World);
    SetBlendMode(EWidgetBlendMode::Opaque);
    SetTwoSided(false);
    SetPivot(FVector2D(0.5, 0.5));
    SetWindowFocusable(true);
    SetTickWhenOffscreen(true);
    SetDrawSize(FVector2D(1280, 720));
    SetBackgroundColor(FLinearColor(0.025f, 0.035f, 0.045f, 1.0f));
    // Keep the physical display legible under both indoor and outdoor exposure.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScreenMaterial(TEXT("/Game/UI/Tablet/M_TabletDisplay.M_TabletDisplay"));
    if (ScreenMaterial.Succeeded()) SetMaterial(0, ScreenMaterial.Object);
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SetGenerateOverlapEvents(false);
    CastShadow = false;
}

ATabletDevice::ATabletDevice()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TabletRoot")));
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TabletBody"));
    Body->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
    if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CaseMaterial(TEXT("/Game/UI/Tablet/M_TabletCase.M_TabletCase"));
    if (CaseMaterial.Succeeded())
    {
        // The prototype cube can use a separate material slot for each face.
        for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot) Body->SetMaterial(Slot, CaseMaterial.Object);
    }
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetGenerateOverlapEvents(false);
    Body->CastShadow = false;

    Display = CreateDefaultSubobject<UTabletDisplayComponent>(TEXT("TabletDisplay"));
    Display->SetupAttachment(RootComponent);
    Display->SetRelativeLocation(FVector(-1.01, 0, 0));
    Display->SetRelativeRotation(FRotator(0, 180, 0));
    Hands = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("TabletHands"));
    Hands->SetupAttachment(RootComponent);
    Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hands->SetGenerateOverlapEvents(false);
    Hands->CastShadow = false;
    Hands->SetVisibility(false);
    SetActorHiddenInGame(true);
}

void ATabletDevice::Equip(APlayerController* Controller, const TSharedRef<STabletScreen>& Screen)
{
    ViewingController = Controller;
    SetOwner(Controller->GetPawn());
    PrepareHands();
    CurrentScreen = Screen;
    CurrentScreen->SetVisibility(EVisibility::HitTestInvisible);
    Display->SetOwnerPlayer(Controller->GetLocalPlayer());
    Display->SetSlateWidget(Screen);
    if (FSlateApplication::IsInitialized())
    {
        if (const TSharedPtr<SWindow> Window = Display->GetSlateWindow())
        {
            FSlateApplication::Get().RegisterVirtualWindow(Window.ToSharedRef());
            KeyboardWindow = Window;
            if (Controller->GetLocalPlayer()->ViewportClient)
                StaticCastSharedPtr<SVirtualWindow>(Window)->AssignParentWidget(Controller->GetLocalPlayer()->ViewportClient->GetGameViewportWidget());
        }
    }
    Display->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Display->SetVisibility(true);
    bEquipping = true;
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
    UpdatePose();
}

void ATabletDevice::Stow()
{
    ReleaseKeyboardWindow();
    bEquipping = false;
    Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (CurrentScreen.IsValid()) CurrentScreen->SetVisibility(EVisibility::HitTestInvisible);
}

void ATabletDevice::ReleaseKeyboardWindow()
{
    if (FSlateApplication::IsInitialized())
    {
        if (const TSharedPtr<SWindow> Window = KeyboardWindow.Pin())
            FSlateApplication::Get().UnregisterVirtualWindow(Window.ToSharedRef());
    }
    KeyboardWindow.Reset();
}

FVector2D ATabletDevice::CalculateScreenSize(float Distance, float HorizontalFov, float AspectRatio)
{
    const float Width = 2 * Distance * FMath::Tan(FMath::DegreesToRadians(HorizontalFov * 0.5f));
    return FVector2D(Width, Width / FMath::Max(AspectRatio, 0.1f));
}

void ATabletDevice::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ViewingController.IsValid() || !ViewingController->GetPawn())
    {
        Destroy();
        return;
    }
    EquipProgress = FMath::Clamp(EquipProgress + (bEquipping ? 1 : -1) * DeltaSeconds / 0.35f, 0.0f, 1.0f);
    UpdatePose();
    if (bEquipping && EquipProgress >= 1 && CurrentScreen.IsValid()) CurrentScreen->SetVisibility(EVisibility::Visible);
    if (!bEquipping && EquipProgress <= 0)
    {
        SetActorHiddenInGame(true);
        Display->SetVisibility(false);
        Display->SetSlateWidget(nullptr);
        CurrentScreen.Reset();
        Hands->SetVisibility(false);
        RestoreCharacterHands();
        SetActorTickEnabled(false);
    }
}

void ATabletDevice::UpdatePose()
{
    APlayerController* Controller = ViewingController.Get();
    if (!Controller) return;
    FVector CameraLocation;
    FRotator CameraRotation;
    Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
    int32 Width = 1280, Height = 720;
    Controller->GetViewportSize(Width, Height);
    const float Aspect = Width > 0 && Height > 0 ? float(Width) / Height : 16.0f / 9.0f;
    constexpr float ScreenDistance = 26.0f;
    const float Fov = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;
    FVector2D ScreenSize = CalculateScreenSize(ScreenDistance, Fov, Aspect);

    // Deprojection includes UE's aspect-ratio FOV policy; avoid assumptions on ultrawide windows.
    FVector TopOrigin, TopDirection, BottomOrigin, BottomDirection;
    if (Width > 0 && Height > 0 &&
        Controller->DeprojectScreenPositionToWorld(0, 0, TopOrigin, TopDirection) &&
        Controller->DeprojectScreenPositionToWorld(Width, Height, BottomOrigin, BottomDirection))
    {
        const FVector Normal = CameraRotation.Vector();
        const FVector PlaneOrigin = CameraLocation + Normal * ScreenDistance;
        const FVector Top = FMath::LinePlaneIntersection(TopOrigin, TopOrigin + TopDirection * 1000, PlaneOrigin, Normal);
        const FVector Bottom = FMath::LinePlaneIntersection(BottomOrigin, BottomOrigin + BottomDirection * 1000, PlaneOrigin, Normal);
        ScreenSize.X = FMath::Abs(FVector::DotProduct(Bottom - Top, CameraRotation.RotateVector(FVector::RightVector)));
        ScreenSize.Y = FMath::Abs(FVector::DotProduct(Bottom - Top, CameraRotation.RotateVector(FVector::UpVector)));
    }
    // Leave the world visible around the device, while retaining full UI resolution.
    ScreenSize *= 0.78f;
    PoseHands(ScreenSize);
    const FIntPoint DrawSize(1280, FMath::Clamp(FMath::RoundToInt(1280.0f * ScreenSize.Y / FMath::Max(ScreenSize.X, 1.0f)), 400, 1600));
    if (DrawSize != CachedDrawSize)
    {
        Display->SetDrawSize(FVector2D(DrawSize));
        CachedDrawSize = DrawSize;
    }
    Display->SetRelativeScale3D(FVector(1, ScreenSize.X * 0.975f / DrawSize.X, ScreenSize.Y * 0.975f / DrawSize.Y));
    if (Body->GetStaticMesh())
    {
        const FVector MeshSize = Body->GetStaticMesh()->GetBoundingBox().GetSize();
        const FVector BodyScale(2.0f / MeshSize.X, ScreenSize.X * 1.08f / MeshSize.Y, ScreenSize.Y * 1.08f / MeshSize.Z);
        Body->SetRelativeScale3D(BodyScale);
        // The template cube has an offset pivot; center its bounds behind the display.
        Body->SetRelativeLocation(-Body->GetStaticMesh()->GetBoundingBox().GetCenter() * BodyScale);
    }
    const float Smooth = EquipProgress * EquipProgress * (3 - 2 * EquipProgress);
    const FVector Offset(ScreenDistance + 1.01f, 0, -ScreenSize.Y * (0.025f + 1.35f * (1 - Smooth)));
    // Tilt only forward/back: the upper edge rests farther from the camera.
    const FQuat Tilt = FRotator(FMath::Lerp(-28.0f, -5.0f, Smooth), 0, -12 * (1 - Smooth)).Quaternion();
    SetActorLocationAndRotation(CameraLocation + CameraRotation.RotateVector(Offset), CameraRotation.Quaternion() * Tilt);
}

void ATabletDevice::PrepareHands()
{
    ALetThemCookCharacter* Character = Cast<ALetThemCookCharacter>(ViewingController.IsValid() ? ViewingController->GetPawn() : nullptr);
    USkeletalMeshComponent* Source = Character ? Character->GetFirstPersonMesh() : nullptr;
    if (!Source || !Source->GetSkeletalMeshAsset()) return;
    if (CharacterHands.Get() != Source)
    {
        RestoreCharacterHands();
        CharacterHands = Source;
        bCharacterHandsWereVisible = Source->IsVisible();
        OriginalAnimTickOption = static_cast<uint8>(Source->VisibilityBasedAnimTickOption);
    }
    Hands->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
    bSimpleHandSkeleton = Hands->GetBoneIndex(TEXT("ARM_L_START")) != INDEX_NONE;
    for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot) Hands->SetMaterial(Slot, Source->GetMaterial(Slot));
    ReferenceHandPose = Hands->GetSkinnedAsset()->GetRefSkeleton().GetRefBonePose();
    CachedGripSize = FVector2D::ZeroVector;
    Hands->SetVisibility(true);
    // Hide only the original render mesh; its camera and animation remain attached.
    Source->SetVisibility(false, false);
    // The camera is attached to its head bone; keep updating that pose while hidden.
    Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ATabletDevice::RestoreCharacterHands()
{
    if (CharacterHands.IsValid())
    {
        CharacterHands->SetVisibility(bCharacterHandsWereVisible, false);
        CharacterHands->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(OriginalAnimTickOption);
    }
    CharacterHands.Reset();
}

void ATabletDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseKeyboardWindow();
    RestoreCharacterHands();
    Super::EndPlay(EndPlayReason);
}

void ATabletDevice::PoseHands(const FVector2D& ScreenSize)
{
    if (!Hands->GetSkinnedAsset() || ReferenceHandPose.IsEmpty() || CachedGripSize.Equals(ScreenSize, 0.01f)) return;
    CachedGripSize = ScreenSize;
    Hands->BoneSpaceTransforms = ReferenceHandPose;
    // The stylized cook has very broad shoulders; fit its arms to a first-person view.
    Hands->SetRelativeScale3D(FVector(bSimpleHandSkeleton ? 0.6f : 1.0f));
    Hands->SetRelativeRotation(FRotator::ZeroRotator);
    Hands->SetRelativeLocation(FVector::ZeroVector);
    Hands->RefreshBoneTransforms();
    const FName LeftShoulder = bSimpleHandSkeleton ? TEXT("ARM_L_START") : TEXT("upperarm_l");
    const FName RightShoulder = bSimpleHandSkeleton ? TEXT("ARM_R_START") : TEXT("upperarm_r");
    if (Hands->GetBoneIndex(LeftShoulder)==INDEX_NONE || Hands->GetBoneIndex(RightShoulder)==INDEX_NONE) { Hands->SetVisibility(false); RestoreCharacterHands(); return; }
    const FVector ReferenceL = Hands->GetBoneLocationByName(LeftShoulder, EBoneSpaces::ComponentSpace);
    const FVector ReferenceR = Hands->GetBoneLocationByName(RightShoulder, EBoneSpaces::ComponentSpace);
    const FVector RightAxis = ReferenceR-ReferenceL;
    const float MeshYaw = 90.0f-FMath::RadiansToDegrees(FMath::Atan2(RightAxis.Y,RightAxis.X));
    Hands->SetRelativeRotation(FRotator(0,MeshYaw,0));
    const FTransform MeshToDevice = Hands->GetRelativeTransform();
    const FVector ShoulderL = MeshToDevice.TransformPosition(ReferenceL);
    const FVector ShoulderR = MeshToDevice.TransformPosition(ReferenceR);
    // Torso stays below the camera; only the reaching arms enter the frame.
    Hands->SetRelativeLocation(FVector(-8, 0, -19) - (ShoulderL + ShoulderR) * 0.5f);
    for (const TCHAR* Bone : {TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r")})
        if (Hands->GetBoneIndex(Bone) != INDEX_NONE) Hands->SetBoneScaleByName(Bone, FVector::ZeroVector, EBoneSpaces::ComponentSpace);
    if (bSimpleHandSkeleton) for (const TCHAR* Bone : {TEXT("BODY1"),TEXT("HEAD_1"),TEXT("LEG")})
        if (Hands->GetBoneIndex(Bone) != INDEX_NONE) Hands->SetBoneScaleByName(Bone,FVector::ZeroVector,EBoneSpaces::ComponentSpace);

    for (const TCHAR* Suffix : {TEXT("l"), TEXT("r")})
    {
        const FString SideName(Suffix);
        const bool Left = SideName == TEXT("l");
        const FName Upper = bSimpleHandSkeleton ? (Left?TEXT("ARM_L_START"):TEXT("ARM_R_START")) : *(TEXT("upperarm_")+SideName);
        const FName Lower = bSimpleHandSkeleton ? (Left?TEXT("ARM_L"):TEXT("ARM_R")) : *(TEXT("lowerarm_")+SideName);
        const FName Hand = bSimpleHandSkeleton ? (Left?TEXT("ARM_L_END"):TEXT("ARM_R_END")) : *(TEXT("hand_")+SideName);
        if (Hands->GetBoneIndex(Upper) == INDEX_NONE || Hands->GetBoneIndex(Lower) == INDEX_NONE || Hands->GetBoneIndex(Hand) == INDEX_NONE) continue;
        const FTransform ToDevice = Hands->GetRelativeTransform();
        const FVector Shoulder = ToDevice.TransformPosition(Hands->GetBoneLocationByName(Upper, EBoneSpaces::ComponentSpace));
        const float Side = Shoulder.Y < 0 ? -1.0f : 1.0f;
        const FVector Grip(-3.0f, Side * (ScreenSize.X * 0.54f + 0.5f), -ScreenSize.Y * 0.28f);
        const FVector A = Hands->GetBoneLocationByName(Upper, EBoneSpaces::ComponentSpace);
        const FVector B = Hands->GetBoneLocationByName(Lower, EBoneSpaces::ComponentSpace);
        const FVector C = Hands->GetBoneLocationByName(Hand, EBoneSpaces::ComponentSpace);
        const FVector Target = ToDevice.InverseTransformPosition(Grip);
        const float UpperLength = FVector::Distance(A,B), LowerLength = FVector::Distance(B,C);
        const FVector Direction = (Target-A).GetSafeNormal();
        const float Distance = FMath::Clamp(float(FVector::Distance(A,Target)), FMath::Abs(UpperLength-LowerLength)+0.1f, UpperLength+LowerLength-0.1f);
        const float Along = (UpperLength*UpperLength-LowerLength*LowerLength+Distance*Distance)/(2*Distance);
        const FVector BendHint = ToDevice.InverseTransformVector(FVector(-0.3f, Side, -1));
        const FVector Bend = (BendHint - Direction * FVector::DotProduct(BendHint,Direction)).GetSafeNormal();
        const FVector Elbow = A + Direction*Along + Bend*FMath::Sqrt(FMath::Max(0.0f,UpperLength*UpperLength-Along*Along));
        FTransform UpperPose = Hands->GetBoneTransformByName(Upper, EBoneSpaces::ComponentSpace);
        UpperPose.SetRotation(FQuat::FindBetweenVectors(B-A,Elbow-A)*UpperPose.GetRotation());
        Hands->SetBoneTransformByName(Upper,UpperPose,EBoneSpaces::ComponentSpace);
        const FVector NewElbow = Hands->GetBoneLocationByName(Lower, EBoneSpaces::ComponentSpace);
        const FVector NewWrist = Hands->GetBoneLocationByName(Hand, EBoneSpaces::ComponentSpace);
        FTransform LowerPose = Hands->GetBoneTransformByName(Lower, EBoneSpaces::ComponentSpace);
        LowerPose.SetLocation(Elbow);
        LowerPose.SetRotation(FQuat::FindBetweenVectors(NewWrist-NewElbow,Target-Elbow)*LowerPose.GetRotation());
        Hands->SetBoneTransformByName(Lower,LowerPose,EBoneSpaces::ComponentSpace);
        FTransform HandPose = Hands->GetBoneTransformByName(Hand, EBoneSpaces::ComponentSpace);
        HandPose.SetLocation(Target);
        const FVector Fingers = ToDevice.InverseTransformVector(FVector(0,-Side,0));
        const FVector Across = ToDevice.InverseTransformVector(FVector(0,0,Side));
        HandPose.SetRotation(FRotationMatrix::MakeFromXY(Fingers,Across).ToQuat());
        Hands->SetBoneTransformByName(Hand,HandPose,EBoneSpaces::ComponentSpace);
        // Curl the fingers around the case; the thumb rests against the front rim.
        for (const TCHAR* Finger : {TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky"),TEXT("thumb")})
        {
            for (int32 Joint = 1; Joint <= 3; ++Joint)
            {
                const FName Bone(*FString::Printf(TEXT("%s_%02d_%s"),Finger,Joint,Suffix));
                const int32 Index = Hands->GetBoneIndex(Bone);
                if (ReferenceHandPose.IsValidIndex(Index))
                {
                    const float Angle = FCString::Strcmp(Finger,TEXT("thumb")) == 0 ? 20.0f : (Joint==1?35.0f:55.0f);
                    Hands->BoneSpaceTransforms[Index].SetRotation(ReferenceHandPose[Index].GetRotation()*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(Angle*Side)));
                }
            }
        }
    }
    Hands->RefreshBoneTransforms();
}
