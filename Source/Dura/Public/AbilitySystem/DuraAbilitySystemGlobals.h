// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "DuraAbilitySystemGlobals.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDuraAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
	
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
