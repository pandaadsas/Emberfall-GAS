// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DuraGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDuraGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
    
    UPROPERTY()
    FName PlayerStartTag = FName();

    UPROPERTY()
    FString LoadSlotName = FString();

    UPROPERTY()
    int32 LoadSlotIndex = 0;
};
