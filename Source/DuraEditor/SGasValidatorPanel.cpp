// Copyright by person HDD

#include "SGasValidatorPanel.h"

#include "GasValidatorCore.h"
#include "GasValidatorSubsystem.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/ScopedSlowTask.h"

// 列表列定义
#define GAS_COL_Severity TEXT("Severity")
#define GAS_COL_Category TEXT("Category")
#define GAS_COL_RuleId   TEXT("RuleId")
#define GAS_COL_Asset    TEXT("Asset")
#define GAS_COL_Message  TEXT("Message")

void SGasValidatorPanel::Construct(const FArguments& InArgs)
{
	FilterOptions = {
		MakeShared<FString>(TEXT("全部")),
		MakeShared<FString>(TEXT("错误+警告")),
		MakeShared<FString>(TEXT("仅错误"))
	};
	SelectedFilter = FilterOptions[0];

	// 订阅子系统：不管谁触发扫描（按钮/控制台命令/以后的 CI），面板都会自动刷新
	if (GEditor)
	{
		if (UGasValidatorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGasValidatorSubsystem>())
		{
			CachedSubsystem = Subsystem;
			ValidationDelegateHandle = Subsystem->OnValidationCompleted.AddRaw(
				this, &SGasValidatorPanel::HandleValidationCompleted);

			// 如果之前已经扫描过，打开面板直接显示上一次结果
			if (Subsystem->GetLastResult().ScannedAssetCount > 0)
			{
				HandleValidationCompleted(Subsystem->GetLastResult());
			}
		}
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// 顶部工具条
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 6.f, 6.f, 2.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("开始扫描")))
				.ToolTipText(FText::FromString(TEXT("重新扫描 /Game/Blueprints/AbilitySystem 下的全部 GAS 资产")))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SGasValidatorPanel::OnScanClicked)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("导出报告")))
				.ToolTipText(FText::FromString(TEXT("把当前结果导出为 Markdown 报告（Saved/GASValidator/ 目录）")))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SGasValidatorPanel::OnExportClicked)
			]

			+ SHorizontalBox::Slot().FillWidth(1.f) // 弹簧：把右侧控件推过去
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SAssignNew(FilterCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&FilterOptions)
				.InitiallySelectedItem(SelectedFilter)
				.OnSelectionChanged(this, &SGasValidatorPanel::OnFilterSelectionChanged)
				.Content()
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(*SelectedFilter); })
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(FText::FromString(TEXT("按资产/规则/说明搜索…")))
				.OnTextChanged(this, &SGasValidatorPanel::OnSearchTextChanged)
			]
		]

		// 统计行
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 2.f)
		[
			SAssignNew(StatsText, STextBlock)
			.Text(FText::FromString(TEXT("尚未扫描。点击「开始扫描」检查技能配置。")))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 0.f)
		[
			SNew(SSeparator)
		]

		// 结果列表
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 4.f)
		[
			SNew(SBorder)
			.Padding(0.f)
			[
				SAssignNew(ListView, SListView<FGasValidationRowItemPtr>)
				.ListItemsSource(&FilteredRows)
				.OnGenerateRow(this, &SGasValidatorPanel::OnGenerateRow)
				.OnMouseButtonDoubleClick(this, &SGasValidatorPanel::OnRowDoubleClicked)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(GAS_COL_Severity).FillWidth(0.8f)[SNew(STextBlock).Text(FText::FromString(TEXT("严重度")))]
					+ SHeaderRow::Column(GAS_COL_Category).FillWidth(1.1f)[SNew(STextBlock).Text(FText::FromString(TEXT("类别")))]
					+ SHeaderRow::Column(GAS_COL_RuleId).FillWidth(2.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("规则")))]
					+ SHeaderRow::Column(GAS_COL_Asset).FillWidth(1.6f)[SNew(STextBlock).Text(FText::FromString(TEXT("资产")))]
					+ SHeaderRow::Column(GAS_COL_Message).FillWidth(5.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("问题说明")))]
				)
			]
		]
	];
}

