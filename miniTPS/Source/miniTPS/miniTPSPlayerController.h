// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "miniTPSPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AminiTPSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** Implemented by the controller Blueprint to show the local result UI. */
	UFUNCTION(BlueprintImplementableEvent, Category="Round")
	void ShowRoundResult(bool bVictory);

	/** Implemented by the local controller Blueprint to show confirmed damage feedback. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ShowHitMarker();

	/** Implemented by the local controller Blueprint to show an accepted shot. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ShowFireFeedback();

	/** Implemented by the local controller Blueprint to show nonlethal player damage feedback. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ShowDamageFeedback();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
