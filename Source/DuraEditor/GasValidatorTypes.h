// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"

/**
 * 问题的严重程度：
 *  - Error：配置一定有问题，运行时会出 bug（技能放不出、不耗蓝、不掉血等）
 *  - Warning：大概率有问题，或者约定没遵守（例如图标没配、数值可疑）
 *  - Info：不算错，但值得知道（例如技能没登记到 DA_AbilityInfo）
 */
enum class EGasValidationSeverity : uint8
{
	Error,
	Warning,
	Info
};

/** 一条校验结果。每条结果对应一个具体资产上的一个具体问题。 */
struct FGasValidationIssue
{
	EGasValidationSeverity Severity = EGasValidationSeverity::Info;

	/** 规则 ID（如 GA.MissingCostGE），用来在文档和日志里精确定位是哪条规则报的 */
	FString RuleId;

	/** 中文类别（成本与冷却 / 引用完整性 / 标签一致性 / 数据资产 / 数值曲线），用于面板过滤 */
	FString Category;

	/** 资产显示名，如 GA_FireBolt */
	FString AssetName;

	/** 资产在 /Game 下的路径 */
	FString AssetPath;

	/** 可打开的完整对象路径（蓝图生成类带 _C 后缀），双击结果行时用 */
	FString ObjectPath;

	/** 人话描述：哪里错了、为什么错、怎么修 */
	FText Message;
};

/** 一次完整扫描的汇总结果 */
struct FGasValidationResult
{
	TArray<FGasValidationIssue> Issues;
	int32 ScannedAssetCount = 0;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	int32 InfoCount = 0;
	double ElapsedSeconds = 0.0;

	void Recount();
};

/** 列表控件的一行数据。包一层 TSharedPtr 交给 SListView 持有。 */
class FGasValidationRowItem : public TSharedFromThis<FGasValidationRowItem>
{
public:
	TSharedPtr<FGasValidationIssue> Issue;

	static TSharedRef<FGasValidationRowItem> Make(const FGasValidationIssue& InIssue)
	{
		TSharedRef<FGasValidationRowItem> Item = MakeShared<FGasValidationRowItem>();
		Item->Issue = MakeShared<FGasValidationIssue>(InIssue);
		return Item;
	}
};

typedef TSharedPtr<FGasValidationRowItem> FGasValidationRowItemPtr;
