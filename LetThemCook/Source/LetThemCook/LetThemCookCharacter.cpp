// Copyright Epic Games, Inc. All Rights Reserved.

#include "LetThemCookCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LetThemCook.h"
#include "Interaction/InteractorComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

ALetThemCookCharacter::ALetThemCookCharacter()
{
	bReplicates = true;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	InteractionPhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("InteractionPhysicsHandle"));
	Interactor = CreateDefaultSubobject<UInteractorComponent>(TEXT("Interactor"));
	Interactor->ViewComponent.ComponentProperty = GET_MEMBER_NAME_CHECKED(ALetThemCookCharacter, FirstPersonCameraComponent);

	static ConstructorHelpers::FObjectFinder<UInputAction> InteractAsset(TEXT("/Game/Input/Actions/IA_Interact.IA_Interact"));
	InteractAction = InteractAsset.Object;

	// Keep the project's capsule, speed, jump, and camera settings. Only tune
	// character/body contact so light movable items stay stable underfoot.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bEnablePhysicsInteraction = true;
	Movement->StandingDownwardForceScale = 0.f;
	Movement->TouchForceFactor = 0.f;
	Movement->RepulsionForce = 0.f;
	Movement->InitialPushForceFactor = 200.f;
	Movement->PushForceFactor = 10000.f;
	Movement->bPushForceScaledToMass = false;
	Movement->bPushForceUsingZOffset = true;
	Movement->PushForcePointZOffsetFactor = 0.f;
	Movement->bImpartBaseAngularVelocity = false;
}

void ALetThemCookCharacter::BeginPlay()
{
	Super::BeginPlay();
	// This project's camera is attached to the animated head. A dedicated
	// server must refresh that pose too, otherwise it traces from a stale socket.
	if (HasAuthority())
	{
		FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	Interactor->AddTickPrerequisiteComponent(FirstPersonMesh);
}

FVector ALetThemCookCharacter::GetPawnViewLocation() const
{
	return FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : Super::GetPawnViewLocation();
}

void ALetThemCookCharacter::UnPossessed()
{
	Interactor->CancelInteraction();
	Interactor->ReleaseHeldItem();
	Super::UnPossessed();
}

void ALetThemCookCharacter::InteractionStarted() { Interactor->BeginInteraction(); }
void ALetThemCookCharacter::InteractionCompleted() { Interactor->EndInteraction(); }
void ALetThemCookCharacter::InteractionCanceled() { Interactor->CancelInteraction(); }

void ALetThemCookCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ALetThemCookCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ALetThemCookCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALetThemCookCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALetThemCookCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALetThemCookCharacter::LookInput);

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ALetThemCookCharacter::InteractionStarted);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ALetThemCookCharacter::InteractionCompleted);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ALetThemCookCharacter::InteractionCanceled);
		}
	}
	else
	{
		UE_LOG(LogLetThemCook, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ALetThemCookCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ALetThemCookCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ALetThemCookCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ALetThemCookCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ALetThemCookCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ALetThemCookCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
