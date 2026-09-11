// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#include "DuraEditor.h"

#include "SGasValidatorPanel.h"
#include "GasValidatorSubsystem.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "HAL/IConsoleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"

const FName GasValidatorTabName = FName(TEXT("GASValidatorTab"));

// 控制台命令：Dura.GASValidate
// 不开界面直接跑一遍校验：报告进日志，已打开的面板自动刷新——和「开始扫描」按钮走同一条子系统路径。
// -ExecCmds 在引擎初始化早期触发时资产注册表可能还在收集，所以加 15 秒延迟兜底。
static void RunValidatorScanWhenReady()
{
	auto StartScan = []()
	{
		if (GEditor)
		{
			if (UGasValidatorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGasValidatorSubsystem>())
			{
				Subsystem->StartScan();
				// 顺手导出 Markdown 报告：让这一条命令成为完整的"扫描→日志→报告文件"流水线
				const FString ReportPath = Subsystem->ExportReport();
				if (!ReportPath.IsEmpty())
				{
					UE_LOG(LogTemp, Log, TEXT("[GAS校验] 报告文件：%s"), *ReportPath);
				}
			}
		}
	};

	IAssetRegistry* AssetRegistry = FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry"))
		? &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get()
		: nullptr;

	if (AssetRegistry && !AssetRegistry->IsGathering())
	{
		StartScan();
	}
	else
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([StartScan](float DeltaTime)
		{
			StartScan();
			return false; // 只执行一次，注销 Ticker
		}), 15.f);
	}
}

static FAutoConsoleCommand GASValidateCmd(
	TEXT("Dura.GASValidate"),
	TEXT("运行 GAS 配置校验，结果写入日志并刷新校验器面板（引擎启动早期调用会自动延迟）"),
	FConsoleCommandDelegate::CreateStatic(&RunValidatorScanWhenReady));

// 控制台命令：Dura.GASValidatorOpen
static FAutoConsoleCommand GASOpenPanelCmd(
	TEXT("Dura.GASValidatorOpen"),
	TEXT("打开 GAS 配置校验器面板"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(GasValidatorTabName);
	}));

// 控制台命令：Dura.GASValidatorExport
static FAutoConsoleCommand GASExportCmd(
	TEXT("Dura.GASValidatorExport"),
	TEXT("把最近一次校验结果导出为 Markdown 报告（路径写进日志）"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (GEditor)
		{
			if (const UGasValidatorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGasValidatorSubsystem>())
			{
				const FString FilePath = Subsystem->ExportReport();
				UE_LOG(LogTemp, Log, TEXT("[GAS校验] 导出报告：%s"),
					FilePath.IsEmpty() ? TEXT("没有可导出的结果，请先执行 Dura.GASValidate") : *FilePath);
			}
		}
	}));

void FGasValidatorEditorModule::StartupModule()
{
	// Nomad 标签页：独立于关卡存在的"工具页"，会自动出现在编辑器的 窗口 菜单里
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GasValidatorTabName,
		FOnSpawnTab::CreateRaw(this, &FGasValidatorEditorModule::SpawnValidatorTab))
		.SetDisplayName(FText::FromString(TEXT("GAS 配置校验器")))
		.SetTooltipText(FText::FromString(TEXT("扫描 GameplayAbility / GameplayEffect / 数据资产，报告配置错误")))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	// 等编辑器把菜单系统初始化完，再往主工具栏塞按钮
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGasValidatorEditorModule::RegisterMenus));
}

void FGasValidatorEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GasValidatorTabName);
}

void FGasValidatorEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.Toolbar"));
	FToolMenuSection& Section = ToolbarMenu->AddSection(TEXT("DuraGASTools"));
	Section.Label = FText::FromString(TEXT("Dura 工具"));

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		TEXT("GASValidator"),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(GasValidatorTabName);
		})),
		FText::FromString(TEXT("GAS 校验")),
		FText::FromString(TEXT("打开 GAS 配置校验器：一键检查技能/效果/数据资产的配置错误")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.DataAsset"))));
}

TSharedRef<SDockTab> FGasValidatorEditorModule::SpawnValidatorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(FText::FromString(TEXT("GAS 配置校验器")))
		[
			SNew(SGasValidatorPanel)
		];
}

IMPLEMENT_MODULE(FGasValidatorEditorModule, DuraEditor)
