// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "Actor/PointCollection.h"
#include <AbilitySystem\DuraAbilitySystemLibrary.h>
#include <Kismet\KismetMathLibrary.h>


APointCollection::APointCollection()
{
	PrimaryActorTick.bCanEverTick = false;

    Pt_0 = CreateDefaultSubobject<USceneComponent>("Pt_0");
    ImmutablePts.Add(Pt_0);
    SetRootComponent(Pt_0);

    Pt_1 = CreateDefaultSubobject<USceneComponent>("Pt_1");
    ImmutablePts.Add(Pt_1);
    Pt_1->SetupAttachment(GetRootComponent());

    Pt_2 = CreateDefaultSubobject<USceneComponent>("Pt_2");
    ImmutablePts.Add(Pt_2);
    Pt_2->SetupAttachment(GetRootComponent());

    Pt_3 = CreateDefaultSubobject<USceneComponent>("Pt_3");
    ImmutablePts.Add(Pt_3);
    Pt_3->SetupAttachment(GetRootComponent());

    Pt_4 = CreateDefaultSubobject<USceneComponent>("Pt_4");
    ImmutablePts.Add(Pt_4);
    Pt_4->SetupAttachment(GetRootComponent());

    Pt_5 = CreateDefaultSubobject<USceneComponent>("Pt_5");
    ImmutablePts.Add(Pt_5);
    Pt_5->SetupAttachment(GetRootComponent());

    Pt_6 = CreateDefaultSubobject<USceneComponent>("Pt_6");
    ImmutablePts.Add(Pt_6);
    Pt_6->SetupAttachment(GetRootComponent());

    Pt_7 = CreateDefaultSubobject<USceneComponent>("Pt_7");
    ImmutablePts.Add(Pt_7);
    Pt_7->SetupAttachment(GetRootComponent());

    Pt_8 = CreateDefaultSubobject<USceneComponent>("Pt_8");
    ImmutablePts.Add(Pt_8);
    Pt_8->SetupAttachment(GetRootComponent());

    Pt_9 = CreateDefaultSubobject<USceneComponent>("Pt_9");
    ImmutablePts.Add(Pt_9);
    Pt_9->SetupAttachment(GetRootComponent());

}


TArray<USceneComponent*> APointCollection::GetGroundPoints(const FVector& GroundLocation, 
    int32 NumPoints, float YawOverride /*= 0.f*/)
{
    checkf(ImmutablePts.Num() >= NumPoints, TEXT("Attempted to access ImmutablePts out of bounds."));

    TArray<USceneComponent*> ArrayCopy;

    // 所有探测点共用同一中心，生存者列表只需查询一次；结果同时作为射线检测的忽略列表
    TArray<AActor*> IgnoreActors;
    UDuraAbilitySystemLibrary::GetLivePlayersWithinRadius(this, IgnoreActors, TArray<AActor*>(), 1500.f, GetActorLocation());

    for (USceneComponent* Pt : ImmutablePts)
    {
        if(ArrayCopy.Num() >= NumPoints) return ArrayCopy;

        if(Pt != Pt_0)
        {
            FVector ToPoint = Pt->GetComponentLocation() - Pt_0->GetComponentLocation();
            ToPoint = ToPoint.RotateAngleAxis(YawOverride, FVector::UpVector);
            Pt->SetWorldLocation(Pt_0->GetComponentLocation() + ToPoint);
        }

        const FVector RaisedLocation = FVector(Pt->GetComponentLocation().X, Pt->GetComponentLocation().Y,Pt->GetComponentLocation().Z + 500.f);
        const FVector LoweredLocation = FVector(Pt->GetComponentLocation().X, Pt->GetComponentLocation().Y,Pt->GetComponentLocation().Z - 500.f);

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActors(IgnoreActors);
        FHitResult HitResult;
        GetWorld()->LineTraceSingleByProfile(HitResult, RaisedLocation, LoweredLocation, FName("BlockAll"), QueryParams);

        // 未命中任何阻挡面时 ImpactPoint/ImpactNormal 无效（会落到 Z=0 和垃圾法线），保持点位不动
        if(HitResult.bBlockingHit)
        {
            const FVector AdjustedLocation = FVector(Pt->GetComponentLocation().X, Pt->GetComponentLocation().Y, HitResult.ImpactPoint.Z);
            Pt->SetWorldLocation(AdjustedLocation);
            Pt->SetWorldRotation(UKismetMathLibrary::MakeRotFromZ(HitResult.ImpactNormal));
        }

        ArrayCopy.Add(Pt);
    }

    return ArrayCopy;
}

void APointCollection::BeginPlay()
{
	Super::BeginPlay();
	
}
