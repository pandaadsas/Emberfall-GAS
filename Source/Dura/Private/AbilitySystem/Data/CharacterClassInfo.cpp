// Copyright by person HDD


#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Dura/DuraLogChannels.h"

FCharacterClassDefaultInfo UCharacterClassInfo::GetClassDefaultInfo(ECharacterClass CharacterClass) const
{
	// 数据资产漏配该职业时回退到 Warrior 并告警，避免 FindChecked 断言崩溃
	if(const FCharacterClassDefaultInfo* Info = CharacterClassInformation.Find(CharacterClass))
	{
		return *Info;
	}

	UE_LOG(LogDura, Warning, TEXT("UCharacterClassInfo [%s] 缺少职业 [%d] 的配置，已回退到 Warrior"),
		*GetName(), static_cast<int32>(CharacterClass));
	return CharacterClassInformation.FindRef(ECharacterClass::Warrior);
}
