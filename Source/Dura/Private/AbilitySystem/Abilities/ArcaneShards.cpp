// Copyright by person HDD  


#include "AbilitySystem/Abilities/ArcaneShards.h"

FString UArcaneShards::GetDescription(int32 Level)
{
    const int32 DamageValue = Damage.GetValueAtLevel(Level);
    const float ManaCost = FMath::Abs(GetManaCost(Level));
    const float Cooldown = GetCooldown(Level);

    if(Level == 1)
    {
        return FString::Printf(TEXT(
            //标题
            "<Title>奥术碎片</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

             //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            "\n<Default>召唤一枚奥术能量碎片，造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点径向奥术伤害。</>"),

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
            "<Title>奥术碎片</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

            //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            //额外电击目标数量
            "\n<Default>召唤 %d 枚奥术能量碎片，造成径向奥术伤害</>"

            //伤害
            "<Damage>%d</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            FMath::Min(Level, MaxNumShards),
            DamageValue);
    }
}

FString UArcaneShards::GetNextLevelDescription(int32 Level)
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
        "\n<Default>召唤 %d 枚奥术能量碎片，造成径向奥术伤害</>"

        //伤害
        "<Damage>%d</>"),

        //数值
        Level,
        ManaCost,
        Cooldown,
        FMath::Min(Level, MaxNumShards),
        DamageValue);
}
