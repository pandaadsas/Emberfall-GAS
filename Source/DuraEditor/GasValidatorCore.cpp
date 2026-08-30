// Copyright by person HDD

#include "GasValidatorCore.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"
#include "ScalableFloat.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

#include "AbilitySystem/Abilities/DuraDamageGameplayAbility.h"
#include "AbilitySystem/Abilities/DuraPassiveAbility.h"
#include "AbilitySystem/DuraAttributeSet.h"
#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DuraGameplayTags.h"
#include "UObject/UnrealType.h"

// 扫描根目录：本项目把所有 GAS 资产都放在这里
static const FName GasScanRootPath = TEXT("/Game/Blueprints/AbilitySystem");

bool FGasValidatorCore::IsEnemyAsset(const FString& InPackagePath)
{
	return InPackagePath.Contains(TEXT("/Enemy/"), ESearchCase::IgnoreCase);
}

bool FGasValidatorCore::IsPassivePath(const FString& InPackagePath)
{
	return InPackagePath.Contains(TEXT("Passive"), ESearchCase::IgnoreCase);
}

bool FGasValidatorCore::DoesCurveRowExist(const FScalableFloat& ScalableFloat)
{
	// 没配曲线引用 = 直接用 Value 常量，视为合法
	if (ScalableFloat.Curve.IsNull())
	{
		return true;
	}
	return ScalableFloat.Curve.GetCurve(TEXT("GASValidator"), /*bWarnIfNotFound*/ false) != nullptr;
}

namespace
{
	/**
	 * 按 FScalableFloat 的引擎语义手工求值：最终值 = Value × 曲线(Level)。
	 * 不用 GetStaticMagnitudeIfPossible 的原因：它在带曲线引用时未必返回曲线值，
	 * 而这里自己读曲线求值，和游戏运行时的取值逻辑完全一致。
	 */
	float EvaluateScalableFloatAtLevel(const FScalableFloat& ScalableFloat, float Level)
	{
		if (ScalableFloat.Curve.IsNull() || ScalableFloat.Curve.CurveTable == nullptr)
		{
			return ScalableFloat.Value;
		}
		const FRealCurve* Curve = ScalableFloat.Curve.GetCurve(TEXT("GASValidator"), false);
		return Curve ? ScalableFloat.Value * Curve->Eval(Level) : ScalableFloat.Value;
	}
}

/**
 * 从修改器里读出 ScalableFloat 数值。
 * 这个成员在引擎里是 protected 的，但 UPROPERTY 反射数据是完整的，所以用反射安全读取；
 * 引擎改字段名时这里会返回 nullptr，规则自动跳过而不是崩溃。
 */
static const FScalableFloat* GetScalableFloatFromMagnitude(const FGameplayEffectModifierMagnitude& Magnitude)
{
	static FStructProperty* Prop = FindFProperty<FStructProperty>(
		FGameplayEffectModifierMagnitude::StaticStruct(), FName(TEXT("ScalableFloatMagnitude")));
	if (Prop == nullptr)
	{
		return nullptr;
	}
	return Prop->ContainerPtrToValuePtr<FScalableFloat>(&Magnitude);
}

/** 读对象上的 protected TSubclassOf 属性（如 DamageEffectClass）。找不到属性时返回 nullptr，规则自动跳过。 */
static UClass* ReadClassProperty(const UObject* Obj, const FName PropName)
{
	const FClassProperty* Prop = FindFProperty<FClassProperty>(Obj->GetClass(), PropName);
	return Prop ? *Prop->ContainerPtrToValuePtr<UClass*>(Obj) : nullptr;
}

/** 读对象上的 protected FGameplayTag 属性（如 DamageType） */
static const FGameplayTag* ReadTagProperty(const UObject* Obj, const FName PropName)
{
	const FStructProperty* Prop = FindFProperty<FStructProperty>(Obj->GetClass(), PropName);
	return Prop ? Prop->ContainerPtrToValuePtr<FGameplayTag>(Obj) : nullptr;
}

