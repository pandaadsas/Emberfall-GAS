// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/AsyncTasks/WaitCooldownChange.h"
#include "AbilitySystemComponent.h"

UWaitCooldownChange* UWaitCooldownChange::WaitForCooldownChange(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTag& InCooldownTag)
{
    UWaitCooldownChange* WaitCooldownChange = NewObject<UWaitCooldownChange>();
    WaitCooldownChange->ASC = AbilitySystemComponent;
    WaitCooldownChange->CooldownTag = InCooldownTag;

    if(!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
    {
        WaitCooldownChange->EndTask();
        return nullptr;
    }

    // 监听冷却效果结束的时机
    AbilitySystemComponent->RegisterGameplayTagEvent(InCooldownTag, EGameplayTagEventType::NewOrRemoved)
    .AddUObject(WaitCooldownChange, &UWaitCooldownChange::CooldownTagChanged);

    // 监听冷却效果被应用的时机
    AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf
    .AddUObject(WaitCooldownChange, &UWaitCooldownChange::OnActiveEffectAdded);

    return WaitCooldownChange;
}

void UWaitCooldownChange::EndTask()
{
    if(IsValid(ASC))
    {
        ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
        ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
    }

    SetReadyToDestroy();
    MarkAsGarbage();
}

void UWaitCooldownChange::CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount)
{
    if(NewCount == 0)
    {
        CooldownEnd.Broadcast(0.f);
    }
}

void UWaitCooldownChange::OnActiveEffectAdded(UAbilitySystemComponent* InASC, const FGameplayEffectSpec& GameplayEffectSpec, 
    FActiveGameplayEffectHandle ActiveEffectHandle)
{
    FGameplayTagContainer AssetTags;
    GameplayEffectSpec.GetAllAssetTags(AssetTags);

    FGameplayTagContainer GrantTags;
    GameplayEffectSpec.GetAllGrantedTags(GrantTags);

    if(AssetTags.HasTagExact(CooldownTag) || GrantTags.HasTagExact(CooldownTag))
    {
        FGameplayEffectQuery GameplayEffectQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTag.GetSingleTagContainer());
        TArray<float> TimeRemaining = InASC->GetActiveEffectsTimeRemaining(GameplayEffectQuery);
        if(TimeRemaining.Num() > 0)
        {
            float MaxTimeRemaining = TimeRemaining[0];
            for (int32 i = 1; i < TimeRemaining.Num(); i++)
            {
                if(TimeRemaining[i] > MaxTimeRemaining)
                {
                    MaxTimeRemaining = TimeRemaining[i];
                }
            }
            CooldownStart.Broadcast(MaxTimeRemaining);
        }
    }
}
