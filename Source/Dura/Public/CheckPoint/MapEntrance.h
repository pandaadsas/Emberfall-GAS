// Copyright by person HDD  

#pragma once

#include "CoreMinimal.h"
#include "CheckPoint/CheckPoint.h"
#include "MapEntrance.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API AMapEntrance : public ACheckPoint
{
	GENERATED_BODY()
public:
    AMapEntrance(const FObjectInitializer& ObjectInitializer);

    /* 高亮接口 */
    virtual void HighlightActor_Implementation() override;
    /* 高亮接口结束 */

    virtual void LoadActor_Implementation() override;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UWorld> DestinationMap;

    UPROPERTY(EditAnywhere)
    FName DestinationPlayerStartTag;

protected:
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    
};
