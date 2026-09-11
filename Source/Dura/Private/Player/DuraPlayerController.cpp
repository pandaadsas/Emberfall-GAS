// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "Player/DuraPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Input/DuraEnhancedInputComponent.h"
#include "Input/DuraInputConfig.h"
#include "Interaction/HighlightInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DuraAbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "DuraGameplayTags.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/Character.h"
#include "UI/UserWidget/DamageTextComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Actor/MagicCircle.h"
#include "Dura/Dura.h"
#include "Interaction/EnemyInterface.h"

ADuraPlayerController::ADuraPlayerController()
{
	bReplicates = true;

	Spline = CreateDefaultSubobject<USplineComponent>("Spline");
}

void ADuraPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter, 
	bool bBlockedHit, bool bCriticalHit)
{
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount, bBlockedHit, bCriticalHit);
	}
}

void ADuraPlayerController::ShowMagicCircle(UMaterialInterface* DecalMaterial)
{
    if(!IsValid(MagicCircle))
    {
        MagicCircle = GetWorld()->SpawnActor<AMagicCircle>(MagicCircleClass);
        // 生成失败（MagicCircleClass 未配置）时跳过材质设置
        if(IsValid(MagicCircle) && DecalMaterial)
        {
            MagicCircle->SetMaterial(0, DecalMaterial);
        }
    }
}

void ADuraPlayerController::HideMagicCircle()
{
    if(IsValid(MagicCircle))
    {
        MagicCircle->Destroy();
    }
}

void ADuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(DuraContext);
	
	UEnhancedInputLocalPlayerSubsystem* SubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (SubSystem)
	{
		SubSystem->AddMappingContext(DuraContext, 0);
	}

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	FInputModeGameAndUI Inputmode;
	Inputmode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Inputmode.SetHideCursorDuringCapture(false);
	SetInputMode(Inputmode);
}

void ADuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(InputConfig);

	UDuraEnhancedInputComponent* DurainputComponent = CastChecked<UDuraEnhancedInputComponent>(InputComponent);
	check(MoveAction);
	check(ShiftAction);

	DurainputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, Move));
	DurainputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, ShiftPressed));
	DurainputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, ShiftReleased));

	for (const FDuraInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			FGameplayTag Tag = Action.InputTag;
			DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Started, 
                [this, Tag](const FInputActionValue& ActionValue) {
                    AbilityInputTagPressed(Tag);
                });
			DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Completed, 
                [this, Tag](const FInputActionValue& ActionValue) {
                    AbilityInputTagReleased(Tag);
                });
			DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Triggered, 
                [this, Tag](const FInputActionValue& ActionValue) {
                    AbilityInputTagHeld(Tag);
                });
		}
	}
}

void ADuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	MouseTrace();
	AutoRun();
    UpdateMagicCircleLocation();
}

void ADuraPlayerController::Move(const FInputActionValue& InputValue)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
        return;

    if(bAutoRunning)
    {
        bAutoRunning = false;
        TargetingStatus = ETargetingStatus::NotTargeting;
    }

	FVector2d AxisValue = InputValue.Get<FVector2d>();

	FRotator CtlRotation = GetControlRotation();
	CtlRotation.Pitch = 0.0f;
	CtlRotation.Roll = 0.0f;

	FVector ForwardVector = FRotationMatrix(CtlRotation).GetUnitAxis(EAxis::X);
	FVector RightVector = FRotationMatrix(CtlRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardVector, AxisValue.Y);
		ControlledPawn->AddMovementInput(RightVector, AxisValue.X);
	}
}

void ADuraPlayerController::MouseTrace()
{
	// 眩晕等状态通过 Player_Block_CursorTrace 屏蔽鼠标寻迹；同时清空过期缓存
	if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_CursorTrace))
	{
		UnHighlightActor(lastActor);
		lastActor = nullptr;
		thisActor = nullptr;
		hitResult = FHitResult();
		return;
	}

	FHitResult CursorHit;
	GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);
	if (!CursorHit.bBlockingHit)
	{
		// 光标移出可命中物体：清除上一帧高亮，避免高亮状态滞留
		UnHighlightActor(lastActor);
		lastActor = nullptr;
		thisActor = nullptr;
		return;
	}
	hitResult = CursorHit;
	thisActor = CursorHit.GetActor();
	
	if (IsValid(thisActor) && thisActor->Implements<UHighlightInterface>())
	{
		HighlightActor(thisActor);
	}
	else
	{
		UnHighlightActor(thisActor);
	}
	
	if (lastActor != thisActor)
	{
		UnHighlightActor(lastActor);
		lastActor = thisActor;
	}
}

