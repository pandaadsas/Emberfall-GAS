// Copyright by person HDD  


#include "AbilitySystem/Abilities/DuraFireBolt.h"
#include "Actor/DuraProjectile.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystemComponent.h"
#include "DuraGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystem/DuraAbilitySystemLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"

FString UDuraFireBolt::GetDescription(int32 Level)
{   
    const int32 DamageValue = Damage.GetValueAtLevel(Level);
    const float ManaCost = FMath::Abs(GetManaCost(Level));
    const float Cooldown = GetCooldown(Level);

    if(Level == 1)
    {
        return FString::Printf(TEXT(
            //标题
            "<Title>火弹术</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

             //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            "\n<Default>发射一枚火弹，命中时爆炸并造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点火焰伤害，并有概率点燃</>"),

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
            "<Title>火弹术</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

            //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            //火弹数量
            "\n<Default>发射 %d 枚火弹，命中时爆炸并造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点火焰伤害，并有概率点燃</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            FMath::Min(Level, NumProjectiles),
            DamageValue);
    }
}

FString UDuraFireBolt::GetNextLevelDescription(int32 Level)
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

            //火弹数量
            "\n<Default>发射 %d 枚火弹，命中时爆炸并造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点火焰伤害，并有概率点燃</>"), 

            //数值
            Level, 
            ManaCost, 
            Cooldown,
            FMath::Min(Level, NumProjectiles), 
            DamageValue);
}

void UDuraFireBolt::SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag,
    bool bOverridePitch, float PitchOverride, AActor* HomingTarget)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    const bool bIsServer = Avatar && Avatar->HasAuthority();
	if (!bIsServer) return;

	if (!Avatar->Implements<UCombatInterface>()) return;

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
            Avatar, SocketTag);
	FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();
	if(bOverridePitch) Rotation.Pitch = PitchOverride;

    const FVector Forward = Rotation.Vector();
    int32 EffectiveNumProjectiles = FMath::Min(NumProjectiles, GetAbilityLevel());

    TArray<FRotator> Rotators = UDuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 
        ProjectileSpread, EffectiveNumProjectiles);

    for (const FRotator& Rot : Rotators)
    {
        FTransform SpawnTransform;
		SpawnTransform.SetLocation(SocketLocation);
		SpawnTransform.SetRotation(Rot.Quaternion());

		AActor* Owner = GetOwningActorFromActorInfo();
		ADuraProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADuraProjectile>(
			ProjectileClass,
			SpawnTransform,
			Owner,
			Cast<APawn>(Owner),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);
		if(!Projectile) continue;

        Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

        if(IsValid(HomingTarget) && HomingTarget->Implements<UCombatInterface>())
        {
            Projectile->ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
        }
        else
        {
            Projectile->HomingTargetSceneComponent = NewObject<USceneComponent>(USceneComponent::StaticClass());
            Projectile->HomingTargetSceneComponent->SetWorldLocation(ProjectileTargetLocation);

            Projectile->ProjectileMovement->HomingTargetComponent = Projectile->HomingTargetSceneComponent;
        }
        Projectile->ProjectileMovement->HomingAccelerationMagnitude = FMath::FRandRange(HomingAccelerationMin, HomingAccelerationMax);
        Projectile->ProjectileMovement->bIsHomingProjectile = bLaunchHomingProjectiles;

		Projectile->FinishSpawning(SpawnTransform);
    }
    
    
}
