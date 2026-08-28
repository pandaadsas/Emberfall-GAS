// Copyright by person HDD  


#include "AbilitySystem/DuraAbilitySystemComponent.h"
#include "DuraGameplayTags.h"
#include "AbilitySystem/Abilities/DuraGameplayAbility.h"
#include "Dura/DuraLogChannels.h"
#include "Interaction/PlayerInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/DuraAbilitySystemLibrary.h"
#include "Game/LoadScreenSaveGame.h"


namespace
{
	// 获取能力 Spec 的激活预测键。实例化能力必须从实例的 CurrentActivationInfo 读取；
	// 仅当能力未实例化（已弃用的旧策略）时才回退到 Spec 上的 ActivationInfo。
	FPredictionKey GetSpecActivationPredictionKey(const FGameplayAbilitySpec& AbilitySpec)
	{
		if (const UGameplayAbility* Instance = AbilitySpec.GetPrimaryInstance())
		{
			return Instance->GetCurrentActivationInfo().GetActivationPredictionKey();
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return AbilitySpec.ActivationInfo.GetActivationPredictionKey();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}



void UDuraAbilitySystemComponent::AddCharacterAbilitiesFromSaveData(ULoadScreenSaveGame* SaveData)
{
    for (const FSavedAbility& Data : SaveData->SavedAbilities)
    {
        const TSubclassOf<UGameplayAbility> LoadedAbilityClass = Data.GameplayAbilityClass;

        FGameplayAbilitySpec LoadedAbilitySpec = FGameplayAbilitySpec(LoadedAbilityClass, Data.AbilityLevel);
        LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilitySlot);
        LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilityStatus);

        if(Data.AbilityType == FDuraGameplayTags::Get().Abilities_Type_Offensive)
        {
            GiveAbility(LoadedAbilitySpec);
        }
        else if(Data.AbilityType == FDuraGameplayTags::Get().Abilities_Type_Passive)
        {
            if(Data.AbilityStatus.MatchesTagExact(FDuraGameplayTags::Get().Abilities_Status_Equipped))
            {
                GiveAbilityAndActivateOnce(LoadedAbilitySpec);
            }
            else
            {
                GiveAbility(LoadedAbilitySpec);
            }
        }
    }

    bStartupAbilitiesGiven = true;
    AbilitiesGivenDelegate.Broadcast();
}

void UDuraAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDuraAbilitySystemComponent::ClientEffectApplied);
}

void UDuraAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : Abilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UDuraGameplayAbility* DuraAbility = Cast<UDuraGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(DuraAbility->StartupInputTag);
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(FDuraGameplayTags::Get().Abilities_Status_Equipped);
			GiveAbility(AbilitySpec);
		}
	}

    bStartupAbilitiesGiven = true;
    AbilitiesGivenDelegate.Broadcast();
}

void UDuraAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassiveAbilities)
{
    for (const TSubclassOf<UGameplayAbility> AbilityClass : PassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
        AbilitySpec.GetDynamicSpecSourceTags().AddTag(FDuraGameplayTags::Get().Abilities_Status_Equipped);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UDuraAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
    if (!InputTag.IsValid()) return;

    FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
			    InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
                AbilitySpec.Handle, GetSpecActivationPredictionKey(AbilitySpec));
			}
		}
	}
}

void UDuraAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

    FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UDuraAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

    FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);

            InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
            AbilitySpec.Handle, GetSpecActivationPredictionKey(AbilitySpec));
		}
	}
}

void UDuraAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
    FScopedAbilityListLock ActiveScopeLock(*this);
    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if(!Delegate.ExecuteIfBound(AbilitySpec))
        {
            UE_LOG(LogDura, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
        }
    }
}

FGameplayTag UDuraAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
    if(AbilitySpec.Ability)
    {
        for (FGameplayTag Tag : AbilitySpec.Ability->GetAssetTags())
        {
            if(Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
            {
                return Tag;
            }
        }
    }
    return FGameplayTag();
}

