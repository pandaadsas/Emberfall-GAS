// Copyright by person HDD  


#include "AbilitySystem/Abilities/DuraFireBlast.h"
#include "AbilitySystem\DuraAbilitySystemLibrary.h"
#include "Actor/DuraFireBall.h"


FString UDuraFireBlast::GetDescription(int32 Level)
{
    const int32 DamageValue = Damage.GetValueAtLevel(Level);
    const float ManaCost = FMath::Abs(GetManaCost(Level));
    const float Cooldown = GetCooldown(Level);

    return FString::Printf(TEXT(
            //标题
            "<Title>火焰爆发</>\n\n"

            //等级
            "<Small>等级：</><Level>%d</>\n"

            //法力消耗
            "<Small>魔法消耗：</><ManaCost>%.1f</>\n"

             //冷却时间
            "<Small>冷却时间：</><Cooldown>%.1f</>\n"

            //火球数量
            "\n<Default>向四面八方发射 %d </>"
            "<Default>枚火球，每枚都会返回并在归途中爆炸，造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点径向火焰伤害</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            NumFireBalls,
            DamageValue);
}

FString UDuraFireBlast::GetNextLevelDescription(int32 Level)
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

            //火球数量
            "\n<Default>向四面八方发射 %d </>"
            "<Default>枚火球，每枚都会返回并在归途中爆炸，造成</>"

            //伤害
            "<Damage>%d</>"
            "<Default> 点径向火焰伤害</>"),

            //数值
            Level,
            ManaCost,
            Cooldown,
            NumFireBalls,
            DamageValue);
}

TArray<ADuraFireBall*> UDuraFireBlast::SpawnFireBalls()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if(!Avatar || !GetWorld()) return TArray<ADuraFireBall*>();

    // 非玩家施放时 PlayerController 可能为空，退化为用 Avatar 自身作为 Instigator
    APawn* InstigatorPawn = (CurrentActorInfo && CurrentActorInfo->PlayerController.IsValid())
        ? CurrentActorInfo->PlayerController->GetPawn()
        : nullptr;

    const FVector Forward = Avatar->GetActorForwardVector();
    const FVector Location = Avatar->GetActorLocation();
    TArray<FRotator> Rotators = UDuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 360.f, NumFireBalls);

    TArray<ADuraFireBall*> FireBalls;
    for (const FRotator& Rotator : Rotators)
    {
        FTransform SpawnTransform;
        SpawnTransform.SetLocation(Location);
        SpawnTransform.SetRotation(Rotator.Quaternion());

        ADuraFireBall* FireBall = GetWorld()->SpawnActorDeferred<ADuraFireBall>(
            FireBallClass,
            SpawnTransform,
            Avatar,
            InstigatorPawn,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );
        if(!FireBall) continue;

        FireBall->ReturnToActor = Avatar;
        FireBall->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
        FireBall->ExplosionDamageParams = MakeDamageEffectParamsFromClassDefaults();

        FireBalls.Add(FireBall);

        FireBall->FinishSpawning(SpawnTransform);
    }

    return FireBalls;
}
