#include "ProjectMarcus/Character/ProjectMarcusCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Sound/SoundCue.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AProjectMarcusCharacter::AProjectMarcusCharacter() :
	BaseTurnRate(MoveData.TurnRate),
	BaseLookUpRate(MoveData.LookUpRate),
	bIsAiming(false),
	CameraDefaultFOV(CameraData.DefaultFOV), // Set withing BeginPlay. Setting to 0 which makes no sense so it's obvious if something is wrong
	CameraZoomedFOV(CameraData.ZoomedFOV)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	if (CameraArm == nullptr)
	{
		CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
		if (ensure(CameraArm))
		{
			// Create a camera boom (pulls in towards the character if there is a collision)
			CameraArm->SetupAttachment(RootComponent);
			CameraArm->TargetArmLength = CameraData.BoomLength; // camera follows at this distance behind the character
			CameraArm->bUsePawnControlRotation = true; // rotate the arm based on the controller
			CameraArm->SocketOffset = CameraData.ScreenOffset; //offsets the character
	
			if (FollowCam == nullptr)
			{
				FollowCam = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
				if (ensure(FollowCam))
				{
					FollowCam->SetupAttachment(CameraArm, USpringArmComponent::SocketName); // attach camera to end of arm
					FollowCam->bUsePawnControlRotation = false; // don't want the camera to rotate relative to arm - the camera follows the camera arms rotation
				}
			}
		}
	}

	// Don't rotate the character when the controller rotates. We only want the controller to rotate the camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// The character rotation is dependent on the movement component
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (ensure(MoveComp))
	{
		MoveComp->bOrientRotationToMovement = false; // Character moves in the direction of input
		// TODO: move these into BP editable params for designers
		MoveComp->RotationRate = MoveData.RotationRate; // determines how fast we rotate. lower = slow rotation. higher = fast. negative = snap instantly
		MoveComp->JumpZVelocity = MoveData.JumpVelocity; // how high the character jumps
		MoveComp->AirControl = MoveData.AirControl; // 0 = no control. 1 = full control at max speed
	}
}

// Called when the game starts or when spawned
void AProjectMarcusCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCam)
	{
		CameraDefaultFOV = FollowCam->FieldOfView;
	}
}

void AProjectMarcusCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.f)
	{
		// Get the rotation around the up (z) axis, find fwd vector, move in that direction
		const FRotator Rotation(Controller->GetControlRotation());
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f); // yaw = rotation around up/z axis

		// find out the direction the controller is pointing fwd
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X)); // EAxis::X = forward direction

		AddMovementInput(Direction, Value);
	}
}

void AProjectMarcusCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.f)
	{
		// Get the rotation around the up (z) axis, find right vector, move in that direction
		const FRotator Rotation(Controller->GetControlRotation());
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f); // yaw = rotation around up/z axis

		// find out the direction the controller is pointing right
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y)); // EAxis::Y = right direction

		AddMovementInput(Direction, Value);
	}
}

void AProjectMarcusCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame = deg/frame
}

void AProjectMarcusCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame = deg/frame
}

void AProjectMarcusCharacter::FireWeapon()
{
	// SFX
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}

	// Muzzle Flash VFX + Linetracing/Collision + Impact Particles + Kickback Anim
	const USkeletalMeshComponent* CharMesh = GetMesh();
	if (CharMesh)
	{
		// Find the socket at the tip of the barrel with its current position and rotation and spawn a particle system
		const USkeletalMeshSocket* BarrelSocket = CharMesh->GetSocketByName("BarrelSocket");
		if (BarrelSocket)
		{
			const FTransform SocketTransform = BarrelSocket->GetSocketTransform(CharMesh);

			// Muzzle flash VFX
			if (MuzzleFlash)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
			}

			FVector BulletHitLocation;
			if (GetFinalHitLocation(SocketTransform.GetLocation(), BulletHitLocation))
			{

				if (GetWorld())
				{
					// Spawn impact particles
					if (BulletImpactParticles)
					{
						UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BulletImpactParticles, BulletHitLocation);
					}

					// Spawn trail particles
					if (BulletTrailParticles)
					{
						UParticleSystemComponent* Trail = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BulletTrailParticles, SocketTransform);
						if (Trail)
						{
							Trail->SetVectorParameter("Target", BulletHitLocation); // makes it so the particles appear in a line from TraceStart  to TrailEndPoint
						}
					}
				}
			}
		}

		// Play Kickback animation
		UAnimInstance* AnimInstance = CharMesh->GetAnimInstance();
		if (AnimInstance && HipFireMontage)
		{
			AnimInstance->Montage_Play(HipFireMontage);
			AnimInstance->Montage_JumpToSection("StartFire");
		}
	}
}