FGameplayTag UDuraAbilitySystemComponent::GetSlotFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
    for (FGameplayTag Tag : AbilitySpec.GetDynamicSpecSourceTags())
    {
        if(Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("InputTag"))))
        {
            return Tag;
        }
    }
    return FGameplayTag();
}

FGameplayTag UDuraAbilitySystemComponent::GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
    for (FGameplayTag Tag : AbilitySpec.GetDynamicSpecSourceTags())
    {
        if(Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities.Status"))))
        {
            return Tag;
        }
    }
    return FGameplayTag();
}

FGameplayTag UDuraAbilitySystemComponent::GetStatusFromAbilityTag(const FGameplayTag& AbilityTag)
{
    if(const FGameplayAbilitySpec* Spec = GetSpecFromAbilityTag(AbilityTag))
    {
        return GetStatusFromSpec(*Spec);
    }
    return FGameplayTag();
}

FGameplayTag UDuraAbilitySystemComponent::GetSlotFromAbilityTag(const FGameplayTag& AbilityTag)
{
    if(const FGameplayAbilitySpec* Spec = GetSpecFromAbilityTag(AbilityTag))
    {
        return GetSlotFromSpec(*Spec);
    }
    return FGameplayTag();
}

bool UDuraAbilitySystemComponent::SlotIsEmpty(const FGameplayTag& Slot)
{
    FScopedAbilityListLock ActiveScopeLock(*this);
    for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if(AbilityHasSlot(AbilitySpec, Slot))
        {
            return false;
        }
    }
    return true;
}

FGameplayAbilitySpec* UDuraAbilitySystemComponent::GetSpecFromAbilityTag(const FGameplayTag& AbilityTag)
{
    FScopedAbilityListLock ActiveScopeLock(*this);
    for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        for (FGameplayTag Tag : AbilitySpec.Ability->GetAssetTags())
        {
            if(AbilityTag.MatchesTagExact(Tag))
            {
                return &AbilitySpec;
            }
        }
    }

    return nullptr;
}

void UDuraAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
    AActor* Avatar = GetAvatarActor();
    if(Avatar && Avatar->Implements<UPlayerInterface>())
    {
        if(IPlayerInterface::Execute_GetAttributePoints(Avatar) > 0)
        {
            ServerUpgradeAttribute(AttributeTag);
        }
    }
}

void UDuraAbilitySystemComponent::ServerUpgradeAttribute_Implementation(const FGameplayTag& AttributeTag)
{
    AActor* Avatar = GetAvatarActor();
    if(!Avatar) return;

    FGameplayEventData Payload;
    Payload.EventTag = AttributeTag;
    Payload.EventMagnitude = 1.f;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, AttributeTag, Payload);

    if(Avatar->Implements<UPlayerInterface>())
    {
        IPlayerInterface::Execute_AddToAttributePoints(Avatar, -1);
    }
}
void UDuraAbilitySystemComponent::UpdateAbilityStatuses(int32 Level)
{
    UAbilityInfo* AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
    if(!AbilityInfo) return;

    for (const FDuraAbilityInfo& Info : AbilityInfo->AbilityInformation)
    {
        if(!Info.AbilityTag.IsValid()) continue;
        if(Level < Info.LevelRequirement) continue;

        if(GetSpecFromAbilityTag(Info.AbilityTag) == nullptr)
        {
            FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Info.Ability, 1);
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(FDuraGameplayTags::Get().Abilities_Status_Eligible);
            GiveAbility(AbilitySpec);
            MarkAbilitySpecDirty(AbilitySpec);
            ClientUpdateAbilityStatus(Info.AbilityTag, FDuraGameplayTags::Get().Abilities_Status_Eligible, 1);
        }       
    }

    
}

