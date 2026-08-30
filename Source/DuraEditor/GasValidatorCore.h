// Copyright by person HDD

#pragma once

#include "CoreMinimal.h"
#include "GasValidatorTypes.h"

/**
 * 校验核心：负责收集资产、跑规则、产出问题列表。
 * 它不碰任何 UI，这样以后想接命令行/CI 也可以直接调用 RunValidation()。
 *
 * 扫描范围：/Game/Blueprints/AbilitySystem（本项目所有 GAS 资产的约定位置）
 * 覆盖资产：GameplayAbility 蓝图、GameplayEffect 蓝图、
 *           UAbilityInfo（DA_AbilityInfo）、UCharacterClassInfo（DA_CharacterClassInfo）、
 *           CurveTable（只计数）。
 */
class FGasValidatorCore
{
public:
	static FGasValidationResult RunValidation();

private:
	/** 判断 /Game 路径是否属于敌人技能目录（敌人的 GA 没有蓝耗/冷却是很正常的，规则会放轻） */
	static bool IsEnemyAsset(const FString& InPackagePath);
	/** 判断路径是否属于被动技能（PassiveSpells / Passive_Startup 目录 + C++ 被动基类） */
	static bool IsPassivePath(const FString& InPackagePath);

	/** FScalableFloat 如果配了曲线引用，检查该行是否真的存在 */
	static bool DoesCurveRowExist(const struct FScalableFloat& ScalableFloat);
};
