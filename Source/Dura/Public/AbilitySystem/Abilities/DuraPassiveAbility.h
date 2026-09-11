// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DuraGameplayAbility.h"
#include "DuraPassiveAbility.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDuraPassiveAbility : public UDuraGameplayAbility
{
	GENERATED_BODY()
public:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    void ReceiveDeactivate(const FGameplayTag& AbilityTag);
};
