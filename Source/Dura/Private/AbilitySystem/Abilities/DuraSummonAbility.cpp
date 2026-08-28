// Copyright by person HDD  


#include "AbilitySystem/Abilities/DuraSummonAbility.h"
#include "Kismet/KismetSystemLibrary.h"

TArray<FVector> UDuraSummonAbility::GetSpawnLocations()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if(!Avatar || !GetWorld()) return TArray<FVector>();

    const FVector Forward = Avatar->GetActorForwardVector();
    const FVector Location = Avatar->GetActorLocation();
    const float DeltaSpread = SpawnSpread / NumMinions;
    
    const FVector LeftOfSpread = Forward.RotateAngleAxis(-SpawnSpread / 2.f, FVector::UpVector);
    TArray<FVector> SpawnLocations;
    for(int32 i = 0; i < NumMinions; i++)
    {
        const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
        FVector ChosenSpawnLocation = Location + Direction * FMath::FRandRange(MinSpawnDistnace, MaxSpawnDistnace);

        FHitResult hit;
        GetWorld()->LineTraceSingleByChannel(hit, 
            ChosenSpawnLocation + FVector(0.f, 0.f, 400.f), 
            ChosenSpawnLocation - FVector(0.f, 0.f, 400.f), ECC_Visibility);

        if(hit.bBlockingHit)
        {
            ChosenSpawnLocation = hit.ImpactPoint;
        }

        SpawnLocations.Add(ChosenSpawnLocation);
    }
  
    return SpawnLocations;
}

TSubclassOf<APawn> UDuraSummonAbility::GetRandomMinionClass()
{
    // 未配置召唤物类别时避免 RandRange(0, -1) 的未定义行为与越界访问
    if(MinionClasses.Num() == 0) return nullptr;

    const int32 Selection = FMath::RandRange(0, MinionClasses.Num() - 1);
    return MinionClasses[Selection];
}
