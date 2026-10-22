// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "miniTPSGameMode.generated.h"

class AEnemyCharacter;
class AminiTPSCharacter;

UENUM(BlueprintType)
enum class EMiniTPSRoundState : uint8
{
	Preparing,
	Playing,
	Victory,
	Defeat
};

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AminiTPSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AminiTPSGameMode();
	virtual void StartPlay() override;

	UFUNCTION(BlueprintPure, Category="Round")
	EMiniTPSRoundState GetRoundState() const { return RoundState; }

	UFUNCTION(BlueprintPure, Category="Round")
	bool IsRoundPlaying() const { return RoundState == EMiniTPSRoundState::Playing; }

	UFUNCTION(BlueprintPure, Category="Round")
	int32 GetRemainingEnemies() const { return RemainingEnemies; }

	void NotifyEnemyDied(AEnemyCharacter* Enemy);
	void NotifyPlayerDied(AminiTPSCharacter* Player);

private:
	void FinishRound(EMiniTPSRoundState Result);

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Round")
	EMiniTPSRoundState RoundState = EMiniTPSRoundState::Preparing;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Round")
	int32 RemainingEnemies = 0;

	TSet<TWeakObjectPtr<AEnemyCharacter>> LivingEnemies;
	TWeakObjectPtr<AminiTPSCharacter> RoundPlayer;
	bool bStartPlayHandled = false;
};



