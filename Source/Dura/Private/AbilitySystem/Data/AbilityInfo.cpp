// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/Data/AbilityInfo.h"
#include "Dura/DuraLogChannels.h"

FDuraAbilityInfo UAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
    for (const FDuraAbilityInfo& Info : AbilityInformation)
	{
		if (Info.AbilityTag.MatchesTagExact(AbilityTag))
		{
			return Info;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogDura, Error, TEXT("Can't find Info for AbilityTag [%s] on AbilityInfo [%s]."), *AbilityTag.ToString(), *GetNameSafe(this));
	}

	return FDuraAbilityInfo();
}
