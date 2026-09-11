// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "Character/DuraCharacterBase.h"
#include "Interaction/HighlightInterface.h"
#include "Interaction/EnemyInterface.h"
#include "UI/WidgetController/DuraOverlayWidgetController.h"
#include "DuraEnemy.generated.h"

class UWidgetComponent;
class UBehaviorTree;
class ADuraAIController;
/**
 * 
 */
UCLASS()
class DURA_API ADuraEnemy : public ADuraCharacterBase, public IEnemyInterface, public IHighlightInterface
{
	GENERATED_BODY()
	
public:
	ADuraEnemy();

	//** 敌人接口（IEnemyInterface）
    virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;
	//** 敌人接口结束（IEnemyInterface）

    //** 高亮接口（IHighlightInterface）
    virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;
    virtual void SetMoveToLocation_Implementation(FVector& OutDestination) override;
    //** 高亮接口结束（IHighlightInterface）

	//** 战斗接口（ICombatInterface）
	virtual int32 GetPlayerLevel_Implementation() const override;
	virtual void Die(const FVector& DeathImpulse) override;
	//** 战斗接口结束（ICombatInterface）
	
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHitReacting = false;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Combat")
	float LifeSpan = 5.f;

	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TObjectPtr<AActor> CombatTarget;

    void SetLevel(int32 InLevel) { Level = InLevel; }
protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;
    virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;
	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Character Class Defaults");
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY()
	TObjectPtr<ADuraAIController> DuraAIController;

    UFUNCTION(BlueprintImplementableEvent)
    void SpawnLoot();
};
