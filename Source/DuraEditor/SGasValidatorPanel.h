// Copyright by person HDD

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "GasValidatorTypes.h"

class STextBlock;
class SSearchBox;
class UGasValidatorSubsystem;

/**
 * 「GAS 配置校验器」主面板。
 *
 * 布局：
 *   [开始扫描] [导出报告]      [严重度过滤▾] [搜索框]
 *   统计行：扫描 N 个资产 · X 错误 · Y 警告 · Z 提示 · 耗时 T
 *   ┌────────────────────────────────────────┐
 *   │ 严重度 │ 类别 │ 规则 │ 资产 │ 问题说明 │
 *   └────────────────────────────────────────┘
 * 双击任意一行 = 打开对应资产并把内容浏览器定位过去。
 */
class SGasValidatorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGasValidatorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SGasValidatorPanel();

	/** 行控件渲染严重度颜色用 */
	static FSlateColor GetSeverityColor(EGasValidationSeverity Severity);

private:
	/** 重新跑一遍校验并刷新列表 */
	FReply OnScanClicked();
	/** 把当前问题列表导出成 Markdown 报告（存到 Saved/GASValidator/ 下） */
	FReply OnExportClicked();

	/** 扫描完成（无论从按钮还是控制台命令触发）都走这里刷新界面 */
	void HandleValidationCompleted(const FGasValidationResult& Result);

	/** 严重度过滤下拉框回调 */
	void OnFilterSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnSearchTextChanged(const FText& NewText);

	/** 某条问题是否应该显示（受过滤条件约束） */
	bool PassesFilter(const FGasValidationIssue& Issue) const;
	/** 根据过滤条件重建列表数据 */
	void RebuildFilteredRows();

	TSharedRef<ITableRow> OnGenerateRow(FGasValidationRowItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnRowDoubleClicked(FGasValidationRowItemPtr Item);
	void OpenIssueAsset(const FGasValidationIssue& Issue) const;

	void UpdateStatsText();

	FGasValidationResult LastResult;
	TArray<FGasValidationRowItemPtr> FilteredRows;
	TSharedPtr<SListView<FGasValidationRowItemPtr>> ListView;
	TSharedPtr<STextBlock> StatsText;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FilterCombo;
	TSharedPtr<SSearchBox> SearchBox;

	TArray<TSharedPtr<FString>> FilterOptions;
	TSharedPtr<FString> SelectedFilter;
	FString SearchText;

	/** 订阅子系统扫描完成广播的句柄，析构时注销，避免悬空回调 */
	TWeakObjectPtr<UGasValidatorSubsystem> CachedSubsystem;
	FDelegateHandle ValidationDelegateHandle;
};

/** 结果列表里的一行 UI */
class SGasValidatorRow : public SMultiColumnTableRow<FGasValidationRowItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SGasValidatorRow) {}
		SLATE_ARGUMENT(FGasValidationRowItemPtr, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	FGasValidationRowItemPtr Item;
};