FReply SGasValidatorPanel::OnScanClicked()
{
	{
		FScopedSlowTask ScanTask(1.f, FText::FromString(TEXT("正在扫描 GAS 配置资产…")), /*bShowCancelButton*/ false);
		ScanTask.MakeDialog(/*bShowCancelButton*/ false);

		// 扫描本体在子系统里；完成后通过 OnValidationCompleted 广播回到 HandleValidationCompleted 刷界面
		UGasValidatorSubsystem* Subsystem = CachedSubsystem.IsValid()
			? CachedSubsystem.Get()
			: (GEditor ? GEditor->GetEditorSubsystem<UGasValidatorSubsystem>() : nullptr);
		if (Subsystem)
		{
			CachedSubsystem = Subsystem;
			Subsystem->StartScan();
		}
		ScanTask.EnterProgressFrame(1.f);
	}

	return FReply::Handled();
}

void SGasValidatorPanel::HandleValidationCompleted(const FGasValidationResult& Result)
{
	LastResult = Result;

	// 结果转成列表行数据
	FilteredRows.Reset();
	for (const FGasValidationIssue& Issue : LastResult.Issues)
	{
		FilteredRows.Add(FGasValidationRowItem::Make(Issue));
	}
	UpdateStatsText();
	ListView->RequestListRefresh();
}

SGasValidatorPanel::~SGasValidatorPanel()
{
	// 面板销毁前解除订阅，防止子系统之后广播时回调到已销毁的控件
	if (UGasValidatorSubsystem* Subsystem = CachedSubsystem.Get())
	{
		Subsystem->OnValidationCompleted.Remove(ValidationDelegateHandle);
	}
}

FReply SGasValidatorPanel::OnExportClicked()
{
	UGasValidatorSubsystem* Subsystem = CachedSubsystem.IsValid()
		? CachedSubsystem.Get()
		: (GEditor ? GEditor->GetEditorSubsystem<UGasValidatorSubsystem>() : nullptr);

	if (Subsystem == nullptr)
	{
		return FReply::Handled();
	}

	const FString FilePath = Subsystem->ExportReport();
	FNotificationInfo Info = FilePath.IsEmpty()
		? FNotificationInfo(FText::FromString(TEXT("还没有扫描结果，先点「开始扫描」。")))
		: FNotificationInfo(FText::FromString(FString::Printf(TEXT("报告已保存：%s"), *FilePath)));
	Info.ExpireDuration = FilePath.IsEmpty() ? 3.f : 6.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

void SGasValidatorPanel::OnFilterSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedFilter = NewSelection;
		RebuildFilteredRows();
	}
}

void SGasValidatorPanel::OnSearchTextChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
	RebuildFilteredRows();
}

