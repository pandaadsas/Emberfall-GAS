// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "Game/LoadScreenSaveGame.h"

FSaveMap ULoadScreenSaveGame::GetSavedMapWithMapName(const FString& InMapName)
{
    for (const FSaveMap& Map : SavedMaps)
    {
        if(Map.MapAssetName == InMapName)
        {
            return Map;
        }
    }

    return FSaveMap();
}

bool ULoadScreenSaveGame::HasMap(const FString& InMapName)
{
    for (const FSaveMap& Map : SavedMaps)
    {
        if(Map.MapAssetName == InMapName)
        {
            return true;
        }
    }

    return false;
}
