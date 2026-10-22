// Copyright Epic Games, Inc. All Rights Reserved.

#include "miniTPSGameMode.h"
#include "EnemyCharacter.h"
#include "miniTPSCharacter.h"
#include "miniTPSPlayerController.h"
#include "miniTPS.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AminiTPSGameMode::AminiTPSGameMode()
{
}

void AminiTPSGameMode::StartPlay()
{
	if (bStartPlayHandled)
	{
		return;
	}
	bStartPlayHandled = true;
	// Keep Preparing while Super dispatches actors' BeginPlay events.
	Super::StartPlay();
	RoundPlayer = Cast<AminiTPSCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (IsValid(Enemy) && !Enemy->IsActorBeingDestroyed())
		{
			LivingEnemies.Add(TWeakObjectPtr<AEnemyCharacter>(Enemy));
		}
	}
	RemainingEnemies = LivingEnemies.Num();
	UE_LOG(LogminiTPS, Log, TEXT("Round %s: initial enemies=%d"), *GetClass()->GetName(), RemainingEnemies);
	if (RemainingEnemies == 0 || !RoundPlayer.IsValid())
	{
		UE_LOG(LogminiTPS, Warning, TEXT("Round stays Preparing: requires a miniTPS player and at least one EnemyCharacter."));
		return;
	}
	RoundState = EMiniTPSRoundState::Playing;
	UE_LOG(LogminiTPS, Log, TEXT("Round: Playing"));
	if (RoundPlayer->IsDead())
	{
		FinishRound(EMiniTPSRoundState::Defeat);
	}
}

void AminiTPSGameMode::NotifyEnemyDied(AEnemyCharacter* Enemy)
{
	if (!IsRoundPlaying() || !IsValid(Enemy)
		|| LivingEnemies.Remove(TWeakObjectPtr<AEnemyCharacter>(Enemy)) == 0)
	{
		return;
	}
	RemainingEnemies = LivingEnemies.Num();
	UE_LOG(LogminiTPS, Log, TEXT("Round: enemy %s died, remaining=%d"), *Enemy->GetName(), RemainingEnemies);
	if (RemainingEnemies == 0)
	{
		// Never award victory to a player whose death has already been committed.
		const bool bPlayerAlive = RoundPlayer.IsValid() && !RoundPlayer->IsActorBeingDestroyed() && !RoundPlayer->IsDead();
		FinishRound(bPlayerAlive ? EMiniTPSRoundState::Victory : EMiniTPSRoundState::Defeat);
	}
}

void AminiTPSGameMode::NotifyPlayerDied(AminiTPSCharacter* Player)
{
	if (IsRoundPlaying() && IsValid(Player) && Player == RoundPlayer.Get() && Player->IsDead())
	{
		FinishRound(EMiniTPSRoundState::Defeat);
	}
}

void AminiTPSGameMode::FinishRound(EMiniTPSRoundState Result)
{
	if (!IsRoundPlaying() || (Result != EMiniTPSRoundState::Victory && Result != EMiniTPSRoundState::Defeat))
	{
		return;
	}
	// Commit before cleanup callbacks: the first result cannot be overwritten.
	RoundState = Result;
	if (AminiTPSCharacter* Player = RoundPlayer.Get())
	{
		Player->EndCombat();
	}
	for (const TWeakObjectPtr<AEnemyCharacter>& Entry : LivingEnemies)
	{
		if (AEnemyCharacter* Enemy = Entry.Get())
		{
			Enemy->EndCombat();
		}
	}
	const bool bVictory = Result == EMiniTPSRoundState::Victory;
	UE_LOG(LogminiTPS, Log, TEXT("Round: %s, remaining=%d"), bVictory ? TEXT("Victory") : TEXT("Defeat"), RemainingEnemies);
	if (AminiTPSCharacter* Player = RoundPlayer.Get())
	{
		AminiTPSPlayerController* PlayerController = Cast<AminiTPSPlayerController>(Player->GetController());
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			PlayerController->ShowRoundResult(bVictory);
		}
	}
}
