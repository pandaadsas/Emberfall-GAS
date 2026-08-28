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
            //Title
            "<Title>FIRE BLAST</>\n\n"

            // Level
            "<Small>Level: </><Level>%d</>\n"

            //ManaCost
            "<Small>ManaCost: </><ManaCost>%.1f</>\n"

             //Cooldown
            "<Small>Cooldown: </><Cooldown>%.1f</>\n"

            //Number of Fire Balls
            "\n<Default>Launched %d </>"
            "<Default>fire balls in all directions, each coming back and exploding upon return, causing </>"

            //Damage
            "<Damage>%d</>"
            "<Default> radial fire damage with</>"), 

            //Values
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
            //Title
            "<Title>NEXT LEVEL: </>\n\n"

            // Level
            "<Small>Level: </><Level>%d</>\n"

            //ManaCost
            "<Small>ManaCost: </><ManaCost>%.1f</>\n"

             //Cooldown
            "<Small>Cooldown: </><Cooldown>%.1f</>\n"

            //Number of Fire Balls
            "\n<Default>Launched %d </>"
            "<Default>fire balls in all directions, each coming back and exploding upon return, causing </>"

            //Damage
            "<Damage>%d</>"
            "<Default> radial fire damage with</>"), 

            //Values
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
