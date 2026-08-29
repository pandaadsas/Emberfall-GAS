# 05 · 伤害计算与 GE 流水线 —— ExecCalc_Damage 公式引擎与 AbilitySystemLibrary 工具箱

> **逻辑链路一句话概括**：
> `技能/投射物构造 FDamageEffectParams` → `Library::ApplyDamageEffect 把参数注入 GE Spec（SetByCaller + Context）并应用到目标` → `GE 执行阶段调用 ExecCalc_Damage::Execute` → `按伤害类型查抗性表 → 径向/格挡/护甲/暴击层层修正 → 输出 IncomingDamage` → `AttributeSet 结算（第 4 篇）`。
> **本篇回答 GAS 面试第二大必考题：一个伤害数字从"策划配的曲线"到"目标扣血"经历了哪几站。**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/AbilitySystem/ExecCalc/ExecCalc_Damage.h` / `.cpp` | 伤害公式引擎（捕获定义 + Execute + DetermineDebuff） | ★★★★★ |
| 2 | `Source/Dura/Public/AbilitySystem/DuraAbilitySystemLibrary.h` / `.cpp` | 全项目静态工具箱（Context 读写/初始化/查询/几何） | ★★★★★ |

配套资产：`GE_Damage.uasset`（伤害主 GE，Modifiers 配 ExecCalc）、`DA_CharacterClassInfo` 里的 `DamageCalculationCoefficients` 曲线表。

---

# 一、铺垫：为什么伤害计算要用 Execution Calculation

GE 的 Modifier 只有四种数值来源：常数、曲线（ScalableFloat）、属性捕获（Attribute Based）、SetByCaller。它们都算"单点数值"。**但伤害公式需要"读双方十几个属性 + 掷骰 + 分支"**——于是有了 `UGameplayEffectExecutionCalculation`：

- 配置方式：GE 的 **Executors** 数组里添加 ExecCalc 类（本项目 `GE_Damage` 里配的就是 `ExecCalc_Damage`）。
- 触发时机：GE 应用时（Execute 类型 GE 在应用瞬间；Duration 型在首次应用+每周期）。
- 与 Modifier 的区别：**ExecCalc 的输出也是"对某属性的一次修改"**（`OutExecutionOutput.AddOutputModifier`），只是这个修改值由你写代码算出来。

- **通俗解释**：Modifier 是"查表发工资"，ExecCalc 是"HR 现场算绩效"——工龄（护甲）、迟到（格挡）、加班（暴击）全现场核算，最后把工资条（EvaluatedData）交财务（属性聚合器）。

---

# 二、DuraDamageStatics —— 捕获声明三件套 ★★★★★

## 2.1 结构体与单例

```cpp
struct DuraDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor_Penetration);
    ...（10 个）

    DuraDamageStatics()
    {
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDuraAttributeSet, Armor_Penetration, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDuraAttributeSet, Armor, Target, false);
        ...
    }
};

static const DuraDamageStatics& DamageStatics()
{
    static DuraDamageStatics DStatics;
    return DStatics;
}
```

- **`DECLARE_ATTRIBUTE_CAPTUREDEF(名字)`**：声明一个 `FGameplayEffectAttributeCaptureDefinition 名字Def;` 成员。
- **`DEFINE_ATTRIBUTE_CAPTUREDEF(集合类, 属性, 来源, 快照)`**：初始化它——三参数语义与第 4 篇 MMC 的捕获定义完全一致（AttributeToCapture/AttributeSource/bSnapshot）。
- **谁捕获谁**（本项目的攻防分工表）：

| 属性 | 来源 | 战斗语义 |
|---|---|---|
| Armor / Block_Chance / 四抗 / CriticalHitResistance | **Target** | 防守方数值 |
| Armor_Penetration / CriticalHitChance / CriticalHitDamage | **Source** | 进攻方数值 |

- **`static const DuraDamageStatics& DamageStatics()`（Meyers 单例）**：捕获定义构造要查反射（GetXXXAttribute），构造一次存起来复用；`static` 局部变量 C++11 保证线程安全初始化。**为什么不用构造函数直接填**：ExecCalc 是 UClass，其构造在 CDO 阶段；把静态数据放独立单例避免类构造时序问题，还方便 `DamageStatics().ArmorDef` 到处取用。
- **⚠️ 全部 `bSnapshot = false`**：伤害计算要**实时**的双方属性（加 Buff 立刻生效）。对比第 4 篇 MaxHealth 也是 false。快照（true）用于"伤害定型"场景——本项目的唯一"快照"语义在技能等级（SetByCaller 按等级查曲线）。

## 2.2 ExecCalc 构造函数

```cpp
UExecCalc_Damage::UExecCalc_Damage()
{
    RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
    ...（10 个全部声明）
}
```

- **`RelevantAttributesToCapture`**：告诉引擎"执行时要为我预备这些捕获数据"（Spec 创建时统一捕获）。**漏声明的后果**：Execute 里 AttemptCalculate 返回 false/0——静默错误。
- 与 MMC 不同点：ExecCalc 的捕获集是**类级**的（一个 ExecCalc 服务所有伤害类型），MMC 是**实例级**（一个 MMC 类捕获一组）。

---

# 三、Execute_Implementation —— 伤害公式引擎 ★★★★★（本篇核心）

## 3.1 前置准备段

```cpp
void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    FGameplayEffectContextHandle EffectContextHandle = Spec.GetContext();
```

- **参数一 `ExecutionParams`**：执行包——`GetOwningSpec()` 拿 GE 规格（SetByCaller 数据、捕获 Tags、Context 全在里面）；还有 `GetSourceAbilitySystemComponent()`/`GetTargetAbilitySystemComponent()` 快捷取双方 ASC。
- **参数二 `OutExecutionOutput`**：输出收集器——最终 `AddOutputModifier` 把算好的修改塞进去。
- **`const` 成员函数**：ExecCalc 是无状态计算器（状态都在 Spec/Context 里），线程安全。

```cpp
    TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition> TagsToCaptureDefs;
    TagsToCaptureDefs.Add(Tags.Attributes_Secondary_Armor, DamageStatics().ArmorDef);
    ...（10 个 Tag→捕获定义的映射）
