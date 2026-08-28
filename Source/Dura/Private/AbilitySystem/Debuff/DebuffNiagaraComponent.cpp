// Copyright by person HDD  


#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Interaction/CombatInterface.h"

UDebuffNiagaraComponent::UDebuffNiagaraComponent()
{
    bAutoActivate = false;
}

void UDebuffNiagaraComponent::BeginPlay()
{
    Super::BeginPlay();

    ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetOwner());
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    if(ASC)
    {
        ASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UDebuffNiagaraComponent::DebuffTagChanged);
    }
    else if(CombatInterface)
    {
        CombatInterface->GetOnASCRegisteredDelegate().AddWeakLambda(this, [this](UAbilitySystemComponent* NewASC)
        {
            NewASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UDebuffNiagaraComponent::DebuffTagChanged);
        });
    }
    
    if(CombatInterface)
    {
        CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDebuffNiagaraComponent::OnOwnerDeath);
    }
}

void UDebuffNiagaraComponent::DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    // 短路求值：Owner 无效时不能继续调用其接口
    const bool bOwnerValidAndAlive = IsValid(GetOwner()) && GetOwner()->Implements<UCombatInterface>()
        && !ICombatInterface::Execute_IsDead(GetOwner());

    if(NewCount > 0 && bOwnerValidAndAlive)
    {
        Activate();
    }
    else
    {
        Deactivate();
    }
}

void UDebuffNiagaraComponent::OnOwnerDeath(AActor* DeadActor)
{
    Deactivate();
}
