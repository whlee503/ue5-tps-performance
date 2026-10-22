// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class UAnimMontage;

/** Walking enemy body; appearance is assigned in the derived Blueprint. */
UCLASS()
class MINITPS_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();
	void EndCombat();
	/** Visual response only; the AI owns attack timing and damage. */
	void PlayAttackAnimation();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	/** Optional material feedback after actual damage leaves this enemy alive. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Feedback")
	void ShowHitMaterialFeedback();

	/** Clears transient material feedback when combat ends. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Feedback")
	void ClearHitMaterialFeedback();

	/** Optional attack visual, assigned in the enemy Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	/** Optional visual for damage that leaves this enemy alive. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	/** Optional full-body death visual; game rules finish before playback. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Animation")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.f;

	/** Per-enemy runtime health, initialized from Blueprint defaults in BeginPlay. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Health")
	float CurrentHealth = 0.f;

private:
	/** Set only when lethal damage commits this enemy's death. */
	bool bIsDead = false;

	void StopAttackAnimation();
	void PlayHitReactAnimation();
	void StopHitReactAnimation();
	void PlayDeathAnimationAndScheduleRemoval();
};
