// Copyright by person HDD  


#include "AbilitySystem/Abilities/Electrocute.h"

FString UElectrocute::GetDescription(int32 Level)
{
    const int32 DamageValue = Damage.GetValueAtLevel(Level);
    const float ManaCost = FMath::Abs(GetManaCost(Level));
    const float Cooldown = GetCooldown(Level);

    if(Level == 1)
    {
        return FString::Printf(TEXT(
            //标题
            "<Title>电击</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

             //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            "\n<Default>射出一道闪电光束连接目标，反复造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点闪电伤害，并有概率眩晕</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            DamageValue);
    }
    else
    {
        return FString::Printf(TEXT(
            //标题
            "<Title>电击</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

            //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            //额外电击目标数量
            "\n<Default>射出一道闪电光束，传导至附近 %d 个额外目标，造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点闪电伤害，并有概率眩晕</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            FMath::Min(Level, MaxNumShockTargets),
            DamageValue);
    }
}

FString UElectrocute::GetNextLevelDescription(int32 Level)
{
    const int32 DamageValue = Damage.GetValueAtLevel(Level);
    const float ManaCost = FMath::Abs(GetManaCost(Level));
    const float Cooldown = GetCooldown(Level);
    
    return FString::Printf(TEXT(
        //标题
        "<Title>下一级：</>\n\n"

        //等级
        "<Small>等级：</><Level>%d</>\n"

        //法力消耗
        "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

        //冷却时间
        "<Small>冷却时间：</><Cooldown>%.1f</>\n"

        //额外电击目标数量
        "\n<Default>射出一道闪电光束，传导至附近 %d 个额外目标，造成</>"

        //伤害
        "<Damage>%d</>"
        "<Default> 点闪电伤害，并有概率眩晕</>"),

        //Values
        Level, 
        ManaCost, 
        Cooldown,
        FMath::Min(Level, MaxNumShockTargets), 
        DamageValue);
}
