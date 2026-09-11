// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DuraAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDuraAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:
	static UDuraAssetManager& Get();

protected:

	virtual void StartInitialLoading() override;
};