void UDuraAbilitySystemComponent::ServerSpendSpellPoint_Implementation(const FGameplayTag& AbilityTag)
{
    if(FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        AActor* Avatar = GetAvatarActor();
        if(Avatar && Avatar->Implements<UPlayerInterface>())
        {
            IPlayerInterface::Execute_AddToSpellPoints(Avatar, -1);
        }

        const FDuraGameplayTags& GameplayTags = FDuraGameplayTags::Get();

        FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);
        if(Status.MatchesTagExact(GameplayTags.Abilities_Status_Eligible))
        {
            AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GameplayTags.Abilities_Status_Eligible);
            AbilitySpec->GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_UnLocked);
            Status = GameplayTags.Abilities_Status_UnLocked;
        }
        else if(Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped) || Status.MatchesTagExact(GameplayTags.Abilities_Status_UnLocked))
        {
            AbilitySpec->Level++;    
        }
        ClientUpdateAbilityStatus(AbilityTag, Status, AbilitySpec->Level);
        MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UDuraAbilitySystemComponent::ServerEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Slot)
{
    if(FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        const FDuraGameplayTags& GameplayTags = FDuraGameplayTags::Get();
        const FGameplayTag& PrevSlot = GetSlotFromSpec(*AbilitySpec);
        const FGameplayTag& Status = GetStatusFromSpec(*AbilitySpec);

        const bool bStatusValid = (Status == GameplayTags.Abilities_Status_Equipped || Status == GameplayTags.Abilities_Status_UnLocked);
        if(bStatusValid)
        {
            //Handle activation / deactivation for passive abilities
            
            if(!SlotIsEmpty(Slot)) //There is an ability in this slot already. Deactivate and clear its slot
            {
                if(FGameplayAbilitySpec* SpecWithSlot = GetSpecWithSlot(Slot))
                {
                    // is that ability the same as this ability? If so , we can return early.
                    if(AbilityTag.MatchesTagExact(GetAbilityTagFromSpec(*SpecWithSlot)))
                    {
                        ClientEquipAbility(AbilityTag, GameplayTags.Abilities_Status_Equipped, Slot, PrevSlot);
                        return;
                    }

                    if(IsPassiveAbility(*SpecWithSlot))
                    {
                        MultiCastActivatePassiveEffect(GetAbilityTagFromSpec(*SpecWithSlot), false);
                        DeactivatePassiveAbility.Broadcast(GetAbilityTagFromSpec(*SpecWithSlot));
                    }

                    ClearSlot(SpecWithSlot);

                }
            }

            if(!AbilityHasAnySlot(*AbilitySpec)) // Ability doesn't yet have a slot(it's not avtive)
            {
                if(IsPassiveAbility(*AbilitySpec))
                {
                    TryActivateAbility(AbilitySpec->Handle);
                    MultiCastActivatePassiveEffect(AbilityTag, true);
                }

                AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GetStatusFromSpec(*AbilitySpec));
                AbilitySpec->GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_Equipped);
            }
            
            AssignSlotToAbility(*AbilitySpec, Slot);
            MarkAbilitySpecDirty(*AbilitySpec);
        }

        ClientEquipAbility(AbilityTag, GameplayTags.Abilities_Status_Equipped, Slot, PrevSlot);
    }
}

void UDuraAbilitySystemComponent::ClientEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Status, const FGameplayTag& Slot, const FGameplayTag& PreviousSlot)
{
    AbilityEquipped.Broadcast(AbilityTag, Status, Slot, PreviousSlot);
}