void ADuraPlayerController::HighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_HighlightActor(InActor);
	}
}

void ADuraPlayerController::UnHighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_UnHighlightActor(InActor);
	}
}

void ADuraPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// 与 Released/Held 保持一致：眩晕等状态会屏蔽输入转发
	if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
		return;

	if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
		return;
	}

	if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
	{
		if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
	}
	else
	{
		const APawn* ControlledPawn = GetPawn();
		if (FollowTime <= ShortPressThreshold && ControlledPawn)
		{
			if (hitResult.bBlockingHit)  CachedDestination = hitResult.ImpactPoint;

			if (IsValid(thisActor) && thisActor->Implements<UEnemyInterface>())
			{
				TargetingStatus = ETargetingStatus::TargetingEnemy;
				bAutoRunning = false;
			}
		}

		if(GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
	}
}

void ADuraPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputReleased))
        return;

	if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
		return;
	}

	if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);

	if (TargetingStatus != ETargetingStatus::TargetingEnemy && !bShiftKeyDown)
	{
		const APawn* ControlledPawn = GetPawn();
		if (FollowTime <= ShortPressThreshold && ControlledPawn)
		{
            if(IsValid(thisActor) && thisActor->Implements<UHighlightInterface>())
            {
                IHighlightInterface::Execute_SetMoveToLocation(thisActor, CachedDestination);
            }
            else if(GetASC() && !GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ClickNiagaraSystem, CachedDestination);
            }

			if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ControlledPawn->GetActorLocation(), CachedDestination))
			{
				Spline->ClearSplinePoints();
				if (NavPath->PathPoints.Num() != 0)
				{
					for (const FVector& point : NavPath->PathPoints)
					{
						Spline->AddSplinePoint(point, ESplineCoordinateSpace::World);
					}

					if (NavPath->PathPoints.Num() > 0)
					{
						CachedDestination = NavPath->PathPoints[NavPath->PathPoints.Num() - 1];
						bAutoRunning = true;
					}
				}
			}        
		}
		FollowTime = 0.0f;
        TargetingStatus = ETargetingStatus::NotTargeting;
	}
}

void ADuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputHeld))
        return;

	if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
		return;
	}

	if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
	{
		if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
	}
	else
	{
		FollowTime += GetWorld()->GetDeltaSeconds();
		
		if (hitResult.bBlockingHit)  CachedDestination = hitResult.ImpactPoint;

		if (APawn* ControlledPawn = GetPawn())
		{
			const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
			ControlledPawn->AddMovementInput(WorldDirection);
		}
	}
}

UDuraAbilitySystemComponent* ADuraPlayerController::GetASC()
{
	if (DuraAbilitySystemComponent == nullptr)
	{
		DuraAbilitySystemComponent = 
			Cast<UDuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return DuraAbilitySystemComponent;
}

void ADuraPlayerController::AutoRun()
{
	if (bAutoRunning == false) return;

	if (APawn* ControlledPawn = GetPawn())
	{
		const FVector LocationOnSpline = Spline->
			FindLocationClosestToWorldLocation(ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);
		const FVector Direction = Spline->
			FindDirectionClosestToWorldLocation(LocationOnSpline, ESplineCoordinateSpace::World);
		ControlledPawn->AddMovementInput(Direction);

		const float DistanceToDestination = (LocationOnSpline - CachedDestination).Length();
		if (DistanceToDestination <= AutoRunAcceptanceRadius)  
            bAutoRunning = false;
	}
}

void ADuraPlayerController::UpdateMagicCircleLocation()
{
    if(IsValid(MagicCircle))
    {
        MagicCircle->SetActorLocation(hitResult.ImpactPoint);
    }
}
