// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * DuraEditor 模块入口：
 * 只负责"把工具挂进编辑器"——注册标签页、注册工具栏按钮。
 * 校验逻辑在 GasValidatorCore，界面在 SGasValidatorPanel，各管各的。
 */
class FGasValidatorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 编辑器菜单/工具栏都就绪后再挂按钮（ToolMenus 的标准时机） */
	void RegisterMenus();

	/** 点工具栏按钮 / 窗口菜单时创建面板标签页 */
	TSharedRef<SDockTab> SpawnValidatorTab(const FSpawnTabArgs& Args);
};

/** 全局标签页 ID（工具栏按钮和标签页注册都要用它） */
extern const FName GasValidatorTabName;