/** 读对象上的 protected FScalableFloat 属性（如 Damage） */
static const FScalableFloat* ReadScalableFloatProperty(const UObject* Obj, const FName PropName)
{
	const FStructProperty* Prop = FindFProperty<FStructProperty>(Obj->GetClass(), PropName);
	return Prop ? Prop->ContainerPtrToValuePtr<FScalableFloat>(Obj) : nullptr;
}

FGasValidationResult FGasValidatorCore::RunValidation()
{
	FGasValidationResult Result;
	const double StartTime = FPlatformTime::Seconds();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// ---------- 第 0 步：收集资产清单 ----------
	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssetsByPath(GasScanRootPath, AllAssets, /*bRecursive*/ true);
	// 按名字排序，保证两次扫描结果顺序一致
	AllAssets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });

	// 一次扫描产生的所有问题都先丢进来
	TArray<FGasValidationIssue> Issues;
	auto AddIssue = [&Issues](EGasValidationSeverity Severity, const FString& RuleId, const FString& Category,
		const FAssetData& Asset, const FString& ObjectPath, const FText& Message)
	{
		FGasValidationIssue Issue;
		Issue.Severity = Severity;
		Issue.RuleId = RuleId;
		Issue.Category = Category;
		Issue.AssetName = Asset.AssetName.ToString();
		Issue.AssetPath = Asset.PackageName.ToString();
		Issue.ObjectPath = ObjectPath.IsEmpty() ? Asset.PackageName.ToString() : ObjectPath;
		Issue.Message = Message;
		Issues.Add(Issue);
	};

	// ---------- 第 1 步：加载资产并按类型分组 ----------
	// GA/GE 的 CDO（类默认对象）就是蓝图上"默认值"那一份数据，编辑器里读它就够了
	TArray<const UGameplayAbility*> AbilityCDOs;
	TArray<FAssetData> AbilityBPAssets;
	TArray<const UGameplayEffect*> EffectCDOs;
	TArray<FAssetData> EffectBPAssets;
	TArray<UAbilityInfo*> AbilityInfoAssets;
	TArray<UCharacterClassInfo*> ClassInfoAssets;
	TSet<FString> ScannedPaths;

	// 交叉引用表：GE 的类路径 -> 谁在用它（GA 资产名列表）
	TMap<FString, TArray<FString>> GEUsedAsCooldown;
	TMap<FString, TArray<FString>> GEUsedAsCost;
	TMap<FString, TArray<FString>> GEUsedAsDamage;
	// DA_AbilityInfo 登记过的 GA 类路径（用于反查"没登记的技能"）
	TSet<FString> RegisteredAbilityPaths;

	for (const FAssetData& AssetData : AllAssets)
	{
		UObject* Loaded = AssetData.GetAsset();
		++Result.ScannedAssetCount;
		ScannedPaths.Add(AssetData.PackageName.ToString());

		if (Loaded == nullptr)
		{
			AddIssue(EGasValidationSeverity::Error, TEXT("Asset.LoadFailed"), TEXT("引用完整性"),
				AssetData, FString(),
				FText::FromString(TEXT("资产加载失败：文件可能损坏或内部引用丢失，建议右键资产尝试修复或重新导入。")));
			continue;
		}

		// 蓝图类：看生成类继承的是 GA 还是 GE
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Loaded))
		{
			UClass* GeneratedClass = Blueprint->GeneratedClass;
			if (GeneratedClass == nullptr)
			{
				continue;
			}

			if (UGameplayAbility* GACDO = Cast<UGameplayAbility>(GeneratedClass->GetDefaultObject()))
			{
				AbilityCDOs.Add(GACDO);
				AbilityBPAssets.Add(AssetData);
			}
			else if (UGameplayEffect* GECDO = Cast<UGameplayEffect>(GeneratedClass->GetDefaultObject()))
			{
				EffectCDOs.Add(GECDO);
				EffectBPAssets.Add(AssetData);
			}
			continue;
		}

		// 数据资产
		if (UAbilityInfo* AbilityInfo = Cast<UAbilityInfo>(Loaded))
		{
			AbilityInfoAssets.Add(AbilityInfo);
		}
		else if (UCharacterClassInfo* ClassInfo = Cast<UCharacterClassInfo>(Loaded))
		{
			ClassInfoAssets.Add(ClassInfo);
		}
		// CurveTable 等其他资产：计入扫描数量即可，没有专属规则
	}

	// ---------- 第 2 步：先扫 GA，建立交叉引用 ----------
	for (int32 Index = 0; Index < AbilityCDOs.Num(); ++Index)
	{
		const UGameplayAbility* GACDO = AbilityCDOs[Index];
		const FAssetData& AssetData = AbilityBPAssets[Index];
		const FString ClassPath = GACDO->GetClass()->GetPathName();
		const FString FullObjectPath = ClassPath;
		const bool bEnemy = IsEnemyAsset(AssetData.PackageName.ToString());
		const bool bPassive = IsPassivePath(AssetData.PackageName.ToString()) || GACDO->IsA<UDuraPassiveAbility>();
		const bool bDamageAbility = GACDO->IsA<UDuraDamageGameplayAbility>();
		const UDuraDamageGameplayAbility* DamageGA = Cast<UDuraDamageGameplayAbility>(GACDO);

		// ---- 冷却 GE ----（GetCooldownGameplayEffect 是引擎公开接口，返回该技能配置的冷却 GE 的 CDO）
		if (const UGameplayEffect* CooldownGE = GACDO->GetCooldownGameplayEffect())
		{
			GEUsedAsCooldown.FindOrAdd(CooldownGE->GetClass()->GetPathName()).Add(AssetData.AssetName.ToString());
		}
		else if (!bPassive)
		{
			// 敌人的普通攻击本来就没有冷却配置，放宽为提示
			if (bEnemy)
			{
				AddIssue(EGasValidationSeverity::Info, TEXT("GA.NoCooldownGE"), TEXT("成本与冷却"),
					AssetData, FullObjectPath,
					FText::FromString(TEXT("该敌人技能没有配置 Cooldown GE。若攻击不需要冷却可以忽略。")));
			}
			else
			{
				AddIssue(EGasValidationSeverity::Warning, TEXT("GA.NoCooldownGE"), TEXT("成本与冷却"),
					AssetData, FullObjectPath,
					FText::FromString(TEXT("未配置 Cooldown GE：技能会没有冷却。在蓝图 Class Defaults 的 Cooldown Gameplay Effect Class 里指定 GE_Cooldown_xxx。")));
			}
		}

		// ---- 消耗 GE ----
		if (const UGameplayEffect* CostGE = GACDO->GetCostGameplayEffect())
		{
			GEUsedAsCost.FindOrAdd(CostGE->GetClass()->GetPathName()).Add(AssetData.AssetName.ToString());
		}
		else if (!bPassive && !bEnemy)
		{
			// 被动技能和敌人技能不消耗蓝是正常设计，只在玩家主动技能上警告
			AddIssue(EGasValidationSeverity::Warning, TEXT("GA.NoCostGE"), TEXT("成本与冷却"),
				AssetData, FullObjectPath,
				FText::FromString(TEXT("未配置 Cost GE：技能施放不会消耗蓝量。在蓝图 Class Defaults 的 Cost Gameplay Effect Class 里指定 GE_Cost_xxx。")));
		}

		// ---- 伤害技能专属 ----
		if (bDamageAbility)
		{
			// DamageEffectClass / DamageType / Damage 在 Dura 类里是 protected 的，用反射安全读取
			const UClass* DamageEffectClass = ReadClassProperty(DamageGA, TEXT("DamageEffectClass"));
			const FGameplayTag* DamageType = ReadTagProperty(DamageGA, TEXT("DamageType"));
			const FScalableFloat* DamageFloat = ReadScalableFloatProperty(DamageGA, TEXT("Damage"));

			if (DamageEffectClass == nullptr)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GA.NoDamageEffectClass"), TEXT("引用完整性"),
					AssetData, FullObjectPath,
					FText::FromString(TEXT("伤害技能未配置 DamageEffectClass：技能命中后不会造成任何伤害。在蓝图 Class Defaults 里指定 GE_Damage。")));
			}
			else
			{
				GEUsedAsDamage.FindOrAdd(DamageEffectClass->GetPathName()).Add(AssetData.AssetName.ToString());
			}

			if (DamageType == nullptr || !DamageType->IsValid())
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GA.DamageTypeInvalid"), TEXT("标签一致性"),
					AssetData, FullObjectPath,
					FText::FromString(TEXT("DamageType 标签未设置：ExecCalc_Damage 按这个标签读取 SetByCaller 伤害，缺了它最终伤害恒为 0。")));
			}
			else if (FDuraGameplayTags::Get().Damage.IsValid() && !DamageType->MatchesTag(FDuraGameplayTags::Get().Damage))
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GA.DamageTypeNotDamage"), TEXT("标签一致性"),
					AssetData, FullObjectPath,
					FText::FromString(FString::Printf(TEXT("DamageType 设为了「%s」，它不是 Damage 系列标签。ExecCalc_Damage 只认 Damage.Fire / Damage.Lightning 等伤害标签，否则伤害恒为 0。"),
						*DamageType->ToString())));
			}

			// 伤害曲线：配了曲线表+行名的话，行必须真实存在
			if (DamageFloat != nullptr)
			{
				if (!DoesCurveRowExist(*DamageFloat))
				{
					AddIssue(EGasValidationSeverity::Error, TEXT("GA.DamageCurveRowMissing"), TEXT("数值曲线"),
						AssetData, FullObjectPath,
						FText::FromString(TEXT("Damage 配置引用的曲线表行不存在：运行时取不到伤害数值（结果为 0）。检查曲线表资产与 RowName 是否拼写一致。")));
				}
				else if (DamageFloat->Value == 0.f && DamageFloat->Curve.IsNull())
				{
					AddIssue(EGasValidationSeverity::Warning, TEXT("GA.DamageZero"), TEXT("数值曲线"),
						AssetData, FullObjectPath,
						FText::FromString(TEXT("Damage 基础值为 0 且未配曲线：若该技能本应造成伤害请补上数值。")));
				}
			}
		}
	}

	// ---------- 第 3 步：扫数据资产（DA_AbilityInfo / DA_CharacterClassInfo） ----------
	for (UAbilityInfo* AbilityInfo : AbilityInfoAssets)
	{
		const FAssetData InfoAssetData(AbilityInfo);

		// 重复 AbilityTag 检测用
		TMap<FString, int32> FirstSeenIndex;

		for (int32 EntryIndex = 0; EntryIndex < AbilityInfo->AbilityInformation.Num(); ++EntryIndex)
		{
			const FDuraAbilityInfo& Entry = AbilityInfo->AbilityInformation[EntryIndex];
			const FString EntryLabel = FString::Printf(TEXT("第 %d 条（%s）"), EntryIndex + 1, *Entry.AbilityTag.ToString());

			// 技能类是否配置
			if (Entry.Ability == nullptr)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("DA.EntryMissingAbility"), TEXT("数据资产"),
					InfoAssetData, AbilityInfo->GetPathName(),
					FText::FromString(FString::Printf(TEXT("%s 没有配置 Ability（技能类）：该条目在技能菜单里将是空壳，无法学习/升级。"), *EntryLabel)));
				continue;
			}

			RegisteredAbilityPaths.Add(Entry.Ability->GetPathName());

			// AbilityTag
			if (!Entry.AbilityTag.IsValid())
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("DA.AbilityTagInvalid"), TEXT("标签一致性"),
					InfoAssetData, AbilityInfo->GetPathName(),
					FText::FromString(FString::Printf(TEXT("%s 的 AbilityTag 无效：技能菜单靠它识别技能，缺了会导致解锁/升级/装备全部失效。"), *EntryLabel)));
			}

			// AbilityTag 重复 = 菜单里永远只找得到第一条
			if (Entry.AbilityTag.IsValid())
			{
				const FString TagString = Entry.AbilityTag.ToString();
				int32* Seen = FirstSeenIndex.Find(TagString);
				if (Seen)
				{
					AddIssue(EGasValidationSeverity::Error, TEXT("DA.AbilityTagDuplicate"), TEXT("标签一致性"),
						InfoAssetData, AbilityInfo->GetPathName(),
						FText::FromString(FString::Printf(TEXT("%s 的 AbilityTag「%s」与第 %d 条重复：查询永远命中第一条，第二条实际无法被访问。"),
							*EntryLabel, *TagString, *Seen + 1)));
				}
				else
				{
					FirstSeenIndex.Add(TagString, EntryIndex);
				}
			}

			// 图标 / 背景材质
			if (Entry.Icon == nullptr)
			{
				AddIssue(EGasValidationSeverity::Warning, TEXT("DA.IconMissing"), TEXT("数据资产"),
					InfoAssetData, AbilityInfo->GetPathName(),
					FText::FromString(FString::Printf(TEXT("%s 没有配置 Icon：技能菜单/快捷栏里会显示为空白图标。"), *EntryLabel)));
			}
			if (Entry.BackgroundMaterial == nullptr)
			{
				AddIssue(EGasValidationSeverity::Info, TEXT("DA.BackgroundMissing"), TEXT("数据资产"),
					InfoAssetData, AbilityInfo->GetPathName(),
					FText::FromString(FString::Printf(TEXT("%s 没有配置 BackgroundMaterial：技能卡片背景会使用默认样式。"), *EntryLabel)));
			}

			// 等级需求
			if (Entry.LevelRequirement < 1)
			{
				AddIssue(EGasValidationSeverity::Warning, TEXT("DA.LevelRequirementInvalid"), TEXT("数据资产"),
					InfoAssetData, AbilityInfo->GetPathName(),
					FText::FromString(FString::Printf(TEXT("%s 的 LevelRequirement 为 %d：等级需求小于 1 会让锁定的技能文案出现负数等级。"),
						*EntryLabel, Entry.LevelRequirement)));
			}

			// 注：InputTag 不做校验——本项目的快捷栏槽位（InputTag）是在运行时由技能菜单
			// 按装备槽分配的（见 SpellMenuWidgetController / DuraOverlayWidgetController），
			// 资产里留空是设计内的正常状态，不是配置错误。

			// CooldownTag 与冷却 GE 授予的标签交叉校验
			const UGameplayAbility* EntryGA = Entry.Ability->GetDefaultObject<UGameplayAbility>();
			if (EntryGA)
			{
				const UGameplayEffect* CooldownGE = EntryGA->GetCooldownGameplayEffect();
				if (CooldownGE)
				{
					const FGameplayTagContainer& GrantedTags = CooldownGE->GetGrantedTags();
					if (GrantedTags.Num() > 0)
					{
						if (Entry.CooldownTag.IsValid() && !GrantedTags.HasTagExact(Entry.CooldownTag))
						{
							AddIssue(EGasValidationSeverity::Warning, TEXT("DA.CooldownTagMismatch"), TEXT("标签一致性"),
								InfoAssetData, AbilityInfo->GetPathName(),
								FText::FromString(FString::Printf(TEXT("%s 的 CooldownTag（%s）与其冷却 GE 授予的标签（%s）不一致：技能菜单刷新冷却状态时会失效。"),
									*EntryLabel, *Entry.CooldownTag.ToString(), *GrantedTags.ToStringSimple())));
						}
						else if (!Entry.CooldownTag.IsValid())
						{
							AddIssue(EGasValidationSeverity::Info, TEXT("DA.CooldownTagMissing"), TEXT("标签一致性"),
								InfoAssetData, AbilityInfo->GetPathName(),
								FText::FromString(FString::Printf(TEXT("%s 未填写 CooldownTag（冷却 GE 授予的是「%s」）：技能菜单将无法显示该技能的冷却。"),
									*EntryLabel, *GrantedTags.ToStringSimple())));
						}
					}
				}
			}
		}
	}

	for (UCharacterClassInfo* ClassInfo : ClassInfoAssets)
	{
		const FAssetData InfoAssetData(ClassInfo);
		const FString InfoPath = ClassInfo->GetPathName();

		if (ClassInfo->PrimaryAttributesSetByCaller == nullptr)
			AddIssue(EGasValidationSeverity::Error, TEXT("CCI.PrimarySetByCallerMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
				FText::FromString(TEXT("PrimaryAttributesSetByCaller 未配置：读档恢复属性时主属性将初始化失败。")));
		if (ClassInfo->SecondaryAttributes == nullptr)
			AddIssue(EGasValidationSeverity::Error, TEXT("CCI.SecondaryMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
				FText::FromString(TEXT("SecondaryAttributes 未配置：角色次级属性（护甲/暴击等）不会被初始化。")));
		if (ClassInfo->SecondaryAttributes_Infinite == nullptr)
			AddIssue(EGasValidationSeverity::Error, TEXT("CCI.SecondaryInfiniteMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
				FText::FromString(TEXT("SecondaryAttributes_Infinite 未配置：蓝量/体力回复等持续效果不会生效。")));
		if (ClassInfo->VitalAttributes == nullptr)
			AddIssue(EGasValidationSeverity::Error, TEXT("CCI.VitalMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
				FText::FromString(TEXT("VitalAttributes 未配置：角色血量/蓝量不会被初始化。")));
		if (ClassInfo->DamageCalculationCoefficients == nullptr)
			AddIssue(EGasValidationSeverity::Warning, TEXT("CCI.DamageCoefficientsMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
				FText::FromString(TEXT("DamageCalculationCoefficients 未配置：伤害系数曲线缺失，伤害数值可能不符合预期。")));

		for (const TPair<ECharacterClass, FCharacterClassDefaultInfo>& Pair : ClassInfo->CharacterClassInformation)
		{
			const FString ClassLabel = FString::Printf(TEXT("职业「%d」"), (int32)Pair.Key);
			const FCharacterClassDefaultInfo& Info = Pair.Value;

			if (Info.PrimaryAttributes == nullptr)
				AddIssue(EGasValidationSeverity::Error, TEXT("CCI.PrimaryAttributesMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
					FText::FromString(FString::Printf(TEXT("%s 的 PrimaryAttributes 未配置：该职业角色主属性全为 0。"), *ClassLabel)));

			for (int32 AbilityIndex = 0; AbilityIndex < Info.StartupAbilities.Num(); ++AbilityIndex)
			{
				if (Info.StartupAbilities[AbilityIndex] == nullptr)
				{
					AddIssue(EGasValidationSeverity::Error, TEXT("CCI.StartupAbilityMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
						FText::FromString(FString::Printf(TEXT("%s 的 StartupAbilities 第 %d 项为空：出生授予技能时会因无效类被跳过或报错。"),
							*ClassLabel, AbilityIndex + 1)));
				}
				else
				{
					RegisteredAbilityPaths.Add(Info.StartupAbilities[AbilityIndex]->GetPathName());
				}
			}

			if (!DoesCurveRowExist(Info.XPReward))
				AddIssue(EGasValidationSeverity::Error, TEXT("CCI.XPRewardCurveMissing"), TEXT("数值曲线"), InfoAssetData, InfoPath,
					FText::FromString(FString::Printf(TEXT("%s 的 XPReward 引用的曲线行不存在：击杀该职业怪物将得不到经验。"), *ClassLabel)));

			for (int32 AbilityIndex = 0; AbilityIndex < ClassInfo->CommonAbilities.Num(); ++AbilityIndex)
			{
				if (ClassInfo->CommonAbilities[AbilityIndex] == nullptr)
				{
					AddIssue(EGasValidationSeverity::Error, TEXT("CCI.CommonAbilityMissing"), TEXT("数据资产"), InfoAssetData, InfoPath,
						FText::FromString(FString::Printf(TEXT("CommonAbilities 第 %d 项为空：所有职业出生时都会因无效技能类出问题。"), AbilityIndex + 1)));
				}
			}
		}
	}

	// ---------- 第 4 步：扫 GE（需要第 2/3 步建立的交叉引用） ----------
	for (int32 Index = 0; Index < EffectCDOs.Num(); ++Index)
	{
		const UGameplayEffect* GECDO = EffectCDOs[Index];
		const FAssetData& AssetData = EffectBPAssets[Index];
		const FString ClassPath = GECDO->GetClass()->GetPathName();

		const TArray<FString>* CooldownUsers = GEUsedAsCooldown.Find(ClassPath);
		const TArray<FString>* CostUsers = GEUsedAsCost.Find(ClassPath);
		const TArray<FString>* DamageUsers = GEUsedAsDamage.Find(ClassPath);

		// 完全空壳的 GE
		if (GECDO->DurationPolicy == EGameplayEffectDurationType::Instant
			&& GECDO->Modifiers.Num() == 0
			&& GECDO->Executions.Num() == 0
			&& GECDO->GetGrantedTags().Num() == 0)
		{
			AddIssue(EGasValidationSeverity::Warning, TEXT("GE.Empty"), TEXT("引用完整性"),
				AssetData, ClassPath,
				FText::FromString(TEXT("该 GE 没有任何实际效果（无持续时间、无修改器、无执行器、不授予标签）。要么没配完，要么是废件。")));
		}

		// 被用作冷却 GE 时：必须有持续时间、时长>0、并且授予冷却标签
		if (CooldownUsers)
		{
			if (GECDO->DurationPolicy != EGameplayEffectDurationType::HasDuration)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GE.CooldownPolicyWrong"), TEXT("成本与冷却"),
					AssetData, ClassPath,
					FText::FromString(FString::Printf(TEXT("它被 %s 用作冷却 GE，但持续时间策略不是 HasDuration：冷却会立刻结束。把 Duration Policy 改成 Has Duration。"),
						*FString::Join(*CooldownUsers, TEXT("、")))));
			}
			else
			{
				float DurationValue = 0.f;
				const FScalableFloat* DurationFloat = GetScalableFloatFromMagnitude(GECDO->DurationMagnitude);
				if (DurationFloat != nullptr)
				{
					DurationValue = EvaluateScalableFloatAtLevel(*DurationFloat, 1.f);
				}
				if (DurationValue <= 0.f)
				{
					AddIssue(EGasValidationSeverity::Warning, TEXT("GE.CooldownDurationZero"), TEXT("成本与冷却"),
						AssetData, ClassPath,
						FText::FromString(FString::Printf(TEXT("它被 %s 用作冷却 GE，但 1 级持续时间为 0：先确认 Set Duration 数值/曲线已配置。"),
							*FString::Join(*CooldownUsers, TEXT("、")))));
				}
			}

			if (GECDO->GetGrantedTags().Num() == 0)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GE.CooldownNoGrantedTag"), TEXT("成本与冷却"),
					AssetData, ClassPath,
					FText::FromString(FString::Printf(TEXT("它被 %s 用作冷却 GE，但没有授予任何 GameplayTag：冷却移除监听（WaitCooldownChange）靠标签恢复技能，缺了它技能会永久卡死无法再释放。"),
						*FString::Join(*CooldownUsers, TEXT("、")))));
			}
		}

		// 被玩家技能用作 Cost GE 时：必须修改 Mana 属性
		if (CostUsers)
		{
			bool bHasManaModifier = false;
			for (const FGameplayModifierInfo& Mod : GECDO->Modifiers)
			{
				if (Mod.Attribute == UDuraAttributeSet::GetManaAttribute())
				{
					bHasManaModifier = true;
					break;
				}
			}

			if (!bHasManaModifier)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GE.CostNoManaModifier"), TEXT("成本与冷却"),
					AssetData, ClassPath,
					FText::FromString(FString::Printf(TEXT("它被 %s 用作 Cost GE，但没有对 Mana 属性的修改器：技能实际不会消耗蓝量。本项目约定 Cost GE 直接加一条 Mana 的 Additive 数值（按等级配置）。"),
						*FString::Join(*CostUsers, TEXT("、")))));
			}
			else
			{
				float CostValue = 0.f;
				for (const FGameplayModifierInfo& Mod : GECDO->Modifiers)
				{
					if (Mod.Attribute == UDuraAttributeSet::GetManaAttribute())
					{
						const FScalableFloat* CostFloat = GetScalableFloatFromMagnitude(Mod.ModifierMagnitude);
						if (CostFloat != nullptr)
						{
							CostValue = EvaluateScalableFloatAtLevel(*CostFloat, 1.f);
						}
						break;
					}
				}
				// 本项目（GAS 惯例）蓝耗配成负数，游戏侧用 FMath::Abs 取绝对值，
				// 所以只把"绝对值为 0"当成配置缺失
				if (FMath::IsNearlyZero(FMath::Abs(CostValue)))
				{
					AddIssue(EGasValidationSeverity::Warning, TEXT("GE.CostZero"), TEXT("数值曲线"),
						AssetData, ClassPath,
						FText::FromString(FString::Printf(TEXT("它被 %s 用作 Cost GE，但 1 级蓝耗为 0：检查 Mana 修改器的数值/曲线是否配置（本项目蓝耗为负数存储，绝对值即消耗量）。"),
							*FString::Join(*CostUsers, TEXT("、")))));
				}
			}
		}

		// 被用作伤害 GE 时：必须挂 ExecCalc_Damage 执行计算器
		if (DamageUsers)
		{
			bool bHasDamageExec = false;
			for (const FGameplayEffectExecutionDefinition& ExecDef : GECDO->Executions)
			{
				if (ExecDef.CalculationClass && ExecDef.CalculationClass->IsChildOf(UExecCalc_Damage::StaticClass()))
				{
					bHasDamageExec = true;
					break;
				}
			}
			if (!bHasDamageExec)
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GE.DamageNoExec"), TEXT("引用完整性"),
					AssetData, ClassPath,
					FText::FromString(FString::Printf(TEXT("它被 %s 用作伤害 GE，但没有挂 ExecCalc_Damage 执行计算器：所有伤害/暴击/减益计算都不会发生。"),
						*FString::Join(*DamageUsers, TEXT("、")))));
			}
		}

		// 所有修改器的曲线行存在性
		for (const FGameplayModifierInfo& Mod : GECDO->Modifiers)
		{
			const FScalableFloat* MagnitudeFloat = GetScalableFloatFromMagnitude(Mod.ModifierMagnitude);
			if (MagnitudeFloat != nullptr && !DoesCurveRowExist(*MagnitudeFloat))
			{
				AddIssue(EGasValidationSeverity::Error, TEXT("GE.ModifierCurveRowMissing"), TEXT("数值曲线"),
					AssetData, ClassPath,
					FText::FromString(FString::Printf(TEXT("修改器「%s」引用的曲线表行不存在：该条修改器在运行时取值会退化为 0。"),
						*Mod.Attribute.GetName())));
			}
		}
	}

	// ---------- 第 5 步：反查没登记进数据资产的玩家主动技能 ----------
	for (int32 Index = 0; Index < AbilityCDOs.Num(); ++Index)
	{
		const FAssetData& AssetData = AbilityBPAssets[Index];
		const UGameplayAbility* GACDO = AbilityCDOs[Index];
		const FString PackagePath = AssetData.PackageName.ToString();

		const bool bEnemy = IsEnemyAsset(PackagePath);
		const bool bPassive = IsPassivePath(PackagePath) || GACDO->IsA<UDuraPassiveAbility>();
		const FString ClassPath = GACDO->GetClass()->GetPathName();

		if (!bEnemy && !bPassive && !RegisteredAbilityPaths.Contains(ClassPath))
		{
			AddIssue(EGasValidationSeverity::Info, TEXT("GA.NotRegistered"), TEXT("数据资产"),
				AssetData, ClassPath,
				FText::FromString(TEXT("该技能没有登记进 DA_AbilityInfo / DA_CharacterClassInfo：如果它是玩家可学习/解锁的技能，技能菜单里将看不到它。")));
		}
	}

	// ---------- 汇总 ----------
	Issues.Sort([](const FGasValidationIssue& A, const FGasValidationIssue& B)
	{
		if (A.Severity != B.Severity) return A.Severity < B.Severity;
		if (A.Category != B.Category) return A.Category < B.Category;
		return A.AssetName < B.AssetName;
	});
	Result.Issues = MoveTemp(Issues);
	Result.Recount();
	Result.ElapsedSeconds = FPlatformTime::Seconds() - StartTime;
	return Result;
}

void FGasValidationResult::Recount()
{
	ErrorCount = 0;
	WarningCount = 0;
	InfoCount = 0;
	for (const FGasValidationIssue& Issue : Issues)
	{
		switch (Issue.Severity)
		{
		case EGasValidationSeverity::Error:   ++ErrorCount;   break;
		case EGasValidationSeverity::Warning: ++WarningCount; break;
		case EGasValidationSeverity::Info:    ++InfoCount;    break;
		}
	}
}
