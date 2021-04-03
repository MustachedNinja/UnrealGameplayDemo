// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDemoCharacter.h"
#include "GameplayDemoProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "Engine/EngineTypes.h"
#include "Math/Vector.h"
#include "CableComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AGameplayDemoCharacter

AGameplayDemoCharacter::AGameplayDemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	Cable = CreateDefaultSubobject<UCableComponent>(TEXT("Cable"));
	Cable->AttachToComponent(FP_MuzzleLocation, FAttachmentTransformRules::KeepRelativeTransform);
	Cable->SetAttachEndTo(this, RootComponent->GetDefaultSceneRootVariableName());

	Cable->NumSegments = 1;
	Cable->EndLocation = FVector(0.0f, 0.0f, 0.0f);
	Cable->bHiddenInGame = true;

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun and attach it to the right-hand VR controller.
	// Create a gun mesh component
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000004, 53.999992, 10.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.

	// Uncomment the following line to turn motion controllers on by default:
	//bUsingMotionControllers = true;

	JumpHeight = 600.0f;
	WalkSpeed = 600.0f;
	SprintSpeed = 1000.0f;

	CanDash = true;
	DashDistance = 6000.0f;
	DashCooldown = 1.0f;
	DashStop = 0.1f;

	GrappleConnected = false;
	GrappleDistance = 6000.0f;
	GrappleRadius = 20.0f;
	GrappleForce = FVector(0.0f, 0.0f, 0.0f);
	GrappleBoost = 1000.0f;
}

void AGameplayDemoCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}
}

void AGameplayDemoCharacter::Tick(float DeltaSeconds) {
	if (GrappleConnected) {
		GetCharacterMovement()->Velocity += GrappleForce;
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AGameplayDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AGameplayDemoCharacter::MultipleJump);

	// Bind sprint event
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AGameplayDemoCharacter::Sprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AGameplayDemoCharacter::Walk);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AGameplayDemoCharacter::OnFire);

	PlayerInputComponent->BindAction("ShootGrapple", IE_Pressed, this, &AGameplayDemoCharacter::OnGrapple);

	// Enable touchscreen input
	EnableTouchscreenMovement(PlayerInputComponent);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AGameplayDemoCharacter::OnResetVR);

	// Bind dash events
	PlayerInputComponent->BindAction("ForwardDash", IE_DoubleClick, this, &AGameplayDemoCharacter::DashForward);
	PlayerInputComponent->BindAction("LeftDash", IE_DoubleClick, this, &AGameplayDemoCharacter::DashLeft);
	PlayerInputComponent->BindAction("RightDash", IE_DoubleClick, this, &AGameplayDemoCharacter::DashRight);
	PlayerInputComponent->BindAction("BackDash", IE_DoubleClick, this, &AGameplayDemoCharacter::DashBack);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AGameplayDemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGameplayDemoCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGameplayDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGameplayDemoCharacter::LookUpAtRate);
}

void AGameplayDemoCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			if (bUsingMotionControllers)
			{
				const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
				const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
				World->SpawnActor<AGameplayDemoProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
			}
			else
			{
				const FRotator SpawnRotation = GetControlRotation();
				// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
				const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

				//Set Spawn Collision Handling Override
				FActorSpawnParameters ActorSpawnParams;
				ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

				// spawn the projectile at the muzzle
				World->SpawnActor<AGameplayDemoProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
			}
		}
	}

	// try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

float CalculateGrappleForce(FVector_NetQuantize hookPoint, FVector velocity, FVector currentLocation) {
	FVector temp = currentLocation - hookPoint;
	float temp2 = FVector::DotProduct(temp, velocity);
	FVector temp3 = -2.0f * temp.GetSafeNormal() * temp2;
	return 0.0f;
}

void AGameplayDemoCharacter::OnGrapple()
{
	if (GrappleConnected) {
		// Disconnect the grapple
		GrappleConnected = false;
		Cable->SetHiddenInGame(GrappleConnected);
	}
	else {
		// Connect the grapple
		FVector direction = FirstPersonCameraComponent->GetForwardVector().GetSafeNormal() * GrappleDistance;
		FVector startPoint = GetActorLocation();
		TArray<TEnumAsByte<EObjectTypeQuery>> objectTypesArray;
		objectTypesArray.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TArray<AActor* > ignoreList;
		FHitResult outHit;
		bool hit = UKismetSystemLibrary::SphereTraceSingleForObjects(GetWorld(), startPoint, startPoint + direction, GrappleRadius, objectTypesArray, false, ignoreList, EDrawDebugTrace::ForDuration, outHit, true);
		if (hit) {
			GrappleConnected = true;
			FVector_NetQuantize impactPoint = outHit.ImpactPoint;

			Cable->SetHiddenInGame(GrappleConnected);

			GrappleForce = FVector(CalculateGrappleForce(impactPoint, GetCharacterMovement()->Velocity, GetActorLocation())) + FVector(FirstPersonCameraComponent->GetForwardVector().GetSafeNormal() * GrappleBoost);

			// The following two lines should be running on every tick;
			//GetCharacterMovement()->Velocity += FVector(CalculateGrappleForce(impactPoint, GetCharacterMovement()->Velocity, GetActorLocation()));
			//GetCharacterMovement()->Velocity += FVector(FirstPersonCameraComponent->GetForwardVector().GetSafeNormal() * 50000.0f);
		}

	}

}

void AGameplayDemoCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AGameplayDemoCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void AGameplayDemoCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	TouchItem.bIsPressed = false;
}

//Commenting this section out to be consistent with FPS BP template.
//This allows the user to turn without using the right virtual joystick

//void AGameplayDemoCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
//	{
//		if (TouchItem.bIsPressed)
//		{
//			if (GetWorld() != nullptr)
//			{
//				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
//				if (ViewportClient != nullptr)
//				{
//					FVector MoveDelta = Location - TouchItem.Location;
//					FVector2D ScreenSize;
//					ViewportClient->GetViewportSize(ScreenSize);
//					FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
//					if (FMath::Abs(ScaledDelta.X) >= 4.0 / ScreenSize.X)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.X * BaseTurnRate;
//						AddControllerYawInput(Value);
//					}
//					if (FMath::Abs(ScaledDelta.Y) >= 4.0 / ScreenSize.Y)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.Y * BaseTurnRate;
//						AddControllerPitchInput(Value);
//					}
//					TouchItem.Location = Location;
//				}
//				TouchItem.Location = Location;
//			}
//		}
//	}
//}

void AGameplayDemoCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AGameplayDemoCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AGameplayDemoCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AGameplayDemoCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool AGameplayDemoCharacter::EnableTouchscreenMovement(class UInputComponent* PlayerInputComponent)
{
	if (FPlatformMisc::SupportsTouchInput() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AGameplayDemoCharacter::BeginTouch);
		PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &AGameplayDemoCharacter::EndTouch);

		//Commenting this out to be more consistent with FPS BP template.
		//PlayerInputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AGameplayDemoCharacter::TouchUpdate);
		return true;
	}
	
	return false;
}

void AGameplayDemoCharacter::Landed(const FHitResult& Hit)
{
	JumpCounter = 0;
}

void AGameplayDemoCharacter::MultipleJump() {
	if (JumpCounter < JumpLimit) {
		ACharacter::LaunchCharacter(FVector(0, 0, JumpHeight), false, true);
		++JumpCounter;
	}
}

void AGameplayDemoCharacter::Sprint()
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AGameplayDemoCharacter::Walk()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AGameplayDemoCharacter::DashForward()
{
	if (CanDash) {
		GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
		LaunchCharacter(FVector(FirstPersonCameraComponent->GetForwardVector().X, FirstPersonCameraComponent->GetForwardVector().Y, 0).GetSafeNormal() * DashDistance, true, true);
		CanDash = false;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AGameplayDemoCharacter::StopDash, DashStop, false);
	}
}

void AGameplayDemoCharacter::DashLeft() {
	if (CanDash) {
		GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
		LaunchCharacter(FVector(FirstPersonCameraComponent->GetForwardVector().X, FirstPersonCameraComponent->GetForwardVector().Y, 0).RotateAngleAxis(-90, FVector(0,0,1)).GetSafeNormal() * DashDistance, true, true);
		CanDash = false;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AGameplayDemoCharacter::StopDash, DashStop, false);
	}
}

void AGameplayDemoCharacter::DashRight() {
	if (CanDash) {
		GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
		LaunchCharacter(FVector(FirstPersonCameraComponent->GetForwardVector().X, FirstPersonCameraComponent->GetForwardVector().Y, 0).RotateAngleAxis(90, FVector(0, 0, 1)).GetSafeNormal() * DashDistance, true, true);
		CanDash = false;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AGameplayDemoCharacter::StopDash, DashStop, false);
	}
}

void AGameplayDemoCharacter::DashBack() {
	if (CanDash) {
		GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
		LaunchCharacter(FVector(FirstPersonCameraComponent->GetForwardVector().X, FirstPersonCameraComponent->GetForwardVector().Y, 0).RotateAngleAxis(180, FVector(0, 0, 1)).GetSafeNormal() * DashDistance, true, true);
		CanDash = false;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AGameplayDemoCharacter::StopDash, DashStop, false);
	}
}

void AGameplayDemoCharacter::StopDash() {
	GetCharacterMovement()->StopMovementImmediately();
	GetWorldTimerManager().SetTimer(UnusedHandle, this, &AGameplayDemoCharacter::ResetDash, DashCooldown, false);
	GetCharacterMovement()->BrakingFrictionFactor = 2.0f;
}

void AGameplayDemoCharacter::ResetDash() {
	CanDash = true;
}
