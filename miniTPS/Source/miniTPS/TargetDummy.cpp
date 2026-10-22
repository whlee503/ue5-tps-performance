// Copyright Epic Games, Inc. All Rights Reserved.

#include "TargetDummy.h"
#include "Components/StaticMeshComponent.h"
#include "miniTPS.h"

ATargetDummy::ATargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(true);

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	SetRootComponent(TargetMesh);
	TargetMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TargetMesh->SetSimulatePhysics(false);
}

void ATargetDummy::BeginPlay()
{
	// Blueprint defaults are available now; the constructor is too early for this copy.
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.f)
	{
		UE_LOG(LogminiTPS, Warning, TEXT("Target %s: invalid MaxHealth; using 100."), *GetName());
		MaxHealth = 100.f;
	}
	CurrentHealth = MaxHealth;
	Super::BeginPlay();
}

float ATargetDummy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	// Preserve the engine's damage notifications. Health is changed only in this class.
	const float AcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(AcceptedDamage) || AcceptedDamage <= 0.f)
	{
		return 0.f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - AcceptedDamage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	UE_LOG(LogminiTPS, Log, TEXT("Target %s: Damage=%.1f Health=%.1f/%.1f"),
		*GetName(), AppliedDamage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		SetCanBeDamaged(false);
		UE_LOG(LogminiTPS, Log, TEXT("Target %s: Destroyed"), *GetName());
		Destroy();
	}

	return AppliedDamage;
}
