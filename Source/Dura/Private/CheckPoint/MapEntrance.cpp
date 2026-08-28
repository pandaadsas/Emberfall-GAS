// Copyright by person HDD  


#include "CheckPoint/MapEntrance.h"
#include "CheckPoint/CheckPoint.h"
#include "Components/SphereComponent.h"
#include "Interaction/PlayerInterface.h"
#include "Game/DuraGameModeBase.h"
#include "Kismet/GameplayStatics.h"


void AMapEntrance::LoadActor_Implementation()
{
    //地图入口在存档加载时无需任何操作
}

void AMapEntrance::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if(OtherActor->Implements<UPlayerInterface>())
    {
        bReached = true;

        //保存世界状态
        if(ADuraGameModeBase* DuraGM = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
        {
            DuraGM->SaveWorldState(GetWorld(), DestinationMap.ToSoftObjectPath().GetAssetName());
        }

        //保存玩家状态
        IPlayerInterface::Execute_SaveProgress(OtherActor, DestinationPlayerStartTag);

        UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationMap);
    }
}

AMapEntrance::AMapEntrance(const FObjectInitializer& ObjectInitializer)
    :Super(ObjectInitializer)
{
    Sphere->SetupAttachment(MoveToComponent);
}

void AMapEntrance::HighlightActor_Implementation()
{
    
}
