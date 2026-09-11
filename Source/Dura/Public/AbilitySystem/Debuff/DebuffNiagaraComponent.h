// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "GameplayTagContainer.h"
#include "DebuffNiagaraComponent.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDebuffNiagaraComponent : public UNiagaraComponent
{
	GENERATED_BODY()
public:
    UDebuffNiagaraComponent();

    UPROPERTY(EditDefaultsOnly, Category = "Debuff")
    FGameplayTag DebuffTag;

protected:
    virtual void BeginPlay() override;

    void DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    UFUNCTION()
    void OnOwnerDeath(AActor* DeadActor);
};
