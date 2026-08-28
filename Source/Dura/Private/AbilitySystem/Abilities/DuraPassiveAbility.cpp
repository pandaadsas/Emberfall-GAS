// Copyright by person HDD  


#include "AbilitySystem/Abilities/DuraPassiveAbility.h"
#include "AbilitySystem/DuraAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

void UDuraPassiveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if(UDuraAbilitySystemComponent* ASC = Cast<UDuraAbilitySystemComponent>(
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo())))
    {
        // InstancedPerActor 实例可能被重复激活，先解绑避免委托堆积
        ASC->DeactivatePassiveAbility.RemoveAll(this);
        ASC->DeactivatePassiveAbility.AddUObject(this, &UDuraPassiveAbility::ReceiveDeactivate);
    }
    
}
void UDuraPassiveAbility::ReceiveDeactivate(const FGameplayTag& AbilityTag)
{
    if(GetAssetTags().HasTagExact(AbilityTag))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}
