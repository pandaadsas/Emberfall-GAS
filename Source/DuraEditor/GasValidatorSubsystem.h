// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GasValidatorTypes.h"
#include "GasValidatorSubsystem.generated.h"

/**
 * 校验器编辑器子系统：扫描逻辑的"服务端"。
 *
 * 为什么要有它：
 *  - 面板按钮、控制台命令、以后的 CI 脚本，都调用同一个 StartScan()，
 *    谁都不需要自己懂扫描细节；
 *  - 扫描结果存在子系统里，谁想要谁去拿，面板关了再开结果也还在；
 *  - 扫描完成广播 OnValidationCompleted，面板订阅它刷新列表，互相解耦。
 */
UCLASS()
class UGasValidatorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValidationCompleted, const FGasValidationResult& /*Result*/);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 同步执行一次完整扫描，然后广播结果 */
	void StartScan();

	const FGasValidationResult& GetLastResult() const { return LastResult; }

	/** 把最近一次结果导出为 Markdown 报告，返回文件完整路径（没扫过则返回空串） */
	FString ExportReport() const;

	FOnValidationCompleted OnValidationCompleted;

private:
	FGasValidationResult LastResult;
};
