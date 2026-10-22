// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyAIController.h"
#include "EnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "miniTPS.h"
#include "miniTPSCharacter.h"
#include "miniTPSGameMode.h"
#include "CollisionQueryParams.h"
#include "GameFramework/DamageType.h"

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ClearChase();
	NextFailureLogTime = 0.0;
	NextAllowedAttackTime = 0.0;
	UE_LOG(LogminiTPS, Log, TEXT("Enemy AI %s: possessed %s"), *GetName(), *GetNameSafe(GetPawn()));

	GetWorldTimerManager().SetTimer(ChaseTimerHandle, this, &AEnemyAIController::UpdateChase, 0.5f, true);
	UpdateChase();
}

void AEnemyAIController::UpdateChase()
{
	const AminiTPSGameMode* GameMode = GetWorld()->GetAuthGameMode<AminiTPSGameMode>();
	if (!IsValid(GameMode) || !GameMode->IsRoundPlaying())
	{
		if (IsValid(GameMode) && GameMode->GetRoundState() != EMiniTPSRoundState::Preparing)
		{
			ClearChase();
		}
		// Keep the timer during Preparing so StartPlay can finish initialization.
		return;
	}
	APawn* EnemyPawn = GetPawn();
	if (!IsValid(EnemyPawn) || EnemyPawn->IsActorBeingDestroyed())
	{
		ClearChase();
		return;
	}

	AminiTPSCharacter* PlayerPawn = Cast<AminiTPSCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!IsValid(PlayerPawn))
	{
		if (GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			StopMovement();
		}
		ChaseTarget.Reset();
		return;
	}

	// Possession can precede BeginPlay. Wait without treating zero health as death.
	if (!EnemyPawn->HasActorBegunPlay() || !PlayerPawn->HasActorBegunPlay())
	{
		return;
	}
	if (!EnemyPawn->CanBeDamaged() || PlayerPawn->IsDead() || PlayerPawn->IsActorBeingDestroyed())
	{
		ClearChase();
		return;
	}

	if (ChaseTarget.Get() != PlayerPawn)
	{
		StopMovement();
		ChaseTarget = PlayerPawn;
	}

	UWorld* World = GetWorld();
	const FVector EnemyLocation = EnemyPawn->GetActorLocation();
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const float EffectiveRange = FMath::IsFinite(AttackRange) && AttackRange > 0.f ? AttackRange : 200.f;
	const bool bInAttackRange = FVector::DistSquared(EnemyLocation, PlayerLocation)
		<= FMath::Square(static_cast<double>(EffectiveRange));
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyMeleeOcclusion), false, EnemyPawn);
	QueryParams.AddIgnoredActor(PlayerPawn);
	const bool bBlocked = World->LineTraceTestByChannel(EnemyLocation, PlayerLocation, ECC_Visibility, QueryParams);

	// Check combat before the active-path early return so walking enemies can attack.
	if (bInAttackRange && !bBlocked)
	{
		if (GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			StopMovement();
		}
		const double Now = World->GetTimeSeconds();
		if (Now < NextAllowedAttackTime)
		{
			return;
		}
		const float EffectiveInterval = FMath::IsFinite(AttackInterval) && AttackInterval > 0.f
			? FMath::Max(AttackInterval, 0.01f) : 1.f;
		const float EffectiveDamage = FMath::IsFinite(AttackDamage) && AttackDamage >= 0.f ? AttackDamage : 20.f;
		// Update before damage callbacks; no catch-up attacks or reset on range changes.
		NextAllowedAttackTime = Now + static_cast<double>(EffectiveInterval);
		// Start the visual before damage can finish the round; damage is independent of playback.
		if (AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(EnemyPawn))
		{
			EnemyCharacter->PlayAttackAnimation();
		}
		UGameplayStatics::ApplyDamage(PlayerPawn, EffectiveDamage, this, EnemyPawn, UDamageType::StaticClass());
		if (!IsValid(PlayerPawn) || PlayerPawn->IsDead() || PlayerPawn->IsActorBeingDestroyed())
		{
			ClearChase();
		}
		return;
	}

	// Moving/Waiting requests already track the actor. Do not restart their paths.
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	const float EffectiveRadius = FMath::IsFinite(AcceptanceRadius) && AcceptanceRadius >= 0.f
		? AcceptanceRadius : 100.f;
	FAIMoveRequest MoveRequest(PlayerPawn);
	MoveRequest.SetAcceptanceRadius(EffectiveRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	// A close target behind a wall must not count as reached through that wall.
	// Also approach more closely if the normal reach test passes outside attack range.
	if (bBlocked || GetPathFollowingComponent()->HasReached(MoveRequest))
	{
		MoveRequest.SetAcceptanceRadius(1.f);
		MoveRequest.SetReachTestIncludesAgentRadius(false);
		MoveRequest.SetReachTestIncludesGoalRadius(false);
	}
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetNavigationFilter(DefaultNavigationFilterClass);
	// MoveToActor wraps MoveTo; use the request directly to control both reach radii.
	const FPathFollowingRequestResult Result = MoveTo(MoveRequest);
	if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		ReportMoveFailure(TEXT("request failed; check NavMesh coverage and a complete route to the player"));
	}
}

void AEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	// Aborted includes intentional StopMovement during target changes and cleanup.
	if (Result.Code != EPathFollowingResult::Success && Result.Code != EPathFollowingResult::Aborted)
	{
		ReportMoveFailure(FString::Printf(TEXT("move ended: %s"), *Result.ToString()));
	}
	// Retry on the timer, never recursively from an immediate completion callback.
}

void AEnemyAIController::ReportMoveFailure(const FString& Reason)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextFailureLogTime)
	{
		UE_LOG(LogminiTPS, Warning, TEXT("Enemy AI %s: %s (retry every 0.5s; warnings limited to every 5s)"),
			*GetName(), *Reason);
		NextFailureLogTime = Now + 5.0;
	}
}

void AEnemyAIController::ClearChase()
{
	GetWorldTimerManager().ClearTimer(ChaseTimerHandle);
	StopMovement();
	ChaseTarget.Reset();
}

void AEnemyAIController::EndCombat()
{
	ClearChase();
}

void AEnemyAIController::OnUnPossess()
{
	ClearChase();
	Super::OnUnPossess();
}

void AEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearChase();
	Super::EndPlay(EndPlayReason);
}
