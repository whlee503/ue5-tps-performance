// Copyright Epic Games, Inc. All Rights Reserved.

#include "miniTPSCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionShape.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "miniTPS.h"
#include "miniTPSPlayerController.h"
#include "miniTPSGameMode.h"
#include "EnemyCharacter.h"
#include "TargetDummy.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

AminiTPSCharacter::AminiTPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(true);
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	ShotOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("ShotOrigin"));
	ShotOrigin->SetupAttachment(GetCapsuleComponent());
	ShotOrigin->SetRelativeLocation(FVector(20.f, 20.f, 40.f));
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AminiTPSCharacter::BeginPlay()
{
	// Capture Blueprint camera/rotation defaults before any aiming state changes.
	const float CameraFOV = FollowCamera->FieldOfView;
	NormalFOV = FMath::IsFinite(CameraFOV) ? FMath::Clamp(CameraFOV, 5.f, 170.f) : 90.f;
	bNormalUseControllerRotationYaw = bUseControllerRotationYaw;
	bNormalOrientRotationToMovement = GetCharacterMovement()->bOrientRotationToMovement;
	bNormalUseControllerDesiredRotation = GetCharacterMovement()->bUseControllerDesiredRotation;
	bAimDefaultsCached = true;
	SetAiming(false, true);

	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.f)
	{
		UE_LOG(LogminiTPS, Warning, TEXT("Player %s: invalid MaxHealth; using 100."), *GetName());
		MaxHealth = 100.f;
	}
	CurrentHealth = MaxHealth;
	bIsDead = false;
	bCombatEnded = false;
	NextAllowedFireTime = 0.0;
	SetCanBeDamaged(true);
	Super::BeginPlay();
}

void AminiTPSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAimDefaultsCached)
	{
		return;
	}
	if (bIsAiming && (!IsCombatActive() || !IsValid(GetController())))
	{
		SetAiming(false, true);
	}

	const float Reduction = FMath::IsFinite(AimFOVReduction) ? FMath::Clamp(AimFOVReduction, 0.f, 60.f) : 15.f;
	const float TargetFOV = bIsAiming ? FMath::Clamp(NormalFOV - Reduction, 5.f, NormalFOV) : NormalFOV;
	const float InterpSpeed = FMath::IsFinite(AimFOVInterpSpeed) && AimFOVInterpSpeed > 0.f
		? AimFOVInterpSpeed : 12.f;
	const float NewFOV = FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, InterpSpeed);
	FollowCamera->SetFieldOfView(FMath::IsNearlyEqual(NewFOV, TargetFOV, 0.01f) ? TargetFOV : NewFOV);
}

void AminiTPSCharacter::StartAim()
{
	SetAiming(true);
}

void AminiTPSCharacter::StopAim()
{
	SetAiming(false);
}

void AminiTPSCharacter::SetAiming(bool bNewAiming, bool bResetCameraImmediately)
{
	if (bNewAiming && (!IsCombatActive() || !bAimDefaultsCached || !IsValid(GetController())))
	{
		return;
	}
	bIsAiming = bNewAiming;
	if (!bAimDefaultsCached)
	{
		return;
	}

	bUseControllerRotationYaw = bIsAiming ? true : bNormalUseControllerRotationYaw;
	GetCharacterMovement()->bOrientRotationToMovement = bIsAiming ? false : bNormalOrientRotationToMovement;
	GetCharacterMovement()->bUseControllerDesiredRotation = bIsAiming ? false : bNormalUseControllerDesiredRotation;
	if (bIsAiming)
	{
		AlignAimYaw();
	}
	else if (bResetCameraImmediately)
	{
		FollowCamera->SetFieldOfView(NormalFOV);
	}
}

void AminiTPSCharacter::AlignAimYaw()
{
	if (const AController* AimController = GetController())
	{
		FRotator BodyRotation = GetActorRotation();
		BodyRotation.Yaw = AimController->GetControlRotation().Yaw;
		SetActorRotation(BodyRotation);
	}
}

void AminiTPSCharacter::UnPossessed()
{
	SetAiming(false, true);
	Super::UnPossessed();
}

float AminiTPSCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsCombatActive() || !CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	const float AcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!IsCombatActive() || !CanBeDamaged() || IsActorBeingDestroyed() || CurrentHealth <= 0.f
		|| !FMath::IsFinite(AcceptedDamage) || AcceptedDamage <= 0.f)
	{
		return 0.f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - AcceptedDamage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	UE_LOG(LogminiTPS, Log, TEXT("Player %s: Damage=%.1f Health=%.1f/%.1f [GameTime=%.9f]"),
		*GetName(), AppliedDamage, CurrentHealth, MaxHealth, GetWorld()->GetTimeSeconds());

	if (CurrentHealth <= 0.f)
	{
		// Commit death before callbacks; retain possession and the current camera.
		bIsDead = true;
		EndCombat();
		UE_LOG(LogminiTPS, Log, TEXT("Player %s: Died"), *GetName());
		if (AminiTPSGameMode* GameMode = GetWorld()->GetAuthGameMode<AminiTPSGameMode>())
		{
			GameMode->NotifyPlayerDied(this);
		}
	}
	// Lethal damage shows the result UI instead; only surviving damage flashes the HUD.
	if (FMath::IsFinite(AppliedDamage) && AppliedDamage > 0.f && CurrentHealth > 0.f && IsCombatActive())
	{
		AminiTPSPlayerController* LocalController = Cast<AminiTPSPlayerController>(GetController());
		if (IsValid(LocalController) && LocalController->IsLocalController())
		{
			LocalController->ShowDamageFeedback();
		}
	}
	return AppliedDamage;
}