```

- **为什么再建一张 Tag→Def 表**：抗性查询是"按 Tag 动态查"的（`DamageTypesToResistances` 给出抗性 Tag，要找它的捕获定义才能取值）。**第 1 篇的 Tag 表 + 这里的 Def 表 = 双跳查询链**：伤害 Tag →（Tag 表）→ 抗性 Tag →（Def 表）→ 捕获定义 → 属性值。
- `checkf(TagsToCaptureDefs.Contains(ResistanceTag), ...)`：防御断言——**Tag 表加了新抗性但这里忘同步**时立即崩溃并指名道姓。**checkf = check + 格式化消息**，比裸 check 的报错可定位性强得多。

```cpp
    int32 SourcePlayerLevel = 1;
    if(IsValid(SourceAvatar) && SourceAvatar->Implements<UCombatInterface>())
    {
        SourcePlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceAvatar);
    }
    // TargetPlayerLevel 同理
```

- 等级默认 1 + 接口查询——**默认值兜底**：没有 CombatInterface 的目标（比如打假人 Actor）按 1 级算，不崩。

```cpp
    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;
```

- `Spec.CapturedSourceTags`：GE 应用时**捕获的攻击者身上 Tag**（技能的 AssetTags+GrantedTags 聚合）。放进 EvaluationParameters 后，捕获的属性聚合会考虑"带 Tag 条件的 Modifier"（比如"灼烧状态下护甲-20"这种 GE）。

## 3.2 DetermineDebuff —— 减益判定（先于伤害主流程）

```cpp
void UExecCalc_Damage::DetermineDebuff(const FGameplayEffectSpec& Spec, 
    const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
    FAggregatorEvaluateParameters EvaluationParameters, 
    const TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition>& InTagsToDefs) const
{
    for( const TTuple<FGameplayTag, FGameplayTag>& Pair : GameplayTags.DamageTypesToDebuffs )
    {
        const FGameplayTag& DamageType = Pair.Key;
        const FGameplayTag& DebuffType = Pair.Value;
        const float TypeDamage = Spec.GetSetByCallerMagnitude(DamageType, false, -1.f);
        if( TypeDamage > -.5f ) // 0.5 的余量用于浮点精度
        {
            const float SourceDebuffChance = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Chance, false, -1.f);
            ...
        }
    }
}
```

- **`GetSetByCallerMagnitude(Tag, bWarnIfNotFound, DefaultValue)`**：从 Spec 取 SetByCaller 注入值。**第二参 false**：找不到别警告（这个 GE 可能根本没带该类型的伤害）；**第三参 -1**：找不到时的默认值——**用 -1 作为"未设置"哨兵**。
- **`TypeDamage > -.5f` 而不是 `> 0`**：伤害值可能是 0（比如物理火球不带火伤）但**有 SetByCaller 记录**；-1 才表示"没设置"。0.5 余量防浮点。**哨兵值与真实值的区分设计**——理解这个才能读懂"这段为什么不是 >0"。
- **如何识别"这颗弹带什么伤害类型"**：遍历四种类型，看 Spec 里哪个类型的 SetByCaller 有值（>-.5）。**数据自描述**——ApplyDamageEffect 只给 `DamageType` 一个键注入了 BaseDamage，这里反向扫描。

```cpp
            float TargetDebuffResistance = 0.f;
            const FGameplayTag ResistanceTag = GameplayTags.DamageTypesToResistances.FindRef(DamageType);
            if(!ResistanceTag.IsValid() || !InTagsToDefs.Contains(ResistanceTag)) continue;
            ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(InTagsToDefs[ResistanceTag], EvaluationParameters, TargetDebuffResistance);
            TargetDebuffResistance = FMath::Max<float>(TargetDebuffResistance, 0.f);

            const float EffectiveDebuffChance = SourceDebuffChance * (100 - TargetDebuffResistance) / 100.f;
            const bool bDebuff = FMath::RandRange(1, 100) <= EffectiveDebuffChance;
