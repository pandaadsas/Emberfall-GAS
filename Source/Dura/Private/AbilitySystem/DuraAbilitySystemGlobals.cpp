// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/DuraAbilitySystemGlobals.h"
#include "DuraAbilitiesTypes.h"

FGameplayEffectContext* UDuraAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FDuraGameplayEffectContext();
}
