// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "EnemyAIController.generated.h"

/** Follows the single local player using navigation, without a behavior tree. */
UCLASS()
class MINITPS_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	void EndCombat();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Additional reach tolerance; the engine also includes agent/goal radii. */
	UPROPERTY(EditDefaultsOnly, Category="Chase", meta=(ClampMin="0.0", Units="cm"))
	float AcceptanceRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1.0", Units="cm"))
	float AttackRange = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.0"))
	float AttackDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.01", Units="s"))
	float AttackInterval = 1.f;

private:
	void UpdateChase();
	void ClearChase();
	void ReportMoveFailure(const FString& Reason);

	TWeakObjectPtr<APawn> ChaseTarget;
	FTimerHandle ChaseTimerHandle;
	double NextFailureLogTime = 0.0;
	double NextAllowedAttackTime = 0.0;
};
