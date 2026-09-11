// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/Abilities/DuraGameplayAbility.h"
#include "AbilitySystem/DuraAttributeSet.h"

FString UDuraGameplayAbility::GetDescription(int32 Level)
{
    return FString::Printf(TEXT("<Default>默认技能描述。</><Level>%d</>"), Level);
}

FString UDuraGameplayAbility::GetNextLevelDescription(int32 Level)
{
    return FString::Printf(TEXT("<Default>下一级：</><Level>%d</>\n<Default>造成更高的伤害</>"), Level);
}

FString UDuraGameplayAbility::GetLockedDescription(int32 Level)
{
    return FString::Printf(TEXT("<Default>技能尚未解锁</>\n<Default>需要等级：%d</>"), Level);
}

float UDuraGameplayAbility::GetManaCost(float InLevel /*= 1.f*/) const
{
    float ManaCost = 0.0f;
    if(const UGameplayEffect* CostEffect = CostGameplayEffectClass.GetDefaultObject())
    {
        for(FGameplayModifierInfo Mod : CostEffect->Modifiers)
        {
            if(Mod.Attribute == UDuraAttributeSet::GetManaAttribute())
            {
                Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, ManaCost);
                break;
            }
        }
    }
    return ManaCost;
}

float UDuraGameplayAbility::GetCooldown(float InLevel /*= 1.f*/) const
{
    float Cooldown = 0.0f;
    if(const UGameplayEffect* CooldownEffect = CooldownGameplayEffectClass.GetDefaultObject())
    {
        CooldownEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, Cooldown);
        
    }
    return Cooldown;
}