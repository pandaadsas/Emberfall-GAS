// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/Abilities/DuraProjectileSpell.h"
#include "Actor/DuraProjectile.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystemComponent.h"
#include "DuraGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"

void UDuraProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UDuraProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const bool bIsServer = Avatar && Avatar->HasAuthority();
	if (!bIsServer) return;

	if (Avatar->Implements<UCombatInterface>())
	{
		const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
            Avatar, SocketTag);
		FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();
		if(bOverridePitch)
        {
            Rotation.Pitch = PitchOverride;
        }


		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SocketLocation);
		SpawnTransform.SetRotation(Rotation.Quaternion());

		AActor* Owner = GetOwningActorFromActorInfo();
		ADuraProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADuraProjectile>(
			ProjectileClass,
			SpawnTransform,
			Owner,
			Cast<APawn>(Owner),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

        Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

		if(!Projectile) return;

		Projectile->FinishSpawning(SpawnTransform);
	}
}
