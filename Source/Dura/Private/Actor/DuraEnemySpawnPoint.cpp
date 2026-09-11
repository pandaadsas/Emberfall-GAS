// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "Actor/DuraEnemySpawnPoint.h"
#include "Character/DuraEnemy.h"

void ADuraEnemySpawnPoint::SpawnEnemy()
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    if(!IsValid(EnemyClass))return;

    if(ADuraEnemy* Enemy = GetWorld()->SpawnActorDeferred<ADuraEnemy>(EnemyClass, GetActorTransform()))
    {
        Enemy->SetLevel(EnemyLevel);
        Enemy->SetCharacterClass(CharacterClass);
        Enemy->FinishSpawning(GetActorTransform());
        Enemy->SpawnDefaultController();
    }

}