```

- **`AttemptCalculateCapturedAttributeMagnitude` vs 直接取**：这是 ExecutionParams 的方法（带 Tag 聚合上下文的重算），返回 bool 表示成功与否——**Attempt 前缀的 API 都返回成功标志**（失败常见原因：捕获集里没这个属性）。
- **Debuff 命中率公式**：`实际概率 = 攻击方DebuffChance × (100 - 目标对应抗性)/100`——**抗性双职能**：既减伤害又抵抗 Debuff（一种属性两个用途，数值策划视角）。
- **`FMath::RandRange(1, 100) <= Chance`**：闭区间 [1,100] 掷骰，Chance=15 → 15% 概率。注意 RandRange 两端都含——与 `% 100` 的 [0,99] 不同。

```cpp
            if( bDebuff )
            {
                FGameplayEffectContextHandle ContextHandle = Spec.GetContext();
                UDuraAbilitySystemLibrary::SetIsSuccessfulDebuff(ContextHandle, true);
                UDuraAbilitySystemLibrary::SetDamageType(ContextHandle, DamageType);
                const float DebuffDamage = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Damage, false, -1.f);
                ...Duration/Frequency 同理...
                UDuraAbilitySystemLibrary::SetDebuffDamage(ContextHandle, DebuffDamage);
                ...
            }
```

- **结果写进 Context**：掷骰结果 + Debuff 四参数全部存入 EffectContext（第 1 篇的网络复制载体）。**后续消费者**：AttributeSet::HandleIncomingDamage 读 IsSuccessfulDebuff 决定是否调 Debuff()；客户端表现层（DebuffNiagaraComponent）不用读这些（它只看 Tag）。
- **为什么在 ExecCalc 里写而不是攻击发起时写**：**能不能上 Debuff 取决于目标的抗性**——只有执行时（拿到目标属性）才知道结果。

## 3.3 主流程：类型伤害循环

```cpp
    float Damage = 0.0f;
    for (const TTuple<FGameplayTag, FGameplayTag>& Pair: GameplayTags.DamageTypesToResistances)
    {
        const FGameplayTag DamageTypeTag = Pair.Key;
        const FGameplayTag ResistanceTag = Pair.Value;
        checkf(TagsToCaptureDefs.Contains(ResistanceTag), ...);
        const FGameplayEffectAttributeCaptureDefinition& CaptureDef = TagsToCaptureDefs[ResistanceTag];
        
        float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
        if(DamageTypeValue <= 0.f) continue;    // 没带这种伤害类型，跳过

        float Resistance = 0.f;
        ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureDef, EvaluationParameters, Resistance);
        Resistance = FMath::Clamp(Resistance, 0.f, 100.f);

        DamageTypeValue *= (100.f - Resistance) / 100.f;
```

- **`GetSetByCallerMagnitude(DamageTypeTag, false)`**：注意这里**没有第三参**——默认值为 0（签名：`GetSetByCallerMagnitude(Tag, bool WarnIfNotFound=false, float Default=0)`），`<=0` 跳过。与 DetermineDebuff 用 -1 哨兵不同：这里"没设置"与"设置 0"语义等价（都不贡献伤害）——**哨兵值按需选择**。
- **抗性减伤公式**：`伤害 × (100-抗性)/100`。抗性 Clamp 到 [0,100]——**不允许负抗性**（负的会增伤，除非策划要"易伤"机制，那是另一个 Clamp 策略）。
- **多类型伤害叠加**：循环累加 Damage——一颗弹可以同时带火+物理（各查各的抗性）。

### 3.3.1 径向伤害分支（★★★★☆ 最精巧也最险的代码）

```cpp
        if(UDuraAbilitySystemLibrary::IsRadialDamage(EffectContextHandle))
        {
            // 1. 在 DuraCharacterBase 中重写 TakeDamage
            // 2. 创建委托 OnDamageDelegate，在 TakeDamage 中广播收到的伤害
            // 3. 在此处将 lambda 绑定到受害者的 OnDamageDelegate 上
            // 4. 调用 UGameplayStatics::ApplyRadialDamageWithFalloff 造成伤害
            //      （这会导致 TakeDamage 被调用）
            // 5. 在 lambda 中，将 DamageTypeValue 设为广播传回的所受伤害

            if(ICombatInterface* CombatInterface = Cast<ICombatInterface>(TargetAvatar))
            {
                const FDelegateHandle OnDamageHandle = CombatInterface->GetOnDamageDelegate().AddLambda([&DamageTypeValue](float DamageAmount)
                {
                    DamageTypeValue = DamageAmount;
                });

                UGameplayStatics::ApplyRadialDamageWithFalloff(
                    TargetAvatar, DamageTypeValue, 0.f,
                    UDuraAbilitySystemLibrary::GetRadialDamageOrigin(EffectContextHandle),
                    UDuraAbilitySystemLibrary::GetRadialDamageInnerRadius(EffectContextHandle),
                    UDuraAbilitySystemLibrary::GetRadialDamageOuterRadius(EffectContextHandle),
                    1.f,
                    UDamageType::StaticClass(),
                    TArray<AActor*>(),
                    SourceAvatar,
                    nullptr
                );

                CombatInterface->GetOnDamageDelegate().Remove(OnDamageHandle);
            }
        }
        Damage += DamageTypeValue;
