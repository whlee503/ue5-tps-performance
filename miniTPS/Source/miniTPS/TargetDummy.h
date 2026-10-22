// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetDummy.generated.h"

class UStaticMeshComponent;

/** A stationary target with health, configured visually in a Blueprint. */
UCLASS()
class MINITPS_API ATargetDummy : public AActor
{
	GENERATED_BODY()

public:
	ATargetDummy();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	/** The Blueprint assigns the mesh asset to this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.f;

	/** Runtime state belongs to each target instance, not to the Blueprint default. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Health")
	float CurrentHealth = 0.f;
};