bool UDuraAbilitySystemComponent::GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, 
    FString& OutDescription, FString& OutNextLevelDescription)
{
    if(const FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        if(UDuraGameplayAbility* DuraAbility = Cast<UDuraGameplayAbility>(AbilitySpec->Ability))
        {
            OutDescription = DuraAbility->GetDescription(AbilitySpec->Level);                
            OutNextLevelDescription = DuraAbility->GetNextLevelDescription(AbilitySpec->Level + 1);     
            return true;
        }
    }
    const UAbilityInfo* AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
    if(!AbilityTag.IsValid() || AbilityTag.MatchesTagExact(FDuraGameplayTags::Get().Abilities_None))
    {
        OutDescription = FString();
    }
    else
    {
        OutDescription = UDuraGameplayAbility::GetLockedDescription(AbilityInfo->FindAbilityInfoForTag(AbilityTag).LevelRequirement);
    }
    OutNextLevelDescription = FString();
    return false;
}

void UDuraAbilitySystemComponent::ClearSlot(FGameplayAbilitySpec* Spec)
{
    const FGameplayTag Slot = GetSlotFromSpec(*Spec);
    Spec->GetDynamicSpecSourceTags().RemoveTag(Slot);
}

void UDuraAbilitySystemComponent::ClearAbilitiesOfSlot(const FGameplayTag& Slot)
{
    FScopedAbilityListLock ActiveListLock(*this);
    for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
    {
        if(AbilityHasSlot(&Spec, Slot))
        {
            ClearSlot(&Spec);
        }
    }
}

bool UDuraAbilitySystemComponent::AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot)
{
    for (FGameplayTag Tag : Spec->GetDynamicSpecSourceTags())
    {
        if(Tag.MatchesTagExact(Slot))
        {
            return true;
        }
    }
    return false;
}

bool UDuraAbilitySystemComponent::AbilityHasSlot(const FGameplayAbilitySpec& Spec, const FGameplayTag& Slot)
{
    return Spec.GetDynamicSpecSourceTags().HasTagExact(Slot);
}

bool UDuraAbilitySystemComponent::AbilityHasAnySlot(const FGameplayAbilitySpec& Spec)
{
    return Spec.GetDynamicSpecSourceTags().HasTag(FGameplayTag::RequestGameplayTag(FName("InputTag")));
}

FGameplayAbilitySpec* UDuraAbilitySystemComponent::GetSpecWithSlot(const FGameplayTag& Slot)
{
    FScopedAbilityListLock ActiveListLock(*this);
    for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
    {
        if(Spec.GetDynamicSpecSourceTags().HasTagExact(Slot))
        {
            return &Spec;
        }
    }
    return nullptr;
}

bool UDuraAbilitySystemComponent::IsPassiveAbility(const FGameplayAbilitySpec& Spec) const
{
    UAbilityInfo* AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
    const FGameplayTag AbilityTag = GetAbilityTagFromSpec(Spec);
    FDuraAbilityInfo FindAbilityInfo = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
    return FindAbilityInfo.AbilityType.MatchesTagExact(FDuraGameplayTags::Get().Abilities_Type_Passive);
}

void UDuraAbilitySystemComponent::AssignSlotToAbility(FGameplayAbilitySpec& Spec, const FGameplayTag& Slot)
{
    ClearSlot(&Spec);
    Spec.GetDynamicSpecSourceTags().AddTag(Slot);
}

void UDuraAbilitySystemComponent::MultiCastActivatePassiveEffect_Implementation(const FGameplayTag& AbilityTag, bool bActivate)
{
    ActivatePassiveEffect.Broadcast(AbilityTag, bActivate);
}

void UDuraAbilitySystemComponent::OnRep_ActivateAbilities()
{
    Super::OnRep_ActivateAbilities();

    if(!bStartupAbilitiesGiven)
    {
        bStartupAbilitiesGiven = true;
        AbilitiesGivenDelegate.Broadcast();        
    }    
}

void UDuraAbilitySystemComponent::ClientUpdateAbilityStatus_Implementation(const FGameplayTag& AbilityTag, 
    const FGameplayTag& StatusTag, int32 AbilityLevel)
{
    AbilityStatusChanged.Broadcast(AbilityTag, StatusTag, AbilityLevel);
}



void UDuraAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);
	EffectAssetTags.Broadcast(TagContainer);
}
