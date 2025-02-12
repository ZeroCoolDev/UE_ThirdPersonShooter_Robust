#include "ProjectMarcus/Enemies/Enemy.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Blueprint/UserWidget.h"

// Sets default values
AEnemy::AEnemy()
	: MaxHealth(100.f)
	, HealthBarDisplayTime(4.f)
	, HitReactIntervalMin(0.25f)
	, HitReactIntervalMax(2.f)
	, HitNumberLifetime(1.5f)
{
	Health = MaxHealth;

 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// Allow mesh to collide with bullet line traces
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);//Set to whatever channel being used for bullets
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &AEnemy::HideHealthBar, HealthBarDisplayTime);
}

void AEnemy::Die()
{
	HideHealthBar();
}

void AEnemy::PlayHitMontage(FName Section, float PlayRate /*= 1.f*/)
{
	if (GetWorldTimerManager().IsTimerActive(HitReactTimer))
		return;

	const float HitReactDelay = FMath::FRandRange(HitReactIntervalMin, HitReactIntervalMax);
	GetWorldTimerManager().SetTimer(HitReactTimer, HitReactDelay, false);

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(HitReactMontage, PlayRate);
		AnimInstance->Montage_JumpToSection(Section, HitReactMontage);
	}
}

void AEnemy::StoreHitNumber(UUserWidget* HitNumber, FVector HitLocation)
{
	HitNumbers.Add(HitNumber, HitLocation);

	FTimerHandle HitNumberTimer;
	FTimerDelegate HitNumberDelegate;
	HitNumberDelegate.BindUFunction(this, FName("DestroyHitNumber"), HitNumber);

	GetWorld()->GetTimerManager().SetTimer(HitNumberTimer, HitNumberDelegate, HitNumberLifetime, false);
}

void AEnemy::DestroyHitNumber(UUserWidget* HitNumber)
{
	HitNumbers.Remove(HitNumber);
	HitNumber->RemoveFromParent();// Removes from viewport
}

void AEnemy::UpdateHitNumbers()
{
	for (TPair<UUserWidget*, FVector>& HitNumberPair : HitNumbers)
	{
		UUserWidget* HitNumber = HitNumberPair.Key;
		const FVector HitLocation = HitNumberPair.Value;
		
		FVector2D OutScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), HitLocation, OutScreenPosition);

		HitNumber->SetPositionInViewport(OutScreenPosition);
	}
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitNumbers();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

float AEnemy::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	if (Health <= 0.f)
		Die();

	return Damage;
}

void AEnemy::OnBulletHit_Implementation(const FHitResult& HitResult)
{
	if (ImpactSound)
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());

	if (ImpactParticles)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, HitResult.Location, FRotator(0.f), true);

	PlayHitMontage(FName("HitReact_Front"));//TODO: Let's not use string literals

	ShowHealthBar();
}