bool AminiTPSCharacter::IsCombatActive() const
{
	const AminiTPSGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AminiTPSGameMode>() : nullptr;
	return !bIsDead && !bCombatEnded && IsValid(GameMode) && GameMode->IsRoundPlaying();
}

void AminiTPSCharacter::EndCombat()
{
	if (bCombatEnded)
	{
		return;
	}
	bCombatEnded = true;
	// Shared by death and victory. Do not change health or bIsDead here.
	SetAiming(false, true);
	SetCanBeDamaged(false);
	StopJumping();
	ConsumeMovementInputVector();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		DisableInput(PlayerController);
	}
}

void AminiTPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AminiTPSCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AminiTPSCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AminiTPSCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AminiTPSCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AminiTPSCharacter::Look);

		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AminiTPSCharacter::StartAim);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AminiTPSCharacter::StopAim);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &AminiTPSCharacter::StopAim);
		}
		else
		{
			UE_LOG(LogminiTPS, Warning, TEXT("'%s': AimAction is not assigned. Set Aim Action in the character Blueprint; firing requires aiming."), *GetNameSafe(this));
		}

		// A press starts one shot; holding the input does not repeat it.
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AminiTPSCharacter::Fire);
		}
		else
		{
			UE_LOG(LogminiTPS, Warning, TEXT("'%s': FireAction is not assigned. Set Fire Action in the character Blueprint."), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogminiTPS, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AminiTPSCharacter::Fire()
{
	if (!IsCombatActive() || !bIsAiming)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PlayerController || !World)
	{
		return;
	}

	const double FireTime = World->GetTimeSeconds();
	if (FireTime < NextAllowedFireTime)
	{
		// Ignore this press without extending the cooldown or queuing a shot.
		return;
	}

	// Enforce a valid interval even when values bypass the editor's ClampMin.
	const float EffectiveFireInterval = FMath::IsFinite(FireInterval) && FireInterval > 0.f
		? FMath::Max(FireInterval, 0.01f) : 0.25f;
	// Every accepted shot consumes the interval, including misses.
	NextAllowedFireTime = FireTime + static_cast<double>(EffectiveFireInterval);
	// Align the attached shot origin even if aiming and firing started in this frame.
	AlignAimYaw();

	// The camera chooses an aim point, not the actor that receives damage.
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const double MaxRange = FMath::IsFinite(TraceDistance) && TraceDistance > 0.f
		? static_cast<double>(TraceDistance) : 10000.0;
	const FVector CameraEnd = ViewLocation + ViewDirection * MaxRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CameraFireTrace), false, this);
	FHitResult AimHit;
	const bool bAimHit = World->LineTraceSingleByChannel(
		AimHit, ViewLocation, CameraEnd, ECC_Visibility, QueryParams);
	const FVector AimPoint = bAimHit ? AimHit.ImpactPoint : CameraEnd;
	const FString AimName = bAimHit ? GetNameSafe(AimHit.GetActor()) : TEXT("Miss");

	if (bShowShotDebug)
	{
		DrawDebugSphere(World, AimPoint, 5.f, 12, FColor::Cyan, false, 1.f);
	}

	// All result reporting precedes damage, which may destroy the hit actor.
	const auto ReportShot = [&](const FString& Result, const FColor& Color)
	{
		UE_LOG(LogminiTPS, Log, TEXT("Fire Aim=%s | %s [GameTime=%.9f Interval=%.9f]"),
			*AimName, *Result, FireTime, static_cast<double>(EffectiveFireInterval));
		if (bShowShotDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 2.f, Color, Result);
		}
	};

	if (!IsValid(ShotOrigin))
	{
		ReportShot(TEXT("Shot Blocked: MissingOrigin"), FColor::Orange);
		return;
	}

	const FVector ShotStart = ShotOrigin->GetComponentLocation();
	const FVector CapsuleCenter = GetCapsuleComponent()->GetComponentLocation();
	if (bShowShotDebug)
	{
		DrawDebugSphere(World, ShotStart, 4.f, 12, FColor::Yellow, false, 1.f);
	}

	// Catch an origin moved through a wall, as well as an origin starting inside one.
	FHitResult OriginPathHit;
	const bool bOriginPathBlocked = World->LineTraceSingleByChannel(
		OriginPathHit, CapsuleCenter, ShotStart, ECC_Visibility, QueryParams);
	const bool bOriginOverlapping = World->OverlapBlockingTestByChannel(
		ShotStart, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(1.f), QueryParams);
	if (bOriginPathBlocked || bOriginOverlapping)
	{
		if (bShowShotDebug)
		{
			DrawDebugLine(World, CapsuleCenter, ShotStart, FColor::Orange, false, 1.f, 0, 2.f);
			DrawDebugSphere(World, ShotStart, 8.f, 12, FColor::Orange, false, 1.f);
		}
		const FString Reason = bOriginPathBlocked
			? FString::Printf(TEXT("Shot Blocked: OriginPath (%s)"), *GetNameSafe(OriginPathHit.GetActor()))
			: TEXT("Shot Blocked: OriginOverlap");
		ReportShot(Reason, FColor::Orange);
		return;
	}

	const FVector ToAim = AimPoint - ShotStart;
	const double AimDistance = ToAim.Size();
	// A camera compressed against a wall can select a point behind the firing origin.
	if (AimDistance <= 1.0 || FVector::DotProduct(ToAim, ViewDirection) <= 0.0)
	{
		ReportShot(TEXT("Shot Blocked: AimTooCloseOrBehind"), FColor::Orange);
		return;
	}

	const FVector ShotDirection = ToAim / AimDistance;
	// Extend 1 cm beyond the aim surface to avoid an exact-endpoint miss, capped by range.
	const double ShotLength = FMath::Min(AimDistance + 1.0, MaxRange);
	const FVector ShotEnd = ShotStart + ShotDirection * ShotLength;
	FHitResult ShotHit;
	const bool bShotHit = World->LineTraceSingleByChannel(
		ShotHit, ShotStart, ShotEnd, ECC_Visibility, QueryParams);
	if (bShotHit && ShotHit.bStartPenetrating)
	{
		ReportShot(TEXT("Shot Blocked: StartPenetrating"), FColor::Orange);
		return;
	}

	const FColor ShotColor = bShotHit ? FColor::Green : FColor::Red;
	if (bShowShotDebug)
	{
		const FVector DebugEnd = bShotHit ? ShotHit.ImpactPoint : ShotEnd;
		DrawDebugLine(World, ShotStart, DebugEnd, ShotColor, false, 1.f, 0, 2.f);
		if (bShotHit)
		{
			DrawDebugSphere(World, ShotHit.ImpactPoint, 8.f, 12, ShotColor, false, 1.f);
		}
	}
	const FString Result = bShotHit
		? FString::Printf(TEXT("Shot Hit: %s at %s"), *GetNameSafe(ShotHit.GetActor()), *ShotHit.ImpactPoint.ToString())
		: TEXT("Shot Miss");
	ReportShot(Result, ShotColor);
	// This shot passed every firing guard; misses and world hits still get a fire response.
	AminiTPSPlayerController* FireFeedbackController = Cast<AminiTPSPlayerController>(PlayerController);
	if (IsValid(FireFeedbackController) && FireFeedbackController->IsLocalController())
	{
		FireFeedbackController->ShowFireFeedback();
	}

	// Only the actual shot can deal damage; never fall back to the camera hit.
	AActor* HitActor = ShotHit.GetActor();
	if (bShotHit && IsValid(HitActor) && FMath::IsFinite(ShotDamage) && ShotDamage > 0.f)
	{
		// Damage may destroy the target, so classify it before the call.
		const bool bFeedbackTarget = HitActor->IsA<AEnemyCharacter>() || HitActor->IsA<ATargetDummy>();
		const float AppliedDamage = UGameplayStatics::ApplyPointDamage(HitActor, ShotDamage, ShotDirection,
			ShotHit, PlayerController, this, UDamageType::StaticClass());
		// Killing the last enemy ends the round inside TakeDamage; the result UI takes priority.
		if (bFeedbackTarget && FMath::IsFinite(AppliedDamage) && AppliedDamage > 0.f && IsCombatActive())
		{
			AminiTPSPlayerController* LocalController = Cast<AminiTPSPlayerController>(PlayerController);
			if (IsValid(LocalController) && LocalController->IsLocalController())
			{
				LocalController->ShowHitMarker();
			}
		}
	}
}

void AminiTPSCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AminiTPSCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AminiTPSCharacter::DoMove(float Right, float Forward)
{
	if (!IsCombatActive())
	{
		return;
	}

	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AminiTPSCharacter::DoLook(float Yaw, float Pitch)
{
	if (!IsCombatActive())
	{
		return;
	}

	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AminiTPSCharacter::DoJumpStart()
{
	if (!IsCombatActive())
	{
		return;
	}

	// signal the character to jump
	Jump();
}

void AminiTPSCharacter::DoJumpEnd()
{
	if (!IsCombatActive())
	{
		return;
	}

	// signal the character to stop jumping
	StopJumping();
}
