// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "DuraAbilitySystemComponent.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*资源标签*/);
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAbilityStatusChanged, const FGameplayTag& /*能力标签*/, const FGameplayTag& /*状态标签*/, int32 /*能力等级*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FAbilityEquipped, const FGameplayTag& /*能力标签*/, const FGameplayTag& /*状态标签*/, const FGameplayTag& /*槽位*/, const FGameplayTag& /*先前槽位*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDeactivatePassiveAbility, const FGameplayTag& /*能力标签*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FActivatePassiveEffect, const FGameplayTag& /*能力标签*/, bool /*是否激活*/);

class ULoadScreenSaveGame;
/**
 * 
 */
UCLASS()
class DURA_API UDuraAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:

    FAbilityStatusChanged AbilityStatusChanged;
    FEffectAssetTags EffectAssetTags;
    FAbilitiesGiven AbilitiesGivenDelegate;
    FAbilityEquipped AbilityEquipped;
    FDeactivatePassiveAbility DeactivatePassiveAbility;
    FActivatePassiveEffect ActivatePassiveEffect;

    void AddCharacterAbilitiesFromSaveData(ULoadScreenSaveGame* SaveData);

	void AbilityActorInfoSet();
	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities);
	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassiveAbilities);

	
    bool bStartupAbilitiesGiven = false;

    void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagHeld(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
    void ForEachAbility(const FForEachAbility& Delegate);

    static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
    static FGameplayTag GetSlotFromSpec(const FGameplayAbilitySpec& AbilitySpec);
    static FGameplayTag GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec);
    FGameplayTag GetStatusFromAbilityTag(const FGameplayTag& AbilityTag);
    FGameplayTag GetSlotFromAbilityTag(const FGameplayTag& AbilityTag);
    bool SlotIsEmpty(const FGameplayTag& Slot);
    static bool AbilityHasSlot(const FGameplayAbilitySpec& Spec, const FGameplayTag& Slot);
    static bool AbilityHasAnySlot(const FGameplayAbilitySpec& Spec);
    FGameplayAbilitySpec* GetSpecWithSlot(const FGameplayTag& Slot);
    bool IsPassiveAbility(const FGameplayAbilitySpec& Spec) const;
    static void AssignSlotToAbility(FGameplayAbilitySpec& Spec, const FGameplayTag& Slot);

    UFUNCTION(NetMulticast, Unreliable)
    void MultiCastActivatePassiveEffect(const FGameplayTag& AbilityTag, bool bActivate);

    FGameplayAbilitySpec* GetSpecFromAbilityTag(const FGameplayTag& AbilityTag);

    void UpgradeAttribute(const FGameplayTag& AttributeTag);

    UFUNCTION(Server, Reliable)
    void ServerUpgradeAttribute(const FGameplayTag& AttributeTag);

    void UpdateAbilityStatuses(int32 Level);

    UFUNCTION(Server, Reliable)
    void ServerSpendSpellPoint(const FGameplayTag& AbilityTag);

    UFUNCTION(Server, Reliable)
    void ServerEquipAbility(const FGameplayTag& AbilityTag, const FGameplayTag& Slot);

    UFUNCTION(Client, Reliable)
    void ClientEquipAbility(const FGameplayTag& AbilityTag, const FGameplayTag& Status, const FGameplayTag& Slot, const FGameplayTag& PreviousSlot);

    bool GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, FString& OutDescription, FString& OutNextLevelDescription);

    static void ClearSlot(FGameplayAbilitySpec* Spec);
    void ClearAbilitiesOfSlot(const FGameplayTag& Slot);
    bool AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot);
protected:

	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, 
        const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);

	virtual void OnRep_ActivateAbilities() override;

    UFUNCTION(Client, Reliable)
    void ClientUpdateAbilityStatus(const FGameplayTag& AbilityTag, const FGameplayTag& StatusTag, int32 AbilityLevel);
};
