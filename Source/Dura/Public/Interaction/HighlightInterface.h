// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HighlightInterface.generated.h"

// 无需修改此类。
UINTERFACE(MinimalAPI, BlueprintType)
class UHighlightInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DURA_API IHighlightInterface
{
	GENERATED_BODY()

	// 在此类中添加接口函数；实现该接口的类将继承此类。
public:
    
    UFUNCTION(BlueprintNativeEvent)
    void HighlightActor();

    UFUNCTION(BlueprintNativeEvent)
	void UnHighlightActor();

    UFUNCTION(BlueprintNativeEvent)
    void SetMoveToLocation(FVector& OutDestination);
};