```

- **问题背景**：FireBlast 的爆炸伤害要**按距离衰减**（内圈全额、外圈趋零）。GAS 的 ExecCalc 一次只算"一个目标"，而 `ApplyRadialDamageWithFalloff` 是**引擎的范围伤害**：自己找范围内所有可伤害对象、算距离衰减、逐个调 `TakeDamage`。
- **桥接方案（源码注释 5 步就是设计文档）**：借引擎的径向衰减算出"这个目标实际吃多少"，再把结果**通过委托回传**到本函数的局部变量——`ApplyRadialDamageWithFalloff` 内部调 TargetAvatar 的 `TakeDamage`（第 2 篇里重写的那份）→ TakeDamage 广播 OnDamageDelegate → 我们的 Lambda 接住实际衰减值写回 `DamageTypeValue`。调用返回后，`DamageTypeValue` 已经是衰减后数值，累加进 Damage。
- **参数逐个讲**：`TargetAvatar`（WorldContext）、`DamageTypeValue`（基础伤害）、`0.f`（最小伤害）、Origin/Inner/Outer（第 1 篇 Context 里的径向四件套）、`1.f`（衰减系数）、`UDamageType`（引擎伤害类型类）、忽略列表、`SourceAvatar`（Instigator）、`DamageCauser=nullptr`。
- **`[&DamageTypeValue]` 按引用捕获栈变量**：Lambda 跨函数栈帧引用局部变量——** Normally 是悬垂大忌**！这里安全是因为 ApplyRadialDamageWithFalloff 是**同步调用**（委托广播发生在函数返回前）。源码注释也点明了风险："径向伤害结算后立即移除绑定，否则受害者委托会一直持有指向本函数栈帧的悬空 lambda（内存破坏 + 多次释放时绑定累积）"——`Remove(OnDamageHandle)` 用**委托句柄精确摘除**（而不是 RemoveAll）。
- **★ 面试高分点**：讲这段要能说出"同步回调内合法、异步必悬垂；句柄式移除 vs RemoveAll 的区别"。
- **代价**：这次径向伤害**同时触发了传统 TakeDamage 管线**（会广播 OnDamageDelegate → 伤害数字组件显示一次数字！）——然后 GAS 的 Damage 累加再走 IncomingDamage 结算又飘一次数字。**实际行为**：TakeDamage 的广播发生在 ExecCalc 内部，目标是"借引擎的衰减计算"，数字显示会有两次来源（项目的 DamageTextComponent 有去重/生命周期管理，展示上是叠加数字）。学习时知道这条暗线即可。

## 3.4 格挡 → 护甲 → 暴击（三段修正）

```cpp
    // 格挡
    float TargetBlockChance = 0.0f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().Block_ChanceDef, EvaluationParameters, TargetBlockChance);
    TargetBlockChance = FMath::Max<float>(0.0f, TargetBlockChance);
    const bool bBlocked = FMath::RandRange(1, 100) <= TargetBlockChance;
    UDuraAbilitySystemLibrary::SetIsBlockedHit(EffectContextHandle, bBlocked);
    Damage = bBlocked ? Damage * 0.5f : Damage;
```

- 格挡流程：捕获目标格挡率 → 掷骰 → **结果写 Context**（客户端要显示"格挡"样式数字）→ 伤害减半。
- **写入 Context 的时机敏感**：必须在**伤害最终结算前**写——Context 会随 Spec 走到 AttributeSet/客户端。

```cpp
    // 护甲 + 护甲穿透（曲线表系数化）
    float TargetArmor = ...;         // 目标护甲
    float SourceArmorPenetration = ...;  // 攻击方穿透
    UCharacterClassInfo* CharacterClassInfo = UDuraAbilitySystemLibrary::GetCharacterClassInfo(SourceAvatar);
    if (CharacterClassInfo && CharacterClassInfo->DamageCalculationCoefficients)
    {
        const FRealCurve* ArmorPenetrationCurve = CharacterClassInfo->DamageCalculationCoefficients->FindCurve(FName("ArmorPenetration"), FString());
        const float ArmorPenetrationCoeff = ArmorPenetrationCurve->Eval(SourcePlayerLevel);
        const float EffectiveArmor = TargetArmor * (100 - SourceArmorPenetration * ArmorPenetrationCoeff) / 100.f;

        const FRealCurve* EffectiveArmorCurve = CharacterClassInfo->DamageCalculationCoefficients->FindCurve(FName("EffectiveArmor"), FString());
        const float EffectiveArmorCoeff = EffectiveArmorCurve->Eval(TargetPlayerLevel);
        Damage *= (100 - EffectiveArmor * EffectiveArmorCoeff) / 100.f;
    }