bool SGasValidatorPanel::PassesFilter(const FGasValidationIssue& Issue) const
{
	if (*SelectedFilter == TEXT("仅错误") && Issue.Severity != EGasValidationSeverity::Error)
	{
		return false;
	}
	if (*SelectedFilter == TEXT("错误+警告") && Issue.Severity == EGasValidationSeverity::Info)
	{
		return false;
	}

	if (!SearchText.IsEmpty())
	{
		const FString Haystack = Issue.AssetName + TEXT(" ") + Issue.RuleId + TEXT(" ") + Issue.Message.ToString() + TEXT(" ") + Issue.AssetPath;
		if (!Haystack.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}

void SGasValidatorPanel::RebuildFilteredRows()
{
	FilteredRows.Reset();
	for (const FGasValidationIssue& Issue : LastResult.Issues)
	{
		if (PassesFilter(Issue))
		{
			FilteredRows.Add(FGasValidationRowItem::Make(Issue));
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SGasValidatorPanel::OnGenerateRow(FGasValidationRowItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SGasValidatorRow, OwnerTable).Item(Item);
}

void SGasValidatorPanel::OnRowDoubleClicked(FGasValidationRowItemPtr Item)
{
	if (Item.IsValid() && Item->Issue.IsValid())
	{
		OpenIssueAsset(*Item->Issue);
	}
}

void SGasValidatorPanel::OpenIssueAsset(const FGasValidationIssue& Issue) const
{
	// 打开对应资产的编辑器（蓝图类路径带 _C 后缀，OpenEditorForAsset 能直接处理）
	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(Issue.ObjectPath);
		}
	}

	// 内容浏览器里定位：把蓝图类路径还原成资产路径（去掉结尾的 _C）再同步
	FString AssetObjectPath = Issue.ObjectPath;
	AssetObjectPath.RemoveFromEnd(TEXT("_C"));
	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetObjectPath);

	TArray<FAssetData> AssetsToSync;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get()
		.GetAssetsByPackageName(FName(*PackageName), AssetsToSync);
	if (AssetsToSync.Num() > 0)
	{
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get()
			.SyncBrowserToAssets(AssetsToSync);
	}
}

void SGasValidatorPanel::UpdateStatsText()
{
	StatsText->SetText(FText::FromString(FString::Printf(
		TEXT("扫描 %d 个资产 · 错误 %d · 警告 %d · 提示 %d · 耗时 %.2f 秒"),
		LastResult.ScannedAssetCount, LastResult.ErrorCount, LastResult.WarningCount, LastResult.InfoCount,
		LastResult.ElapsedSeconds)));
}

FSlateColor SGasValidatorPanel::GetSeverityColor(EGasValidationSeverity Severity)
{
	switch (Severity)
	{
	case EGasValidationSeverity::Error:   return FSlateColor(FLinearColor(0.95f, 0.35f, 0.35f));
	case EGasValidationSeverity::Warning: return FSlateColor(FLinearColor(0.95f, 0.8f, 0.35f));
	case EGasValidationSeverity::Info:    return FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
	}
	return FSlateColor::UseForeground();
}

// ---------------- 行控件 ----------------

void SGasValidatorRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<FGasValidationRowItemPtr>::Construct(FSuperRowType::FArguments(), OwnerTable);
}

TSharedRef<SWidget> SGasValidatorRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	const FGasValidationIssue* Issue = Item.IsValid() ? Item->Issue.Get() : nullptr;
	if (Issue == nullptr)
	{
		return SNew(STextBlock).Text(FText::GetEmpty());
	}

	const FGasValidationIssue IssueCopy = *Issue;
	const FText AssetText = FText::FromString(Issue->AssetName);
	const FText MessageText = Issue->Message;
	const FText RuleText = FText::FromString(Issue->RuleId);
	const FText CategoryText = FText::FromString(Issue->Category);
	const FSlateColor SeverityColor = SGasValidatorPanel::GetSeverityColor(Issue->Severity);

	FText SeverityText;
	switch (Issue->Severity)
	{
	case EGasValidationSeverity::Error:   SeverityText = FText::FromString(TEXT("● 错误")); break;
	case EGasValidationSeverity::Warning: SeverityText = FText::FromString(TEXT("● 警告")); break;
	case EGasValidationSeverity::Info:    SeverityText = FText::FromString(TEXT("● 提示")); break;
	}

	FString ColumnString = ColumnName.ToString();
	if (ColumnString == GAS_COL_Severity)
	{
		return SNew(STextBlock).Text(SeverityText).ColorAndOpacity(SeverityColor);
	}
	if (ColumnString == GAS_COL_Category)
	{
		return SNew(STextBlock).Text(CategoryText);
	}
	if (ColumnString == GAS_COL_RuleId)
	{
		return SNew(STextBlock).Text(RuleText).Font(FCoreStyle::GetDefaultFontStyle("Mono", 9));
	}
	if (ColumnString == GAS_COL_Asset)
	{
		return SNew(STextBlock).Text(AssetText).ToolTipText(FText::FromString(IssueCopy.AssetPath));
	}
	// 默认：问题说明列
	return SNew(STextBlock).Text(MessageText).AutoWrapText(true).ToolTipText(MessageText);
}

#undef GAS_COL_Severity
#undef GAS_COL_Category
#undef GAS_COL_RuleId
#undef GAS_COL_Asset
#undef GAS_COL_Message
