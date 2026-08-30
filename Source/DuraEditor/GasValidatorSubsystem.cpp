// Copyright by person HDD

#include "GasValidatorSubsystem.h"

#include "GasValidatorCore.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogGASValidator, Log, All);

void UGasValidatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGasValidatorSubsystem::Deinitialize()
{
	OnValidationCompleted.Clear();
	Super::Deinitialize();
}

void UGasValidatorSubsystem::StartScan()
{
	LastResult = FGasValidatorCore::RunValidation();

	// 日志里留一份完整报告：不开面板也能看结果，CI 也能采集
	UE_LOG(LogGASValidator, Log, TEXT("[GAS校验] ===== 扫描 %d 个资产 | 错误 %d | 警告 %d | 提示 %d | 耗时 %.2f 秒 ====="),
		LastResult.ScannedAssetCount, LastResult.ErrorCount, LastResult.WarningCount, LastResult.InfoCount, LastResult.ElapsedSeconds);
	for (const FGasValidationIssue& Issue : LastResult.Issues)
	{
		const TCHAR* SeverityLabel = Issue.Severity == EGasValidationSeverity::Error ? TEXT("ERROR")
			: Issue.Severity == EGasValidationSeverity::Warning ? TEXT("WARN ")
			: TEXT("INFO ");
		UE_LOG(LogGASValidator, Log, TEXT("[GAS校验][%s][%s] %s | %s | %s"),
			SeverityLabel, *Issue.RuleId, *Issue.AssetName, *Issue.Message.ToString(), *Issue.AssetPath);
	}
	UE_LOG(LogGASValidator, Log, TEXT("[GAS校验] ===== 报告结束 ====="));

	OnValidationCompleted.Broadcast(LastResult);
}

FString UGasValidatorSubsystem::ExportReport() const
{
	if (LastResult.ScannedAssetCount == 0)
	{
		return FString();
	}

	const FString TimeStamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FilePath = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("GASValidator"),
		FString::Printf(TEXT("GAS校验报告_%s.md"), *TimeStamp));

	FString Report;
	Report += FString::Printf(TEXT("# GAS 配置校验报告\n\n- 时间：%s\n- 扫描资产：%d\n- 错误：%d / 警告：%d / 提示：%d\n- 耗时：%.2f 秒\n\n"),
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
		LastResult.ScannedAssetCount, LastResult.ErrorCount, LastResult.WarningCount, LastResult.InfoCount,
		LastResult.ElapsedSeconds);

	static const TCHAR* SeverityLabels[] = { TEXT("❌ 错误"), TEXT("⚠️ 警告"), TEXT("ℹ️ 提示") };
	for (const FGasValidationIssue& Issue : LastResult.Issues)
	{
		Report += FString::Printf(TEXT("- **%s** | %s | `%s` | **%s**：%s（资产：%s）\n"),
			SeverityLabels[(int32)Issue.Severity],
			*Issue.Category, *Issue.RuleId, *Issue.AssetName,
			*Issue.Message.ToString(), *Issue.AssetPath);
	}

	if (FFileHelper::SaveStringToFile(Report, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		return FilePath;
	}
	return FString();
}