```

- **`FindCurve(曲线名, "")` + `Eval(等级)`**：曲线表（`UCurveTable`，编辑器里配的数值表）按**曲线行名**找行，按等级取值——**策划在表里调数值，程序员一行不改**。
- 双系数设计：穿透系数按**攻击方等级**查（我越强穿得越狠），减免系数按**目标等级**查（怪越硬减得越多）——**系数等级用"谁的属性谁定级"**。
- **`if (CharacterClassInfo && DamageCalculationCoefficients)`**：曲线表缺失时跳过护甲修正（裸伤）而不是崩——**数值表是可缺失的增强项**（与 Tag 表的 checkf 不同：Tag 是结构必需、曲线是数值增强）。
- 公式两步：`有效护甲 = 目标护甲 × (100 - 穿透×穿透系数)/100`；`伤害 ×= (100 - 有效护甲×减免系数)/100`。
- **⚠️ 潜在崩溃点**：`FindCurve` 找不到行返回 nullptr，下一行 `ArmorPenetrationCurve->Eval` 直接解引用——**表里没配 ArmorPenetration 行会崩**。与上面"表可缺失"的防御不一致，是审查发现的遗留隐患（学习时可作为"防御完备性"批判案例）。

```cpp
    // 暴击
    float EffectiveCriticalHitChance = SourceCritHitChance;
    if (CharacterClassInfo && CharacterClassInfo->DamageCalculationCoefficients)
    {
        const FRealCurve* CritHitResistanceCurve = ...FindCurve(FName("CriticalHitResistance"), ...);
        const float CritHitResistanceCoeff = CritHitResistanceCurve->Eval(TargetPlayerLevel);
        EffectiveCriticalHitChance = SourceCritHitChance - TargetCritHitResistence * CritHitResistanceCoeff;
    }
    const bool bCriticalHit = FMath::RandRange(1, 100) <= EffectiveCriticalHitChance;
    UDuraAbilitySystemLibrary::SetIsCriticalHit(EffectContextHandle, bCriticalHit);
    Damage = bCriticalHit ? Damage * 2 + SourceCritHitDamage : Damage;
```

- 暴击抗性按目标等级查系数后**直接从暴击率里扣**（注释说明"曲线表未配置时不做抵抗修正"——与护甲段不同，这里 FindCurve 判空了吗？`CritHitResistanceCurve->Eval` 同样没判空——同款隐患）。
- **暴击公式**：`伤害 ×2 + 暴击伤害加成`——2 倍是硬编码（可挪进曲线表）；CritHitDamage 是"额外加值"型（不是乘数型）。

```cpp
    const FGameplayModifierEvaluatedData EvaluatedData(UDuraAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage);
    OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
```

- **输出**：`FGameplayModifierEvaluatedData(目标属性, 运算, 数值)`——三参构造与 Modifier 面板一致。**Additive 到 IncomingDamage**：引擎把这条修改聚合成 IncomingDamage 的变化 → PostGameplayEffectExecute 触发（第 4 篇漏斗）。
- **至此公式全貌**：

```
基础伤害（SetByCaller，按技能等级查曲线）
  × (100-对应抗性)/100          ← 类型循环，可多类型累加
  × (径向距离衰减修正)            ← 径向分支（借引擎算）
  × (格挡 ? 0.5 : 1)            ← 掷骰
  × (100-有效护甲×系数)/100      ← 穿透后护甲，双等级曲线
  × 2 + 暴击伤害                 ← 掷骰
  = IncomingDamage