void AProjectMarcusCharacter::AimButtonPressed()
{
	bIsAiming = true;
	if (FollowCam)
	{
		FollowCam->SetFieldOfView(CameraZoomedFOV);
	}
}

void AProjectMarcusCharacter::AimButtonReleased()
{
	bIsAiming = false;
	if (FollowCam)
	{
		FollowCam->SetFieldOfView(CameraDefaultFOV);
	}
}

bool AProjectMarcusCharacter::GetFinalHitLocation(const FVector BarrelSocketLocation, FVector& OutHitLocation)
{
	if (GetWorld())
	{
		// Get current viewport size
		FVector2D ViewportSize;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// Get the cross hair local position (screen space)
		FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2); // Exact middle of the screen;
		CrosshairLocation.Y -= CameraData.ScreenOffset.Y; // adjust to match HUD

		// Translate crosshair to world position
		FVector CrosshairWorldPos;
		FVector CrosshairWorldDir;
		bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
			UGameplayStatics::GetPlayerController(this, LOCAL_USER_NUM),
			CrosshairLocation,
			CrosshairWorldPos,
			CrosshairWorldDir // Forward vector (so outwards from the viewport)
		);

		if (bScreenToWorld)
		{
			// Trace from crosshairs straight out
			FHitResult CrosshairsHit;
			const FVector CrosshairTraceStart(CrosshairWorldPos);
			const FVector CrosshairTraceEnd(CrosshairWorldPos + (CrosshairWorldDir * 50'000.f));
			OutHitLocation = CrosshairTraceEnd; // default hit location is as far as the trace went
			GetWorld()->LineTraceSingleByChannel(CrosshairsHit, CrosshairTraceStart, CrosshairTraceEnd, ECollisionChannel::ECC_Visibility);
			if (CrosshairsHit.bBlockingHit)
			{
				// Our crosshairs hit something, but still need to check if our bullet hit anything closer (possible unless we allow for changing aiming stance)
				OutHitLocation = CrosshairsHit.Location;
			}

			// Trace from weapon barrel socket to whatever the crosshairs hit
			FHitResult BulletHit;
			const FVector BulletTraceStart(BarrelSocketLocation);
			const FVector BulletTraceEnd(OutHitLocation);
			GetWorld()->LineTraceSingleByChannel(BulletHit, BulletTraceStart, BulletTraceEnd, ECollisionChannel::ECC_Visibility);
			if (BulletHit.bBlockingHit)
			{
				// Bullet hit something (might be the same as crosshairs, or something sooner)
				OutHitLocation = BulletHit.Location;
			}
			return true;			
		}
	}
	return false;
}

// Called every frame
void AProjectMarcusCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AProjectMarcusCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (ensure(PlayerInputComponent))
	{
		// Movement
		PlayerInputComponent->BindAxis("MoveForward", this, &AProjectMarcusCharacter::MoveForward);
		PlayerInputComponent->BindAxis("MoveRight", this, &AProjectMarcusCharacter::MoveRight);
		
		// Look rotations
		PlayerInputComponent->BindAxis("TurnRate", this, &AProjectMarcusCharacter::TurnAtRate);
		PlayerInputComponent->BindAxis("LookUpRate", this, &AProjectMarcusCharacter::LookUpAtRate);
		PlayerInputComponent->BindAxis("TurnMouse", this, &APawn::AddControllerYawInput);
		PlayerInputComponent->BindAxis("LookUpMouse", this, &APawn::AddControllerPitchInput);

		// Jump
		PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &ACharacter::Jump);
		PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Released, this, &ACharacter::StopJumping);

		// Weapon Input
		PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Pressed, this, &AProjectMarcusCharacter::FireWeapon);
		PlayerInputComponent->BindAction("AimButton", EInputEvent::IE_Pressed, this, &AProjectMarcusCharacter::AimButtonPressed);
		PlayerInputComponent->BindAction("AimButton", EInputEvent::IE_Released, this, &AProjectMarcusCharacter::AimButtonReleased);
	}
}

