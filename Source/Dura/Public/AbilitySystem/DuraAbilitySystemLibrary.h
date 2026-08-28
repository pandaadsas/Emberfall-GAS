// Copyright by person HDD  

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/CharacterClassInfo.h"
#include "DuraAbilitiesTypes.h"
#include "DuraAbilitySystemLibrary.generated.h"


class UDuraOverlayWidgetController;
class UAttributeMenuWidgetController;
class USpellMenuWidgetController;
class UAbilityInfo;
class ULoadScreenSaveGame;
class ULootTiers;

/**
 * 
 */
UCLASS()
class DURA_API UDuraAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    
    /* Widget 控制器 */

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static bool MakeWidgetControllerParams(const UObject* WorldContextObject, 
        FWidgetControllerParams& OutWCParams, ADuraHUD*& OutDuraHUD);
    

	UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UDuraOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static USpellMenuWidgetController* GetSpellMenuWidgetController(const UObject* WorldContextObject);


    /* 能力系统初始化 */

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeDefaultAttributes(const UObject* WorldContextObject, 
		ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeDefaultAttributesFromSaveData(const UObject* WorldContextObject, 
		UAbilitySystemComponent* ASC, ULoadScreenSaveGame* SaveGame);

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults")
	static void GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass);

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults")
	static UCharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults")
	static UAbilityInfo* GetAbilityInfo(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|CharacterClassDefaults", meta = (DefaultToSelf = "WorldContextObject"))
	static ULootTiers* GetLootTiers(const UObject* WorldContextObject);

    /* 效果上下文读取器 */

	UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static bool IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static bool IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static bool IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static FGameplayTag GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static FVector GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static FVector GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static bool IsRadialDamage(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static float GetRadialDamageInnerRadius(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static float GetRadialDamageOuterRadius(const FGameplayEffectContextHandle& EffectContextHandle);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static FVector GetRadialDamageOrigin(const FGameplayEffectContextHandle& EffectContextHandle);

    /* 效果上下文设置器 */

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetIsBlockedHit(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit);

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetIsCriticalHit(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInIsCriticalHit);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetIsSuccessfulDebuff(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInIsSuccessfulDebuff);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDamage(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float bInDebuffDamage);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDuration(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float bInDebuffDuration);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffFrequency(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float bInDebuffFrequency);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetDamageType(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, FGameplayTag InDamageType);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static void SetDeathImpulse(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, FVector InDeathImpulse);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static void SetKnockbackForce(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, FVector InKnockbackForce);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetIsRadialDamage(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInsRadialDamage);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetRadialDamageInnerRadius(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float bInRadialDamageInnerRadius);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
	static void SetRadialDamageOuterRadius(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float bInRadialDamageOuterRadius);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayEffects")
    static void SetRadialDamageOrigin(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, FVector InRadialDamageOrigin);

    /* 游戏机制 */

	UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayMechanics")
	static void GetLivePlayersWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, 
		const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereLocation);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|GameplayMechanics")
    static void GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, const FVector& Origin, TArray<AActor*>& OutClosestTargets);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayMechanics")
    static bool IsNotFriend(AActor* FirstActor, AActor* SecondActor);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|DamageEffect")
    static FGameplayEffectContextHandle ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayMechanics")
    static TArray<FRotator> EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators);

    UFUNCTION(BlueprintPure, Category = "DuraAbilitySystemLibrary|GameplayMechanics")
    static TArray<FVector> EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors);
    
    static int32 GetXPRewardForClassAndLevel(const UObject* WorldContextObject,const ECharacterClass CharacterClass,const int32 CharacterLevel);

    /* 伤害效果参数 */
    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|DamageEffect")
    static void SetIsRadialDamageEffectParam(UPARAM(ref) FDamageEffectParams& DamageEffectParams, bool bIsRadial, float InnerRadius, float OuterRadius, FVector Origin);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|DamageEffect")
    static void SetKnockbackDirection(UPARAM(ref) FDamageEffectParams& DamageEffectParams, FVector KnockbackDirection, float Magnitude = 0.f);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|DamageEffect")
    static void SetDeathImpulseDirection(UPARAM(ref) FDamageEffectParams& DamageEffectParams, FVector DeathImpulseDirection, float Magnitude = 0.f);

    UFUNCTION(BlueprintCallable, Category = "DuraAbilitySystemLibrary|DamageEffect")
    static void SetEffectParamTargetASC(UPARAM(ref) FDamageEffectParams& DamageEffectParams, UAbilitySystemComponent* ASC);
};