```

- **顺序的教学意义**：抗性→格挡→护甲→暴击的**乘法顺序不影响结果**（乘法交换律），但**加法穿插有影响**（暴击的 `+CritDamage` 若在护甲前，加值会被护甲减免——设计上"暴击伤害无视护甲加值"还是"跟着减免"是策划决策，当前实现是"先减免后暴击加值"）。

---

# 四、DuraAbilitySystemLibrary —— 静态工具箱全景 ★★★★★

## 4.1 为什么需要 Library

`UBlueprintFunctionLibrary` 子类 = **全静态函数集合**，蓝图与 C++ 双端可调。本项目的 Library 分五类：
1. **WidgetController 工厂**（第 8 篇详细讲）
2. **能力系统初始化**（属性/技能授予）
3. **Context 读写器**（30 个函数，成对）
4. **游戏机制查询**（范围/最近目标/敌我）
5. **伤害参数设置**（FDamageEffectParams 的链式配置器）

## 4.2 ApplyDamageEffect —— 伤害应用的唯一入口 ★★★★★

```cpp
FGameplayEffectContextHandle UDuraAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
    if(!DamageEffectParams.SourceAbilitySystemComponent || !DamageEffectParams.TargetAbilitySystemComponent)
    {
        return FGameplayEffectContextHandle();   // 空 Handle 表示未应用
    }

    const AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();

    FGameplayEffectContextHandle EffectContextHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
    EffectContextHandle.AddSourceObject(SourceAvatarActor);
    SetDeathImpulse(EffectContextHandle, DamageEffectParams.DeathImpulse);
    SetKnockbackForce(EffectContextHandle, DamageEffectParams.KnockbackForce);
    SetIsRadialDamage(EffectContextHandle, DamageEffectParams.bIsRadialDamage);
    SetRadialDamageOrigin(EffectContextHandle, DamageEffectParams.RadialDamageOrigin);
    ...径向两半径...

    const FGameplayEffectSpecHandle SpecHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeOutgoingSpec(
        DamageEffectParams.DamageGameplayEffectClass,
        DamageEffectParams.AbilityLevel,
        EffectContextHandle
    );
    if(!SpecHandle.IsValid()) return EffectContextHandle;

    //Set By Caller 赋值（6 个）
    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
    ...Debuff_Damage/Duration/Frequency...

    DamageEffectParams.TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

    return EffectContextHandle;
}
```

- **Param → Spec 的"搬运"全景**（这张映射表就是 Param 与 Context 的接口契约）：
  - **进 Context**（ExecCalc 的写答案区也要用，得提前带）：DeathImpulse、KnockbackForce、径向四件套。
  - **进 SetByCaller**（ExecCalc 的读数区）：`DamageType→BaseDamage`（**键就是伤害类型 Tag**！所以 DetermineDebuff 能反向扫描类型）、Debuff 四参数。
- **`AssignTagSetByCallerMagnitude(SpecHandle, Tag, Value)`**：蓝图库便捷函数（等价 `SpecHandle.Data->AddSetByCallerMagnitude`）。**注意它是 Tag 键**——GE 资产里的 SetByCaller 配置也要用 Tag 分支（旧版用 FName，Tag 是新规范）。
- **`ApplyGameplayEffectSpecToSelf`**：从 Source 的角度创建 Spec，**应用到 Target 自己**（对 Target ASC 调用）。为什么不是 `ToTarget`：`ToSelf` 的调用者是目标 ASC；Spec 已在手上，两者等价，Self 版少一次 Actor 查找。
- **返回 EffectContextHandle**：调用方（投射物）可继续用它（虽然本项目的投射物不用返回值——**返回值是扩展接口**）。
- **通俗解释**：这是"伤害快递发货台"——把 DamageEffectParams 包裹单（第 1 篇）拆成两联：随行证联（Context，给结算方）+ 货运单联（SetByCaller，给计算器），统一打包进 Spec 发往目标。

## 4.3 Context 读写器模式（30 个函数的"批量复制"艺术）

```cpp
bool UDuraAbilitySystemLibrary::IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
    if (const FDuraGameplayEffectContext* DuraEffectContext = static_cast<const FDuraGameplayEffectContext*>(EffectContextHandle.Get()))
    {
        return DuraEffectContext->IsBlockedHit();
    }
    return false;
}

void UDuraAbilitySystemLibrary::SetIsBlockedHit(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit)
{
    if (FDuraGameplayEffectContext* DuraEffectContext = static_cast<FDuraGameplayEffectContext*>(EffectContextHandle.Get()))
    {
        DuraEffectContext->SetIsBlockedHit(bInIsBlockedHit);
    }
}
```

- **`EffectContextHandle.Get()`**：Handle 内部 TSharedPtr 的裸指针；`static_cast` 到子类（**非 UObject 的转型只能 C++ cast**——第 4 篇提过）。
- **为什么判空**：Handle 可能空（默认构造）或指向基类 Context（没有走我们 Globals 的路径）——static_cast 是强转**不做类型检查**，指向错误类型时调用会 UB；这里的 `if (ptr)` 只防空不防型。**更严谨**是 `dynamic_cast`（有 RTTI 成本）——项目选择性能优先+约定保证（所有 Context 都是 FDura 版）。
- **`UPARAM(ref)`**：UHT 元数据——**蓝图里按引用传参**（蓝图默认按值复制参数！不标记 ref 的话，蓝图里 Set 到的是副本，原 Context 不变——**蓝图函数库必考坑**）。Getter 用 `BlueprintPure`（无执行引脚），Setter 用 `BlueprintCallable`（有执行引脚）——**Pure 函数每次执行都重算且无副作用，Setter 有副作用必须 Callable**。

## 4.4 初始化家族

### InitializeDefaultAttributes（按职业）

```cpp
void UDuraAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, 
    ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
    UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
    if(!CharacterClassInfo || !ASC) return;
    FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
    ...
    // Primary → Secondary → Vital 三连（GE 四步曲 ×3）
}
```

- 与第 2 篇基类版本的差异：**GE 类从 CharacterClassInfo 按职业取**（Warrior 的主属性 GE 和 Elementalist 的不同）——数据驱动职业差异。这就是为什么 DuraEnemy 覆写 InitializeDefaultAttributes 调这里，而玩家（有存档需求）另有路径。
- **`AddSourceObject(AvatarActor)`** 每次都加——第 4 篇 MMC 的等级查询靠它！**链条闭环**：初始化时塞 SourceObject → MMC 才能查到等级。

### InitializeDefaultAttributesFromSaveData（SetByCaller 版本）

```cpp
    const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
        CharacterClassInfo->PrimaryAttributesSetByCaller, 1.f, EffectContextHandle
    );
    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Strength, SaveGame->Strength);
    ...四维各一行...
