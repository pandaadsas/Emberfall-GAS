// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "GameplayTagContainer.h"
#include "PassiveNiagaraComponent.generated.h"

class UDuraAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class DURA_API UPassiveNiagaraComponent : public UNiagaraComponent
{
	GENERATED_BODY()
public:
    UPassiveNiagaraComponent();

    UPROPERTY(EditDefaultsOnly)
    FGameplayTag PassiveSpellTag;

protected:
    virtual void BeginPlay() override;
    void OnPassiveActivate(const FGameplayTag& AbilityTag, bool bActivate);

    void ActivateIfEquipped(UDuraAbilitySystemComponent* ASC);
};
