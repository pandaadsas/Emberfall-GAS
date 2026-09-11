// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerInterface.generated.h"

// 无需修改此类。
UINTERFACE(MinimalAPI)
class UPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DURA_API IPlayerInterface
{
	GENERATED_BODY()

	// 在此类中添加接口函数；实现该接口的类将继承此类。
public:
    UFUNCTION(BlueprintNativeEvent)
    int32 FindLevelForXP(int32 InXP) const;

    UFUNCTION(BlueprintNativeEvent)
    int32 GetXP() const;

    UFUNCTION(BlueprintNativeEvent)
    int32 GetAttributePointsReward(int32 Level) const; 

    UFUNCTION(BlueprintNativeEvent)
    int32 GetSpellPointsReward(int32 Level) const; 

    UFUNCTION(BlueprintNativeEvent)
    void AddToXP(int32 InXP);

    UFUNCTION(BlueprintNativeEvent)
    void AddToPlayerLevel(int32 InPlayerLevel);

    UFUNCTION(BlueprintNativeEvent)
    void AddToAttributePoints(int32 InAttributePoints);

    UFUNCTION(BlueprintNativeEvent)
    void AddToSpellPoints(int32 InSpellPoints);

    UFUNCTION(BlueprintNativeEvent)
    int32 GetAttributePoints() const;

     UFUNCTION(BlueprintNativeEvent)
    int32 GetSpellPoints() const;

    UFUNCTION(BlueprintNativeEvent)
    void LevelUp();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void ShowMagicCircle(UMaterialInterface* DecalMaterial = nullptr);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void HideMagicCircle();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void SaveProgress(const FName& CheckPointTag);
};