```

- **读档专用 GE**：`PrimaryAttributesSetByCaller` 是另一个 GE 资产——它的四个 Modifier 全配成 SetByCaller-Tag 模式（Strength/Intelligence/Resilience/Vigor 四个 Tag 键）。**同一套 GE 管线，数值来源换成存档**。等级传 1.f（SetByCaller 数值不按等级缩放）。
- 对比：正常初始化用职业 GE（MMC 算）——**读档恢复的是"四维定值"，派生树重算**（第 2 篇讲过"存根不存叶"）。
- `SecondaryAttributes_Infinite`：**无限时长版**次要属性 GE（存档恢复后要永久存在，`DurationPolicy=Infinite`）。

### GiveStartupAbilities（技能授予）

```cpp
void UDuraAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
    UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
    if (!CharacterClassInfo) return;

    for (const auto& AbilityClass : CharacterClassInfo->CommonAbilities)
    {
        FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
        ASC->GiveAbility(AbilitySpec);
    }

    const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
    if(ASC->GetAvatarActor()->Implements<UCombatInterface>())
    {
        int32 PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor());
        for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
        {
            FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, PlayerLevel);
            ASC->GiveAbility(AbilitySpec);
        }
    }
}
```

- **两层技能**：`CommonAbilities`（全员通用，1 级，如 HitReact 受击反应）+ `StartupAbilities`（职业专属，**按角色当前等级**授予——敌人 5 级出生，它的攻击技能就是 5 级的（伤害曲线按等级放大））。
- **`FGameplayAbilitySpec(AbilityClass, Level)`**：Spec = 技能类+等级的"实例配置"（还有 SourceObject/动态句柄等，第 6 篇）。`GiveAbility` **只能服务器调**（内部有检查）。
- **玩家的技能不走这里**：玩家在蓝图配 StartupAbilities 数组（第 2 篇 AddCharacterAbilities）——**玩家技能跟随蓝图资产，敌人技能跟随职业数据表**，两种组织方式的对照。

### GetXPRewardForClassAndLevel

```cpp
    const FCharacterClassDefaultInfo& Info = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
    const float XPReward = Info.XPReward.GetValueAtLevel(CharacterLevel);
    return static_cast<int32>(XPReward);
```

- `XPReward` 是 `FScalableFloat`——**可按等级缩放的浮点**（`GetValueAtLevel` 查它关联的曲线，没曲线就是常数）。第 4 篇 SendXPEvent 的数据源头。
- `static_cast<int32>` 显式截断（float→int）。

## 4.5 游戏机制查询

### GetLivePlayersWithinRadius

```cpp
void UDuraAbilitySystemLibrary::GetLivePlayersWithinRadius(const UObject* WorldContextObject, 
    TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereLocation)
{
    OutOverlappingActors.Reset();

    FCollisionQueryParams SphereParams;
    SphereParams.AddIgnoredActors(ActorsToIgnore);

    TArray<FOverlapResult> Overlaps;
    if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->OverlapMultiByObjectType(Overlaps, SphereLocation, FQuat::Identity, 
            FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), 
            FCollisionShape::MakeSphere(Radius), SphereParams);
        ...
