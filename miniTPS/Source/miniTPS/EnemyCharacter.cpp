// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"
#include "EnemyAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "miniTPS.h"
#include "miniTPSGameMode.h"
#include "Engine/World.h"

AEnemyCharacter::AEnemyCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	// Shots use Visibility; preserve the capsule's other Pawn collision responses.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Enemies stay out of ray tracing (Lumen HWRT reflections/shadows/GI); the player is unchanged.
	// Avoids the per-frame RT skinning and BLAS work measured in P4-02/P4-03.
	GetMesh()->bVisibleInRayTracing = false;
	SetCanBeDamaged(true);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void AEnemyCharacter::BeginPlay()
{
	// Initialize before Super dispatches Blueprint BeginPlay events.
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.f)
	{
		UE_LOG(LogminiTPS, Warning, TEXT("Enemy %s: invalid MaxHealth; using 100."), *GetName());
		MaxHealth = 100.f;
	}
	CurrentHealth = MaxHealth;
	Super::BeginPlay();
	// Report the applied value, not a forced one, so a Blueprint override stays visible.
	UE_LOG(LogminiTPS, Log, TEXT("Enemy %s: VisibleInRayTracing=%d"),
		*GetName(), GetMesh()->bVisibleInRayTracing ? 1 : 0);
}

float AEnemyCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	AminiTPSGameMode* GameMode = GetWorld()->GetAuthGameMode<AminiTPSGameMode>();
	if (!IsValid(GameMode) || !GameMode->IsRoundPlaying()
		|| bIsDead || !CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	// Preserve the engine's damage notifications; only this class subtracts health.
	const float AcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	// Damage callbacks may have changed the actor's state or destroyed it.
	if (!IsValid(GameMode) || !GameMode->IsRoundPlaying()
		|| bIsDead || !CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(AcceptedDamage) || AcceptedDamage <= 0.f)
	{
		return 0.f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - AcceptedDamage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	UE_LOG(LogminiTPS, Log, TEXT("Enemy %s: Damage=%.1f Health=%.1f/%.1f"),
		*GetName(), AppliedDamage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		// Block repeated death handling before movement or destruction callbacks run.
		bIsDead = true;
		EndCombat();
		SetActorEnableCollision(false);
		UE_LOG(LogminiTPS, Log, TEXT("Enemy %s: Died"), *GetName());
		GameMode->NotifyEnemyDied(this);
		// The round result is immediate; only the visual body's removal is delayed.
		PlayDeathAnimationAndScheduleRemoval();
	}
	else if (AppliedDamage > 0.f)
	{
		ShowHitMaterialFeedback();
		PlayHitReactAnimation();
	}

	return AppliedDamage;
}

void AEnemyCharacter::PlayAttackAnimation()
{
	UAnimMontage* Montage = AttackMontage.Get();
	if (!CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f || !IsValid(Montage))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance))
	{
		return;
	}

	// The latest allowed visual wins; the AI still owns attack timing and damage.
	if (UAnimMontage* HitMontage = HitReactMontage.Get(); IsValid(HitMontage))
	{
		AnimInstance->Montage_Stop(0.f, HitMontage);
	}
	AnimInstance->Montage_Stop(0.f, Montage);
	AnimInstance->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false);
}

void AEnemyCharacter::StopAttackAnimation()
{
	UAnimMontage* Montage = AttackMontage.Get();
	if (!IsValid(Montage))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance))
	{
		AnimInstance->Montage_Stop(0.1f, Montage);
	}
}

void AEnemyCharacter::PlayHitReactAnimation()
{
	UAnimMontage* Montage = HitReactMontage.Get();
	if (!CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f || !IsValid(Montage))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance))
	{
		return;
	}

	// Replace only the two known visuals; accepted damage does not depend on playback.
	if (UAnimMontage* Attack = AttackMontage.Get(); IsValid(Attack))
	{
		AnimInstance->Montage_Stop(0.f, Attack);
	}
	AnimInstance->Montage_Stop(0.f, Montage);
	AnimInstance->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false);
}

void AEnemyCharacter::StopHitReactAnimation()
{
	UAnimMontage* Montage = HitReactMontage.Get();
	if (!IsValid(Montage))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance))
	{
		AnimInstance->Montage_Stop(0.1f, Montage);
	}
}

void AEnemyCharacter::PlayDeathAnimationAndScheduleRemoval()
{
	// Victory may already be committed, and damage has intentionally been disabled.
	if (!bIsDead || IsActorBeingDestroyed())
	{
		return;
	}

	UAnimMontage* Montage = DeathMontage.Get();
	if (!IsValid(Montage))
	{
		Destroy();
		return;
	}

	const float MontageLength = Montage->GetPlayLength();
	if (!FMath::IsFinite(MontageLength) || MontageLength <= 0.f
		|| !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
	{
		Destroy();
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance))
	{
		Destroy();
		return;
	}

	const float Duration = AnimInstance->Montage_Play(Montage, 1.f,
		EMontagePlayReturnType::Duration, 0.f, false);
	// Montage-start callbacks may have destroyed the actor.
	if (IsActorBeingDestroyed())
	{
		return;
	}
	if (!FMath::IsFinite(Duration) || Duration <= 0.f)
	{
		Destroy();
		return;
	}

	// Auto blend-out is disabled on the asset to hold its last pose until removal.
	// A positive bounded lifespan also cleans up interrupted or misconfigured visuals.
	const float RemovalDelay = FMath::Clamp(Duration + 0.2f, 0.1f, 10.f);
	SetLifeSpan(RemovalDelay);
}

void AEnemyCharacter::EndCombat()
{
	SetCanBeDamaged(false);
	ClearHitMaterialFeedback();
	if (AEnemyAIController* EnemyController = Cast<AEnemyAIController>(GetController()))
	{
		EnemyController->EndCombat();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	StopAttackAnimation();
	StopHitReactAnimation();
}
