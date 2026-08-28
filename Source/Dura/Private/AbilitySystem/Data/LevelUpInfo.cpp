// Copyright by person HDD  


#include "AbilitySystem/Data/LevelUpInfo.h"

int32 ULevelUpInfo::FindLevelForXP(int32 XP) const
{
    int32 Level = 1;
    bool bSearching = true;
    while(bSearching)
    {
        // LevelUpInformation[1] = 等级 1 的信息
        // LevelUpInformation[2] = 等级 2 的信息
        if(LevelUpInformation.Num() - 1 <= Level) return Level;

        if(XP >= LevelUpInformation[Level].LevelUpRequirement)
        {
            ++Level;
        }
        else
        {
            bSearching = false;
        }
    }
    return Level;
}