```

- **`GetWorldFromContextObject(对象, LogAndReturnNull)`**：静态函数没有 GetWorld()（不是 Actor），从 WorldContext 反查——静态函数族的标准开头。`LogAndReturnNull`：拿不到就记日志返回空（不崩）。
- **`OverlapMultiByObjectType`**：按**对象类型**（AllDynamicObjects = 所有动态物理对象：Pawn/物理体）而不是通道查询——**范围查询用 ObjectType（宽筛），后续用接口（细筛）**：`Implements<UCombatInterface>`（是战斗单位）→ `IsDead` 排除尸体 → `GetAvatar`（**注意**：取的是接口的 Avatar 而非 Overlap 命中的 Actor——命中"玩家的武器组件"时归一到玩家本体）→ `AddUnique` 去重。
- **消费者**：召唤技能（DuraSummonAbility 召点）、ArcaneShards（取目标位置）、Electrocute（链电目标池）。

### GetClosestTargets（贪心选择）

```cpp
void UDuraAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, const FVector& Origin, TArray<AActor*>& OutClosestTargets)
{
    if(Actors.Num() <= MaxTargets)
    {
        OutClosestTargets = Actors;   // 不超量直接全给
        return;
    }
    while(NumTargetsFound < MaxTargets)
    {
        // 线性找最近 → 移除 → 计数
        ActorsToCheck.Remove(ClosestActor);
        OutClosestTargets.AddUnique(ClosestActor);
        NumTargetsFound++;
    }
}
```

- **贪心算法**：循环 MaxTargets 次，每次 O(N) 找最近——O(K×N)，K 小 N 小，够用。**为什么不排序取前 K**（O(N logN)）：K 通常 1~3，贪心更快且代码直观。**算法选择的实用主义**。
- `Remove`（按值移除第一个匹配）—— Actor 指针按值比较。`AddUnique` 防重复（Actor 数组理论无重复，防御式）。

### IsNotFriend

```cpp
bool UDuraAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
    if(!FirstActor || !SecondActor) return true;   // 失效按敌对处理，弹道不崩

    const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
    const bool bBothAreEnemies = ...;
    return !bFriends;
}
```

- **用 ActorTag（不是 GameplayTag！）**：`ActorHasTag(FName("Player"))` 是 Actor 层的字符串标签（BP_DuraCharacter 的默认 Tags 里加了 "Player"，BP_DuraEnemy 加了 "Enemy"）。**两层 Tag 体系的分工**：ActorTag 管"阵营归属"（超轻量字符串），GameplayTag 管"游戏状态"（重系统）。**别混用**——这里用 ActorTag 因为投射物需要超轻量的阵营判断（没有 ASC 的 actor 也可能要判断）。
- **空指针按敌对**：弹道碰撞回调里对方可能已被销毁——**失效者当敌人处理**让弹丸继续飞行/爆炸，不中断表现。

### EvenlySpacedRotators / EvenlyRotatedVectors（几何工具）

```cpp
TArray<FRotator> UDuraAbilitySystemLibrary::EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, 
    float Spread, int32 NumRotators)
{
    if(NumRotators > 1)
    {
        const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);   // 从扇区最左开始
        const float DeltaSpread = Spread / (NumRotators - 1);                        // 均分间隔
        for (int32 i = 0; i < NumRotators; i++)
        {
            const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
            Rotators.Add(Direction.Rotation());
        }
    }
    else
    {
        Rotators.Add(Forward.Rotation());   // 单发 = 正前方
    }
    return Rotators;
}
```

- **扇形均布算法**：N 发弹在 Spread 度的扇形里均匀排开——从"最左侧"（Forward 绕轴转 -Spread/2）开始，每发加 `Spread/(N-1)` 度。**N=1 特判**：除零保护（`(N-1)` 当分母）。
- **`RotateAngleAxis(角度, 轴)`**：绕轴旋转（罗德里格斯旋转的向量版）。Axis 通常是世界 Z（水平扇形）或角色前向（垂直扇形）。
- **消费者**：FireBolt 多段发射（等级越高弹数越多、扇形越大）、FireBlast 的弹幕。**EvenlyRotatedVectors 是同函数的向量版**（返回 FVector 不转 FRotator——直接给 Spawn 方向用）。
- **通俗解释**：像开伞——伞骨均匀张开，中心对准目标，等级越高伞越大骨越多。

---

# 五、本章全链路总图（一次火球全程）

```
玩家按下 LMB（第 3 篇）→ GA_FireBolt 激活（第 6 篇）→ 生成 BP_FireBolt 投射物（第 12 篇）
    │ 服务器命中回调
    ▼
构造 FDamageEffectParams（BaseDamage=曲线(等级), DamageType=Fire, DebuffChance=15...）
    │ AbilityLibrary::SetKnockbackDirection / SetDeathImpulseDirection 预处理
    ▼
ApplyDamageEffect(params)                          ← Library
    ├─ MakeEffectContext + AddSourceObject + 死冲/击退/径向 写入 Context
    ├─ MakeOutgoingSpec(GE_Damage, AbilityLevel, Context)
    ├─ AssignTagSetByCallerMagnitude ×6（DamageType→伤害值；Debuff×4）
    └─ TargetASC->ApplyGameplayEffectSpecToSelf
    ▼
GE_Damage 的 Executor：ExecCalc_Damage::Execute      ← 本篇
    ├─ 捕获双方 10 属性（实时）
    ├─ DetermineDebuff：扫描类型 → 抗性修正概率 → 掷骰 → 结果写 Context
    ├─ 类型循环：抗性减伤 →（径向分支：借引擎 TakeDamage 回传衰减值）
    ├─ 格挡掷骰 ×0.5（写 Context）
    ├─ 护甲：穿透曲线系数 × 目标等级减免曲线
    └─ 暴击掷骰 ×2+加成（写 Context）→ 输出 Additive 到 IncomingDamage
    ▼
AttributeSet::PostGameplayEffectExecute（第 4 篇漏斗）→ 扣血/死亡/受击/飘字/Debuff
```

# 六、动手实验建议

1. 在 ExecCalc 的每段修正处打 `UE_LOG(LogDura, Log, TEXT("阶段=%s 伤害=%f"), ...)`，PIE 打一发火球，逐阶段观察数值演变——公式黑盒变透明。
2. 把护甲段的 `FindCurve` 返回值判空补上（`if(ArmorPenetrationCurve)`），再故意删掉曲线表里该行——对比崩溃与降级。
3. 修改 `EvenlySpacedRotators` 的 `NumRotators=1` 分支让它返回空数组，发射单发 FireBolt——观察技能静默失败，理解"特判分支"的保护意义。

---

*下一篇：`06-技能释放全链路.md` —— DuraAbilitySystemComponent 的输入转发/技能授予/冷却监听、GameplayAbility 基类、TargetDataUnderMouse 异步任务与全部技能族。*
