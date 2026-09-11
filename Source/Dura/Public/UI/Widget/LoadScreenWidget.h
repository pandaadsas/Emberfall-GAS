// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadScreenWidget.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API ULoadScreenWidget : public UUserWidget
{
	GENERATED_BODY()
public:

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
    void BlueprintInitializeWidget();
protected:
    
};
