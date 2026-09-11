// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "DuraAssetManager.h"
#include "DuraGameplayTags.h"


UDuraAssetManager& UDuraAssetManager::Get()
{
	check(GEngine);

	UDuraAssetManager* DuraAssetManager = Cast<UDuraAssetManager>(GEngine->AssetManager);
	return *DuraAssetManager;
}

void UDuraAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	FDuraGameplayTags::InitializeNativeGameplayTags();
}
