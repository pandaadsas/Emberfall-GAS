// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "AbilitySystem/Data/LootTiers.h"

TArray<FLootItem> ULootTiers::GetLootItems()
{
    TArray<FLootItem> ReturnItems;

    for (FLootItem& Item : LootItems)
    {
        for (int32 i = 0; i < Item.MaxNumberToSpawn; ++i)
        {
            if(FMath::FRandRange(1.f, 100.f) < Item.ChanceToSpawn)
            {
                FLootItem NewItem;
                NewItem.LootClass = Item.LootClass;
                NewItem.bLootLevelOverride = Item.bLootLevelOverride;
                ReturnItems.Add(NewItem);
            }
        }
    }

    return ReturnItems;
}
