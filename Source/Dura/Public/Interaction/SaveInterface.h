// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveInterface.generated.h"

// 无需修改此类。
UINTERFACE(MinimalAPI)
class USaveInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DURA_API ISaveInterface
{
	GENERATED_BODY()

	// 在此类中添加接口函数；实现该接口的类将继承此类。
public:

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    bool ShouldLoadTransform();

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    void LoadActor();
};
