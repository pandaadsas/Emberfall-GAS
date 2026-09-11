// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DuraDamageGameplayAbility.h"
#include "ArcaneShards.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UArcaneShards : public UDuraDamageGameplayAbility
{
	GENERATED_BODY()
public:
      
    virtual FString GetDescription(int32 Level) override;
    virtual FString GetNextLevelDescription(int32 Level) override;
	
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxNumShards = 10;
};
