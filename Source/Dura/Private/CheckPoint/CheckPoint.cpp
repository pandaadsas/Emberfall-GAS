// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "CheckPoint/CheckPoint.h"
#include "Components/SphereComponent.h"
#include "Interaction/PlayerInterface.h"
#include "Game/DuraGameModeBase.h"
#include "Kismet/GameplayStatics.h"


ACheckPoint::ACheckPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;

    CheckPointMesh = CreateDefaultSubobject<UStaticMeshComponent>("CheckPointMesh");
    CheckPointMesh->SetupAttachment(GetRootComponent());
    CheckPointMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CheckPointMesh->SetCollisionResponseToAllChannels(ECR_Block);

    Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Sphere->SetupAttachment(CheckPointMesh);

    MoveToComponent = CreateDefaultSubobject<USceneComponent>("MoveToComponent");
    MoveToComponent->SetupAttachment(GetRootComponent());

    CheckPointMesh->SetCustomDepthStencilValue(CustomDepthStencilOverride);
    CheckPointMesh->MarkRenderStateDirty();
}

void ACheckPoint::LoadActor_Implementation()
{
    if(bReached)
    {
        HandleGlowEffects();
    }
}

void ACheckPoint::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if(OtherActor->Implements<UPlayerInterface>())
    {
        bReached = true;

        //保存世界状态
        if(ADuraGameModeBase* DuraGM = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
        {
            const UWorld* World = GetWorld();
            FString MapName = World->GetMapName();
            MapName.RemoveFromStart(World->StreamingLevelsPrefix);

            DuraGM->SaveWorldState(GetWorld(), MapName);
        }

        //保存玩家状态
        IPlayerInterface::Execute_SaveProgress(OtherActor, PlayerStartTag);


        HandleGlowEffects();
    }
}

void ACheckPoint::BeginPlay()
{
    Super::BeginPlay();

    if(bCallOverlapCallback)
    {
        Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACheckPoint::OnSphereOverlap);
    }
}

void ACheckPoint::UnHighlightActor_Implementation()
{
    CheckPointMesh->SetRenderCustomDepth(false);
}

void ACheckPoint::HighlightActor_Implementation()
{
    if(!bReached)
    {
        CheckPointMesh->SetRenderCustomDepth(true);
    }
}

void ACheckPoint::SetMoveToLocation_Implementation(FVector& OutDestination)
{
    OutDestination = MoveToComponent->GetComponentLocation();
}

void ACheckPoint::HandleGlowEffects()
{
    Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UMaterialInstanceDynamic* DynamicMaterialInstance = UMaterialInstanceDynamic::Create(CheckPointMesh->GetMaterial(0), this);
    CheckPointMesh->SetMaterial(0, DynamicMaterialInstance);
    CheckpointReached(DynamicMaterialInstance);
}
