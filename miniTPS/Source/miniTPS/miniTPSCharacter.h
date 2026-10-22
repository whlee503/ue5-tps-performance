// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "miniTPSCharacter.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AminiTPSCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** Temporary firing point; adjust its relative transform in the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ShotOrigin;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Health")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Health")
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Round")
	bool bCombatEnded = false;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Fire Input Action assigned by the character Blueprint */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> FireAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> AimAction = nullptr;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Aim")
	bool bIsAiming = false;

	/** Degrees removed from the Blueprint camera's normal FOV while aiming. */
	UPROPERTY(EditDefaultsOnly, Category="Aim", meta=(ClampMin="0.0", ClampMax="60.0"))
	float AimFOVReduction = 15.f;

	UPROPERTY(EditDefaultsOnly, Category="Aim", meta=(ClampMin="0.1"))
	float AimFOVInterpSpeed = 12.f;

	float NormalFOV = 90.f;
	bool bAimDefaultsCached = false;
	bool bNormalUseControllerRotationYaw = false;
	bool bNormalOrientRotationToMovement = true;
	bool bNormalUseControllerDesiredRotation = false;

	/** Maximum camera trace distance in centimeters */
	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1.0", Units="cm"))
	float TraceDistance = 10000.f;

	/** Damage delivered to the first actor hit by each shot. */
	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.0"))
	float ShotDamage = 25.f;

	/** Minimum game-time seconds between accepted shots. */
	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.01", Units="s"))
	float FireInterval = 0.25f;

	/** Show shot trace shapes and on-screen diagnostics when enabled in the character Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Debug")
	bool bShowShotDebug = false;

	/** Per-instance runtime state; the first shot is immediately available. */
	double NextAllowedFireTime = 0.0;

public:

	/** Constructor */
	AminiTPSCharacter();	
	virtual void Tick(float DeltaSeconds) override;
	virtual void UnPossessed() override;
	void EndCombat();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category="Aim")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

protected:
	virtual void BeginPlay() override;
	bool IsCombatActive() const;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called once when the fire input starts */
	void Fire();

	void StartAim();
	void StopAim();
	void SetAiming(bool bNewAiming, bool bResetCameraImmediately = false);
	void AlignAimYaw();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
