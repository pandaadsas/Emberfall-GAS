// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "UI/HUD/LoadScreenHUD.h"
#include "UI/Widget/LoadScreenWidget.h"
#include "UI/ViewModel/MVVM_LoadScreen.h"

void ALoadScreenHUD::BeginPlay()
{
    Super::BeginPlay();

    LoadScreenViewModel = NewObject<UMVVM_LoadScreen>(this, LoadScreenViewModelClass);
    LoadScreenViewModel->CreateAndInitLoadSlots();

    LoadScreenWidget = CreateWidget<ULoadScreenWidget>(GetWorld(), LoadScreenWidgetClass);
    LoadScreenWidget->AddToViewport();
    LoadScreenWidget->BlueprintInitializeWidget();

    LoadScreenViewModel->LoadSavedSlotDatas();
}
