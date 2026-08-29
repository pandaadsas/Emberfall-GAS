# 04 · 属性系统全链路 —— AttributeSet 的属性流水线与派生计算

> **逻辑链路一句话概括**：
> `初始化 GE（Primary/Secondary/Vital）把数值写进属性` → `伤害 GE 把数值写进"元属性 IncomingDamage"（而不是直接扣血）` → `PostGameplayEffectExecute 是结算漏斗：扣血、判死、触发受击/击退/Debuff、发 XP 事件、飘伤害数字` → `PreAttributeChange 在写入前 Clamp` → `属性变化/OnRep 复制到客户端 → WidgetController 委托刷新 UI（第 8 篇）`。
> **本篇是 GAS 面试出题密度最高的一个文件，务必吃透每一个回调的时机差异。**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/AbilitySystem/DuraAttributeSet.h` | 属性声明、访问器宏、FEffectProperties、函数指针表 | ★★★★★ |
| 2 | `Source/Dura/Private/AbilitySystem/DuraAttributeSet.cpp` | 结算漏斗全逻辑（本篇核心） | ★★★★★ |
| 3 | `Source/Dura/Public/AbilitySystem/ModMagCalc/MMC_MaxHealth.h` / `.cpp` | MaxHealth = 80 + 2.5×Vigor + 10×Level | ★★★★☆ |
| 4 | `Source/Dura/Public/AbilitySystem/ModMagCalc/MMC_MaxMana.h` / `.cpp` | MaxMana = 50 + 2×Intelligence + 10×Level | ★★★☆☆ |

配套资产（蓝图，供对照）：`GE_DuraPrimaryAttribute` / `GE_DuraPrimaryAttribute_SetByCaller` / `GE_SecondaryAttribute` / `GE_VitalAttribute` / `GE_Damage`。

---

# 一、属性声明的"模板代码"解剖 ★★★★★

## 1.1 一个属性的完整四件套

```cpp
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category="Vital Attributes")
FGameplayAttributeData Health;
ATTRIBUTE_ACCESSORS(UDuraAttributeSet, Health);

UFUNCTION()
void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

// GetLifetimeReplicatedProps 里：
DOREPLIFETIME_CONDITION_NOTIFY(UDuraAttributeSet, Health, COND_None, REPNOTIFY_Always);
```

**为什么一个属性要写四处**——逐个拆：

1. **`FGameplayAttributeData`**（不是 float！）：内含 `BaseValue`（基础值）与 `CurrentValue`（当前值）。**为什么要两个值**：GE 修改的是 CurrentValue 的聚合结果；Duration GE 过期后，引擎用 BaseValue 重建 CurrentValue（比如 +50 血的 Buff 到期，血自动回落）。`GetHealth()` 返回 CurrentValue，`SetHealth()` 同时写 Base+Current（永久修改）。
2. **`UPROPERTY(BlueprintReadOnly, ReplicatedUsing=...)`**：
   - `BlueprintReadOnly`：蓝图只能读（写必须走 GE，**保证数值变更全走 GAS 管线**——这是 GAS 的纪律性设计；蓝图乱 Set 血量会绕过事件与复制）。
   - 属性复制要求 UPROPERTY 反射可见，`ReplicatedUsing` 指定客户端回调。
3. **`ATTRIBUTE_ACCESSORS` 宏展开**（把宏一行行翻译）：

```cpp
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \   // static FGameplayAttribute GetXxxAttribute();
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \                 // float GetXxx() const;
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \                 // void SetXxx(float NewVal);
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)                  // void InitXxx(float NewVal);
```

- **`GetHealthAttribute()`**：返回 `FGameplayAttribute`——**属性的"反射句柄"**（内部是 UClass + 属性指针），用于：GE 修改器定位属性、`GetGameplayAttributeValueChangeDelegate(句柄)` 监听、`GetNumericValue(句柄)` 通用读值。**这是"属性作为一等公民"的关键**：属性不再只是数据，而是可传递、可比较、可注册的对象。
- **`GetHealth()`**：读当前值（全项目最常用）。
- **`SetHealth(v)`**：永久设置（写 Base+Current，触发属性变化委托、触发复制）。
- **`InitHealth(v)`**：初始化（绕过 Modifier 聚合直接写 Base+Current，不触发 GE 流程）——用于"出生给初值"，区别于 Set（Set 用于运行时校正）。
- **宏的本质**：28 个属性 × 4 行样板 = 一百多行重复代码，宏压缩成一行一个。**这是 GAS 项目的标准宏**（官方文档推荐写法），面试写出它 = 熟悉 GAS。

4. **`DOREPLIFETIME_CONDITION_NOTIFY(类, 属性, COND_None, REPNOTIFY_Always)`**（★★★★★ 高频考点）：
   - `COND_None`：不做条件过滤（人人可收；可用 `COND_OwnerOnly` 只有拥有者收——适合暴击率这类隐私数据）。
   - **`REPNOTIFY_Always` vs 默认（REPNOTIFY_OnChanged）**：默认只在**值变化**时调 OnRep；**Always 只要复制就调**（哪怕值没变）。**为什么属性必须 Always**：读档后血量恰好等于旧值（比如都是 100.0），OnChanged 不触发 → UI 血条不刷新；Always 保证"重新赋值也通知"。这是 GAS 网络同步的著名坑。
   - `GetLifetimeReplicatedProps` 的**遗漏症状**：客户端永远收不到该属性（本地改不生效）——新手最常见"客户端血条不动"的原因。

## 1.2 OnRep 的标准体

```cpp
void UDuraAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UDuraAttributeSet, Health, OldHealth);
}
```

- 宏展开 ≈ `ATTRIBUTE_LISTENER(Health); CHECK_ATTRIBUTE_IMPL(Health, OldValue);` ——**手动触发属性聚合监听**（等价于本地 SetHealth 时引擎自动做的事）。没有它，**客户端收到的属性变化不会触发 `GetGameplayAttributeValueChangeDelegate` 委托**——UI 委托静默失效。又一个"客户端 UI 不动"的经典病因。
- `const` 成员函数：OnRep 只是通知，不改状态。

## 1.3 TagsToAttributes —— 属性的函数指针注册表 ★★★★☆

```cpp
// 头文件
template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;

TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes;

// 构造函数
TagsToAttributes.Add(GameplayTags.Attributes_Primary_Strength, GetStrengthAttribute);
TagsToAttributes.Add(GameplayTags.Attributes_Secondary_Armor, GetArmorAttribute);
...（18 个属性全部注册）
```

- **`TStaticFuncPtr<FGameplayAttribute()>`**：**无捕获静态函数指针**的类型别名。注释里那句 "typedef 特指 FGameplayAttribute() 签名，而 TStaticFunPtr 对所选的任意签名都是通用的"——这是把引擎内部（Delegate 模块的 `FFuncPtr`）抽成可复用别名的现代 C++ 手法（`using` + `typename`）。
- **存的是"属性 Getter 函数指针"**：`GetStrengthAttribute` 是 `static` 函数，取函数指针不需要对象。构造函数里就能填表（CDO 阶段 Tag 已就绪）。
- **谁消费这张表**：`AttributeMenuWidgetController` / `DuraOverlayWidgetController`（第 8 篇）——UI 要监听"某个 Tag 对应的属性"变化时，`TagsToAttributes[Tag]()` 拿到 FGameplayAttribute 句柄再注册委托。**Tag ↔ 属性 的桥**，让 UI 配置里只写 Tag 名就能自动监听对应属性。
- **面试语言点**：`TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr` 是 UE 委托模板的底层函数指针类型；用 `typename` 是因为它是依赖类型（dependent name）。

---

# 二、FEffectProperties —— 结算上下文的"八件套" ★★★★★

```cpp
USTRUCT()
struct FEffectProperties
{
    FGameplayEffectContextHandle EffectContextHandle;
    UAbilitySystemComponent* SourceASC;
    AActor* SourceAvatarActor;
    AController* SourceController;
    ACharacter* SourceCharacter;
    UAbilitySystemComponent* TargetASC;    // 目标四件同理
    AActor* TargetAvatarActor;
    AController* TargetController;
    ACharacter* TargetCharacter;
};
```

- **为什么需要它**：PostGameplayEffectExecute 只直接拿到 `Data.Target`（自己），**攻击者是谁要费一番周折**（走 EffectContext 的 Instigator 链）。把这些解析结果打包，后面 HandleIncomingDamage/Debuff/XP 全部直接用。
- **Source 四层链**：ASC → AvatarActor → Controller → Character。**为什么 Controller 还要二次兜底**（见下文 SetEffectProperties）：AbilityActorInfo 的 PlayerController 对**敌人**是空的（敌人没有 PlayerController），要手动 `Pawn->GetController()` 补（AIController 也算 Controller）。Character 再从 Controller 取 Pawn——**保证是"当前的身体"而不是 Possess 时的旧引用**。

---

# 三、PostGameplayEffectExecute —— 一切结算的漏斗口 ★★★★★

## 3.1 时机（必须先讲清楚）

GE 应用 → 修改器聚合 → **属性值实际改变** → `PostGameplayEffectExecute(Data)`。**只对"直接修改属性"的 GE 触发一次**（Period GE 每周期一次）。`Data.EvaluatedData.Attribute` 告诉你"这次改的是哪个属性"，`Data.EffectSpec` 带着完整上下文。

## 3.2 函数体逐段精讲

```cpp
void UDuraAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    FEffectProperties Props;
    SetEffectProperties(Data, Props);

    if(IsValid(Props.TargetAvatarActor) && Props.TargetAvatarActor->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Props.TargetAvatarActor))
    {
        return;
    }
```

- **死人免结算**：三连判断（有效 → 实现战斗接口 → 已死）后直接 return。**为什么必要**：Debuff GE 是周期性的（每 1 秒跳一次伤害）——目标在灼烧期间死亡，下一跳不能触发"死亡处理/受击/掉落"链（否则双倍掉落、重复 Die 崩溃）。**周期 GE 的幂等性守卫**。
- 判断用 `ICombatInterface::Execute_IsDead(...)`（接口调用）而不是直接 Cast 拿 bDead——**保持接口解耦**，未来非 Dura 类的目标也能正确结算。

```cpp
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    if (Data.EvaluatedData.Attribute == GetManaAttribute())
    {
        SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
    }
```

- **属性 == 句柄的比较**：`FGameplayAttribute::operator==` 比较反射指针。`GetHealthAttribute()` 每次生成新句柄但内部指针相等——标准写法。
- **直接改 Health/Mana 的 GE（比如回血药水）在这里兜底 Clamp**。注意这里的 Clamp 是**结算后补刀**（Post），真正的"事前约束"在 PreAttributeChange（见第 5 节）。

```cpp
    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        HandleIncomingDamage(Props);
    }
    if (Data.EvaluatedData.Attribute == GetIncomingXPAttribute())
    {
        HandleIncomingXP(Props);  
    }
```

- **元属性消费点**：伤害 GE 的 Modifier 写的是 **IncomingDamage**（不是 Health！）——见第 4 节设计动机。

## 3.3 SetEffectProperties —— 攻击者解析链 ★★★★★

```cpp
void UDuraAttributeSet::SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const
{
    Props.EffectContextHandle = Data.EffectSpec.GetContext();
    Props.SourceASC = Props.EffectContextHandle.GetOriginalInstigatorAbilitySystemComponent();

    if (IsValid(Props.SourceASC) && Props.SourceASC->AbilityActorInfo.IsValid() && Props.SourceASC->AbilityActorInfo->AvatarActor.IsValid())
    {
        Props.SourceAvatarActor = Props.SourceASC->AbilityActorInfo->AvatarActor.Get();
        Props.SourceController = Props.SourceASC->AbilityActorInfo->PlayerController.Get();
        if (Props.SourceController == nullptr && Props.SourceAvatarActor != nullptr)
        {
            if (const APawn* Pawn = Cast<APawn>(Props.SourceAvatarActor))
            {
                Props.SourceController = Pawn->GetController();
            }
        }
        if (Props.SourceController)
        {
            Props.SourceCharacter = Cast<ACharacter>(Props.SourceController->GetPawn());
        }
    }

    if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
    {
        Props.TargetAvatarActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
        Props.TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
        Props.TargetCharacter = Cast<ACharacter>(Props.TargetAvatarActor);
        Props.TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Props.TargetAvatarActor);
    }
}
```

- **`GetOriginalInstigatorAbilitySystemComponent()`**：Context 里存的 Instigator ASC（第 1 篇 NetSerialize 收尾的 `AddInstigator` 初始化的字段）。**"Original"** 的含义：多重 GE 链（GE 触发 GE）时，最原始的施加者——XP 归属要认"第一凶手"。
- **`AbilityActorInfo`**：ASC 的 Actor 信息包（第 2 篇 InitAbilityActorInfo 构建）。`AvatarActor.Get()` 从弱指针取裸指针（**先 IsValid 再 Get**，弱指针可能已失效——空指针防御三段式）。
- **Controller 的双重解析**：玩家从 ActorInfo 直接取；敌人走 Pawn->GetController() 拿 AIController。**统一抽象**：不管攻击者是玩家还是 AI，SourceController/SourceCharacter 都有值。
- **Target 侧为什么也要拿一遍**：`Data.Target` 虽然就是本 AttributeSet 的 Owner，但接口统一、代码对称——Props 里目标四件也备齐。

---

# 四、HandleIncomingDamage —— 伤害结算全流程 ★★★★★（本篇心脏）

```cpp
void UDuraAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
    const float LocalIncomingDamage = GetIncomingDamage();
    SetIncomingDamage(0.0f);                    // ① 先清零（防重复结算）
    if (LocalIncomingDamage > 0.0f)
    {
        const float NewHealth = GetHealth() - LocalIncomingDamage;
        SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));   // ② 扣血

        const bool bFatal = NewHealth <= 0.0f;   // ③ 死亡判定（用未 Clamp 的值！）
        if (bFatal)
        {
            if (ICombatInterface* Combat = Cast<ICombatInterface>(Props.TargetAvatarActor))
            {
                FVector Impulse = UDuraAbilitySystemLibrary::GetDeathImpulse(Props.EffectContextHandle);
                Combat->Die(Impulse);            // ④ 致死：带死亡冲量调用 Die
            }
            SendXPEvent(Props);                  // ⑤ 给攻击者发 XP 事件
        }
        else
        {
            // ⑥ 未死：受击反应（被电击中时跳过，避免打断麻痹动画）
            if(IsValid(Props.TargetCharacter) && Props.TargetASC && Props.TargetCharacter->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsBeingShocked(Props.TargetCharacter))
            {
                FGameplayTagContainer TagContainer;
                TagContainer.AddTag(FDuraGameplayTags::Get().Effect_HitReact);
                Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
            }

            // ⑦ 击退
            const FVector& KnockbackForce = UDuraAbilitySystemLibrary::GetKnockbackForce(Props.EffectContextHandle); 
            if(IsValid(Props.TargetCharacter) && !KnockbackForce.IsNearlyZero(1.f))
            {
                Props.TargetCharacter->LaunchCharacter(KnockbackForce, true, true);
            }
        }

        // ⑧ 飘伤害数字
        const bool bBlock = UDuraAbilitySystemLibrary::IsBlockedHit(Props.EffectContextHandle);
        const bool bCriticalHit = UDuraAbilitySystemLibrary::IsCriticalHit(Props.EffectContextHandle);
        ShowFloatingText(Props, LocalIncomingDamage, bBlock, bCriticalHit);

        // ⑨ Debuff 处理
        if(UDuraAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle) && 
          Props.TargetCharacter != Props.SourceCharacter)
        {
            Debuff(Props);
        }
    }
}
```

逐点精讲：

- **① 先取后清**：`SetIncomingDamage(0.0f)`——元属性是**一次性信箱**。不清零的后果：下一个无关 GE 触发 PostGameplayEffectExecute 时（哪怕改的是别的属性），只要再查 IncomingDamage 还是旧伤害值（本次函数不重入其实没事——但若同一帧多个 GE 各写一次 IncomingDamage，每次进 Handle 都该只消费自己的那笔）。**信箱模式：取件后必须清箱**。
- **③ `bFatal = NewHealth <= 0.0f` 用未 Clamp 的值**：Clamp 后 Health 最小是 0，无法区分"刚好 0"与"穿到负数"；**先存原始差值再 Clamp** 才能正确判定死亡。顺序敏感的代码——调换两行就是 bug。
- **④ 死亡冲量**：`GetDeathImpulse(Context)` 从 Context 取（ExecCalc 写入的、考虑了攻击方向的力向量）。`Die(Impulse)` 让尸体朝被击方向飞——**战斗手感细节**：背后偷袭致死的尸体向前扑倒。
- **⑤ SendXPEvent**（见 4.1 节）在死亡分支里：**杀人奖励**。
- **⑥ 受击反应走技能**：`TryActivateAbilitiesByTag({Effect_HitReact})`——受击硬直不是直接播蒙太奇，而是**激活一个 Tag 为 Abilities.HitReact 的技能**（GA_HitReact，蓝图里播蒙太奇+GE）。**为什么绕道技能系统**：(a) 技能有 PlayMontageAndWait 的完整生命周期（打断/结束回调）；(b) 技能可以被 Block/Cancel（被电击时不想触发受击——外面的 IsBeingShocked 判断正是此意）；(c) 统一走 ASC 的激活规则（权限/并发/作用域）。
- **⑦ `LaunchCharacter(力, XYOverride=true, ZOverride=true)`**：CharacterMovement 的击飞 API，true/true 表示完全覆盖 XY/Z 速度。`IsNearlyZero(1.f)` 容差 1cm/s 以下不触发（浮点噪声防御）。**击退不走路由**：直接对 Character 施加位移——Layered：GAS 管"数值"，位移这种瞬时物理表现直接引擎 API（分工明确）。
- **⑧ 飘字与暴击标志**：`IsBlockedHit/IsCriticalHit(Context)`——**Lib 的静态包装函数**（内部 Cast Context 后调 IsBlockedHit()）。为什么经 Lib 而不直接 Cast：**集中转换逻辑 + 蓝图也要用**（Lib 的函数多是 BlueprintCallable 静态）。
- **⑨ Debuff 双守卫**：掷骰成功（IsSuccessfulDebuff）且 **不是自伤**（TargetCharacter != SourceCharacter）——火球炸自己不给自己上火。

### 4.1 SendXPEvent —— 把"奖励"还给攻击者

```cpp
void UDuraAttributeSet::SendXPEvent(const FEffectProperties& Props)
{
    if(IsValid(Props.TargetCharacter) && Props.TargetCharacter->Implements<UCombatInterface>())
    {
        const int32 TargetLevel = ICombatInterface::Execute_GetPlayerLevel(Props.TargetCharacter);
        const ECharacterClass TargetClass = ICombatInterface::Execute_GetCharacterClass(Props.TargetCharacter);
        const int32 Reward = UDuraAbilitySystemLibrary::GetXPRewardForClassAndLevel(Props.TargetCharacter, TargetClass, TargetLevel);

        FGameplayEventData Payload;
        Payload.EventTag = GameplayTags.Attributes_Meta_IncomingXP;
        Payload.EventMagnitude = Reward;

        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Props.SourceCharacter, 
            GameplayTags.Attributes_Meta_IncomingXP, Payload);
    }
}
```

- **GameplayEvent（事件）与 Tag（状态）的区别（★★★★★ 面试考点）**：Tag 是**持续状态**（挂身上）；Event 是**瞬时消息**（带 Payload，路由到监听的技能）。`SendGameplayEventToActor(Actor, Tag, Payload)` 会触发 Actor ASC 上**所有监听该 Tag 的已激活技能**（`GA_ListenForEvent` 的 WaitGameplayEvent 任务）与 AbilityTriggers。
- **FGameplayEventData**：事件包裹（EventTag、EventMagnitude 数值、TargetData、OptionalObject 等）。这里用 EventMagnitude 携带 XP 数值。
- **链路**：敌人死亡 → Payload 按敌人等级/职业查表算 Reward → 发事件给**攻击者** → 攻击者身上的 `GA_ListenForEvent`（蓝图技能，常驻）收到事件 → 应用 `GE_EventBasedEffect`（把 EventMagnitude 加到 **IncomingXP 元属性**）→ PostGameplayEffectExecute → HandleIncomingXP。**伤害给 XP 绕了一大圈（Post → Event → 技能 → GE → Post）**——为什么？因为**只有 GE 才能触发 PostGameplayEffectExecute**，想复用同一套元属性结算管线就必须回到 GE。设计的一致性 > 路径最短。
- **谁被排除**：`Props.TargetCharacter`（死者）必须实现 CombatInterface；检查的是**死者**的等级/职业——奖励按"猎物"定价。

### 4.2 HandleIncomingXP —— 升级结算

```cpp
void UDuraAttributeSet::HandleIncomingXP(const FEffectProperties& Props)
{
    const float LocalIncomingXP = GetIncomingXP();
    SetIncomingXP(0.f);   // 同样：信箱清零

    if(IsValid(Props.SourceCharacter) && Props.SourceCharacter->Implements<UCombatInterface>() && Props.SourceCharacter->Implements<UPlayerInterface>())
    {
        const int32 CurrentLevel = ICombatInterface::Execute_GetPlayerLevel(Props.SourceCharacter);
        const int32 CurrentXP = IPlayerInterface::Execute_GetXP(Props.SourceCharacter);
            
        const int32 NewLevel = IPlayerInterface::Execute_FindLevelForXP(Props.SourceCharacter, CurrentXP + LocalIncomingXP);
        const int32 NumLevelUps = NewLevel - CurrentLevel;
        if(NumLevelUps > 0)
        {
            IPlayerInterface::Execute_AddToPlayerLevel(Props.SourceCharacter, NumLevelUps);
            
            int32 AttributePointReward = 0;
            int32 SpellPointReward = 0;
            for (int32 i = 0; i < NumLevelUps; ++i)
            {
                AttributePointReward += IPlayerInterface::Execute_GetAttributePointsReward(Props.SourceCharacter, CurrentLevel + i); 
                SpellPointReward += IPlayerInterface::Execute_GetSpellPointsReward(Props.SourceCharacter, CurrentLevel + i); 
            }
            IPlayerInterface::Execute_AddToAttributePoints(Props.SourceCharacter, AttributePointReward);
            IPlayerInterface::Execute_AddToSpellPoints(Props.SourceCharacter, SpellPointReward);

            bTopOffHealth = true;
            bTopOffMana = true;

            IPlayerInterface::Execute_LevelUp(Props.SourceCharacter);
        }

        IPlayerInterface::Execute_AddToXP(Props.SourceCharacter, LocalIncomingXP);
    }
}
```

- **接口五连调用**：GetLevel/GetXP → FindLevelForXP（查 `DA_LevelUpInfo` 曲线）→ AddToPlayerLevel → 奖励循环（**跨多级升级**：一次吃到 3 级就把 2、3 级的奖励都拿——`CurrentLevel + i` 逐级查表累加）→ LevelUp（播特效）→ AddToXP。
- **注意顺序**：先 AddToPlayerLevel 再 AddToXP——等级先就位，XP 条 UI 刷新时等级显示正确。
- **`bTopOffHealth/bTopOffMana = true`**：升级请求"血蓝回满"，但**不能这里直接 SetHealth**——因为 `AddToPlayerLevel` → `DuraASC->UpdateAbilityStatuses` → 可能应用升级 GE 改 MaxHealth（比如升级时 MMC 重算）；要等 **PostAttributeChange**（MaxHealth 真正变化后）再回满。**延迟两步的补满标记**——见第 5.3 节。
- **为什么走 PlayerInterface**：SourceCharacter 是 Actor，"给 XP/查等级"是玩家能力——IPlayerInterface（第 11 篇）把"玩家特有操作"从 CombatInterface 里分离出来。接口分层的判断依据：**敌人不需要的实现，单独成接口**。

---

# 五、Pre / Post AttributeChange —— 属性修改的前后哨站 ★★★★★

## 5.1 三个回调的时机对比（★★★★★ 必考表格）

| 回调 | 触发时机 | 能改什么 | 典型用途 |
|---|---|---|---|
| `PreAttributeChange(Attribute, NewValue)` | **每次 CurrentValue 被计算/修改前**（包括聚合中间步骤） | **只能改 NewValue（临时值）**，不能改别的属性 | Clamp 当前值（血不超上限） |
| `PostGameplayEffectExecute(Data)` | GE 执行修改**完成后**（只对 Instant/Execute GE） | 一切 | 结算（扣血/死亡/UI/Debuff） |
| `PostAttributeChange(Attribute, Old, New)` | BaseValue 改变**后**（Set/Init 调用） | 一切（但此时改别的属性要小心递归） | 升级补满血蓝 |

- **PreAttributeChange 的陷阱**：它在**每次聚合重算时**都可能调（Base 变化、Modifier 增删都会触发），所以**只许做"当前值净化"**——里面调 `SetHealth()` 这种"改 Base 的操作"会引发重算循环（无限递归/未定义行为）。
- **为什么 Health 的 Clamp 出现在 Pre（Pre 里 Clamp Current）又出现在 PostGameplayEffectExecute（SetHealth Clamp）**：Pre 保住"当前值恒不越界"（包括 Buff 中间态）；PostExecute 里那份是**写 Base 时的对齐**（SetHealth 同时写 Base+Current，Base 也要在 [0,Max] 内）。两处不是冗余，是**两层防线**。

## 5.2 PreAttributeChange 本体

```cpp
void UDuraAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    if (Attribute == GetManaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
    }
}
```

- 只对 Health/Mana 做——**MaxHealth/MaxMana 本身不 Clamp**（上限可以被"诅咒减上限"设计修改，不该被硬编码约束）；其他属性（抗性可为负、点数可为 0）也不限制。

## 5.3 PostAttributeChange 与 bTopOff 标记

```cpp
void UDuraAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    if(Attribute == GetMaxHealthAttribute() && bTopOffHealth)
    {
        SetHealth(GetMaxHealth());
        bTopOffHealth = false;
    }
    if(Attribute == GetMaxManaAttribute() && bTopOffMana)
    {
        SetMana(GetMaxMana());
        bTopOffMana = false;
    }
}
```

- **一次性的补满请求**：HandleIncomingXP 置 true → 升级导致 MaxHealth Base 变化 → PostAttributeChange 捕获 → 立即 `SetHealth(MaxHealth)` 并**复位标记**（bool 只消费一次）。若标记不复位，以后任何 MaxHealth 微调都会把血瞬间拉满（隐藏 bug）。
- **"先标记、后执行"的跨回调协作模式**：当前回调做不了的事（时机不对）留给下游回调，用成员 bool 传递意图。**为什么不直接延迟 Timer**：事件驱动 > 定时猜测，PostAttributeChange 必然在 MaxHealth 变化后精确到达。

---

# 六、Debuff() —— 运行时动态构造 GameplayEffect ★★★★★（高级部分）

这是全项目最"魔法"的一段：**C++ 在运行时从零造一个 GE**。

```cpp
void UDuraAttributeSet::Debuff(const FEffectProperties& Props)
{
    if(!Props.SourceASC || !Props.TargetASC) return;   // 周期伤害结算期间 ASC 可能已销毁

    const FGameplayTag DamageType = UDuraAbilitySystemLibrary::GetDamageType(Props.EffectContextHandle);
    const float DebuffDamage = ...;   // 四参数从 Context 读（ExecCalc 掷骰结果）
    ...

    //创建动态 GameplayEffect
    FString DebuffName = FString::Printf(TEXT("DynamicDebuff_%s"), *DamageType.ToString());
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(DebuffName));
    
    Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Effect->Period = DebuffFrequency;
    Effect->DurationMagnitude = FScalableFloat(DebuffDuration);
```

- **为什么动态造而不用蓝图 GE 资产**：Debuff 的四个参数（伤害/时长/频率/类型）是**运行时随机/按技能等级变化的**——蓝图 GE 是静态资产，每变一次参数就要一个新资产；动态构造按需生成任意参数组合。**对比**：SetByCaller 也能把参数注入静态 GE（伤害主 GE 就是这么干的），但这里连" granted 的 Tag"都要按类型变（Burn vs Stun），动态构造更彻底。
- **`NewObject<UGameplayEffect>(GetTransientPackage(), FName(DebuffName))`**：
  - **GetTransientPackage()**：临时包——不属于任何关卡/资产，游戏退出即弃。动态对象的标准 Outer。
  - **FName 按 DamageType 命名**：调试时资产名可读（DynamicDebuff_Damage.Fire）。
- **三要素**：HasDuration（持续型）+ Period=DebuffFrequency（**每 N 秒跳一次伤害**——Period GE 的机制）+ DurationMagnitude=FScalableFloat(时长)。`FScalableFloat` 是"带曲线能力的常数"（这里当常数用）。

```cpp
    //添加目标标签组件
    UTargetTagsGameplayEffectComponent& TargetTagComponent = Effect->AddComponent<UTargetTagsGameplayEffectComponent>();
    FGameplayTagContainer TagContainer;
    FGameplayTag DebuffTag = GameplayTags.DamageTypesToDebuffs.FindRef(DamageType);
    if(!DebuffTag.IsValid()) return;
    TagContainer.AddTag(DebuffTag);
    if(DebuffTag.MatchesTagExact(GameplayTags.Debuff_Stun))
    {
        TagContainer.AddTag(GameplayTags.Player_Block_CursorTrace);
        TagContainer.AddTag(GameplayTags.Player_Block_InputHeld);
        TagContainer.AddTag(GameplayTags.Player_Block_InputPressed);
        TagContainer.AddTag(GameplayTags.Player_Block_InputReleased);
    }
    FInheritedTagContainer InheritedTagContainer;
    InheritedTagContainer.Added = TagContainer;
    TargetTagComponent.SetAndApplyTargetTagChanges(InheritedTagContainer);
```

- **GE5+ 的组件化 GE**：新版引擎的 GE 用 `AddComponent<T>()` 挂功能组件（Tag 授予、Cue、块间免疫……）。`UTargetTagsGameplayEffectComponent` = "给**目标**授予 Tag"。
- **查表取 Debuff Tag**：`DamageTypesToDebuffs.FindRef(DamageType)`——**FindRef vs operator[]**：缺失键时 FindRef 返回默认值（无效 Tag），operator[] 是断言崩溃。注释里写明"UE5.8 起 TMap::operator[] 对缺失键为断言语义"——**用 FindRef + IsValid 显式降级**，这是本项目的防御升级点（git 提交里的断言降级精神）。
- **眩晕特判**：Debuff.Stun 时额外授予四个 `Player_Block_*` Tag——**眩晕禁操作在 GE 的 grantedTag 层实现**！GE 挂上 → Tag 在 → 输入屏蔽；GE 过期 → Tag 自动移除 → 解锁。**用 GE 生命周期管理输入锁**，比手动 Add/Remove 更可靠（进程死亡/GE 移除时自动清理）。
- **`SetAndApplyTargetTagChanges(InheritedTagContainer)`**：配置组件的 Added 列表（应用时授予，移除时回收）。

```cpp
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Effect->StackingType = EGameplayEffectStackingType::AggregateBySource;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    Effect->StackLimitCount = 1;
```

- **叠加策略**：`AggregateBySource`（按施加者聚合）+ 限 1 层——**同一个敌人对同一目标只挂一个灼烧**（刷新时长而不叠加伤害）。源码注释解释了 PRAGMA 的原因：引擎 5.7 把 StackingType 转私有且 setter 未导出，暂时直赋+压警告。**读真实工程代码会遇到的兼容层**——面试聊"引擎版本升级的迁移经验"可用。
- **`StackLimitCount = 1`** 的含义：第二颗火球命中时**重置** Duration（RefreshPeriod）而不是叠层。

```cpp
    int32 Index = Effect->Modifiers.Num();
    Effect->Modifiers.Add(FGameplayModifierInfo());
    FGameplayModifierInfo& ModifierInfo = Effect->Modifiers[Index];
    ModifierInfo.ModifierMagnitude = FScalableFloat(DebuffDamage);
    ModifierInfo.ModifierOp = EGameplayModOp::Additive;
    ModifierInfo.Attribute = UDuraAttributeSet::GetIncomingDamageAttribute();
```

- **手动加 Modifier**：每 Period 跳一次，把 DebuffDamage **加到 IncomingDamage 元属性**——**Debuff 的伤害也走同一套结算漏斗**（受击反应/飘数字/死亡/XP 全部复用）！这就是元属性设计的回报。
- `Modifiers` 是 `TArray<FGameplayModifierInfo>`；每个 Modifier 三要素（Attribute 目标、Magnitude 数值、Op 运算）——与蓝图 GE 编辑器里配的 Modifier 面板一一对应。

```cpp
    FGameplayEffectContextHandle EffectContext = Props.SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(Props.SourceASC);

    FGameplayEffectSpec MutableSpec(Effect, EffectContext, 1.f);
    if(FDuraGameplayEffectContext* DuraContext = static_cast<FDuraGameplayEffectContext*>(MutableSpec.GetContext().Get()))
    {
        TSharedPtr<FGameplayTag> DebuffDamageType = MakeShared<FGameplayTag>(DamageType);
        DuraContext->SetDamageType(DebuffDamageType);
    }

    Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);
}
```

- **Spec 三参构造**：`FGameplayEffectSpec(Effect, Context, Level=1)`。
- **`static_cast<FDuraGameplayEffectContext*>`**：这里用 static_cast 而非 CastChecked——FGameplayEffectContext 不是 UObject，UE 的 Cast 不适用，只能 C++ 转型（**UObject 用 Cast，普通类用 dynamic_cast/static_cast** 的分界）。
- **把伤害类型写进 Context**：Debuff 每跳的 ShowFloatingText/后续 Debuff 判断要用。
- **`ApplyGameplayEffectSpecToSelf`**：目标 ASC 自己给自己上（我们正在目标的 AttributeSet 里执行，TargetASC 就是 owner）。
- **生效效果**：目标身上挂了"每 DebuffFrequency 秒跳 DebuffDamage 点伤害、持续 DebuffDuration 秒、授予 Burn/Stun Tag、限一层"的 GE——**而整个 GE 没有对应资产文件**。

## 6.1 元属性设计总结（★★★★★ 通俗解释）

**为什么伤害不直接写 Health？**

- 直接写：ExecCalc 算完 → `SetHealth(Health - Damage)`。**问题**：ExecCalc 是"计算器"，不该有"结算副作用"的权限（死亡/掉落/UI/AI 反应都不该在计算器里）；多个 GE 同帧改 Health 会互相覆盖；无法区分"这一刀"与"那个 Buff"。
- 元属性：ExecCalc 写 IncomingDamage → PostGameplayEffectExecute 统一结算。**收益**：
  1. **结算集中**：死亡判定、受击反应、击退、飘字、Debuff、XP 全在一处；
  2. **顺序正确**：GE 聚合完再结算（同帧多刀合并成一跳 or 依次结算）；
  3. **复用**：Debuff 周期伤害、直接伤害、径向伤害全走同一管道。
- **通俗解释**：IncomingDamage 是"账单信箱"——所有伤害都塞信箱（ExecCalc 只管写账单），月底（PostGameplayEffectExecute）财务统一结算扣款、发通知、上黑名单。直接改 Health 相当于每个柜台自己动手扣客户的钱——乱且对不上账。

---

# 七、MMC —— Modifier Magnitude Calculation 派生计算 ★★★★☆

## 7.1 MMC 是什么

`UGameplayModMagnitudeCalculation`：**GE Modifier 数值的自定义计算器**。GE 的 Modifier 可以配"固定值 / 曲线 / 属性引用 / SetByCaller"，复杂公式（多属性+外部数据）就用 MMC——**蓝图资产里选这个 C++ 类，聚合时调它的 CalculateBaseMagnitude**。

## 7.2 捕获定义（构造函数）

```cpp
UMMC_MaxHealth::UMMC_MaxHealth()
{
    VigorDef.AttributeToCapture = UDuraAttributeSet::GetVigorAttribute();
    VigorDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
    VigorDef.bSnapshot = false;

    RelevantAttributesToCapture.Add(VigorDef);
}
```

- **三要素**：
  - `AttributeToCapture`：捕获哪个属性（反射句柄）。
  - `AttributeSource`：**Target 还是 Source**？——取**谁的** Vigor。MaxHealth 是目标自己的派生 → Target。
  - **`bSnapshot`（★★★★★ 面试必考）**：`true`=快照——**GE 创建那一刻**捕获的值（攻击力定型，之后 Buff 自己不加成）；`false`=实时——**GE 生效期间每次重算**都取最新值（吃 Buff 立即涨血上限）。MaxHealth 用 false：升级加 Vigor 后 MaxHealth 要实时跟随。
  - `RelevantAttributesToCapture`：声明"我要捕获这些"——引擎在 Spec 创建时预先捕获，避免计算时实时查询。

## 7.3 计算函数

```cpp
float UMMC_MaxHealth::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;

    float Vigor = 0.f;
    GetCapturedAttributeMagnitude(VigorDef, Spec, EvaluationParameters, Vigor);
    Vigor = FMath::Max<float>(Vigor, 0.f);

    int32 PlayerLevel = 1;
    if(const UObject* SourceObject = Spec.GetContext().GetSourceObject())
    {
        if(SourceObject->Implements<UCombatInterface>())
        {
            PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceObject);
        }
    }

    return 80.f + 2.5f * Vigor + 10.f * PlayerLevel;
}
```

- **参数打包**：`FAggregatorEvaluateParameters` 带 Source/Target Tags——捕获的属性聚合时会考虑 Tag 修改器（某些 GE 带条件 Tag）。
- **`GetCapturedAttributeMagnitude(Def, Spec, Params, Out)`**：从 Spec 的预捕获数据里取值——**静态函数**，必传 Def（哪个属性）+Params（Tag 上下文）。
- **`FMath::Max(Vigor, 0)`**：负活力不加成——**公式自防御**。
- **等级从哪来（精妙处）**：`Spec.GetContext().GetSourceObject()`——Context 里的 SourceObject。**回顾第 2 篇**：`ApplyAttributeInitEffectToSelf` 里 `EffectContext.AddSourceObject(this)` 塞的就是**角色自己**！所以这里"问 SourceObject 要等级"= 问角色要等级（走 CombatInterface：玩家答 PlayerState 的 Level，敌人答自己的 Level 变量）。**MMC 用接口而不是 Cast 具体类**——同一个 MMC 服务玩家与敌人。
- **公式**：`80 + 2.5×Vigor + 10×Level`（MaxMana：`50 + 2×Intelligence + 10×Level`）。数值设计：基础血 80，每点活力 2.5 血，每级 10 血。
- **const 函数**：计算器不改状态。

## 7.4 数据流向图（GE_DuraSecondaryAttribute 资产怎么配）

```
GE_SecondaryAttribute 资产（蓝图）
  Modifier: Attribute=MaxHealth, Op=Additive, MagnitudeType=MMC → 选 UMMC_MaxHealth
       │ 应用时（SecondaryInitEffect → 玩家/敌人）
       ▼
MMC_MaxHealth::CalculateBaseMagnitude
  ├─ 捕获 Target.Vigor（实时）     ← AttributeSet.Vigor（刚被 PrimaryGE 写入）
  └─ SourceObject.GetPlayerLevel  ← Context.AddSourceObject(角色) 在第 2 篇塞入
       │
       ▼ 80 + 2.5×Vigor + 10×Level
  MaxHealth.CurrentValue = Base + 该值 → 触发属性变化委托 → 血条上限刷新
```

- **依赖顺序回顾**（第 2 篇埋的伏笔）：Primary 先写 Vigor → Secondary 的 MMC 才能捕获到正确 Vigor → Vital 的 Health=MaxHealth 才能引用到正确上限。**GE 应用顺序即依赖顺序**。

---

# 八、本章全链路总图

```
                    ┌────────────────────────────────────────┐
                    │        Init GE 三连（第 2 篇顺序）          │
                    │  Primary(Vigor=10) → Secondary(MMC 算   │
                    │  MaxHealth=80+2.5V+10L) → Vital(Health  │
                    │  = MaxHealth, Mana = MaxMana)           │
                    └────────────────┬───────────────────────┘
                                     │
   敌人火球命中（ExecCalc，第 5 篇）      │
   └─ 写 IncomingDamage（SetByCaller）  │
                                     ▼
                    ┌────────────────────────────────────────┐
                    │  PostGameplayEffectExecute（结算漏斗）     │
                    │  ① 死人守卫（周期Debuff幂等）               │
                    │  ② Health/Mana 兜底 Clamp                │
                    │  ③ IncomingDamage → HandleIncomingDamage │
                    │     ├─ 取件清零 → 扣血（原始差值判死）        │
                    │     ├─ 致死：Die(死亡冲量) + SendXPEvent   │
                    │     ├─ 未死：HitReact技能 / LaunchCharacter│
                    │     ├─ ShowFloatingText（暴击/格挡样式）    │
                    │     └─ Debuff：动态造 GE（Tag/周期/限层）    │
                    │  ④ IncomingXP → HandleIncomingXP          │
                    │     └─ 多级奖励 / bTopOff 标记 / LevelUp   │
                    └────────────────┬───────────────────────┘
                                     │
        PostAttributeChange（bTopOff → 补满）  PreAttributeChange（Clamp）
                                     │
                                     ▼
                    属性复制（REPNOTIFY_Always）→ OnRep → ATTRIBUTE_REPNOTIFY
                                     → 属性变化委托 → WidgetController → UI（第 8 篇）
```

# 九、动手实验建议

1. 把 `DOREPLIFETIME_CONDITION_NOTIFY` 的 `REPNOTIFY_Always` 改成 `REPNOTIFY_OnChanged`，读档恢复到相同血量，观察客户端血条不刷新——体感 Always 的意义。
2. 删掉 `OnRep_Health` 里的 `GAMEPLAYATTRIBUTE_REPNOTIFY` 宏（留空函数），客户端掉血时观察 UI 委托不再触发。
3. 在 `PreAttributeChange` 里加一行 `SetHealth(50)`，PIE 观察无限循环/崩溃——理解"Pre 只能改 NewValue"。
4. 给 `MMC_MaxHealth` 的 VigorDef 改 `bSnapshot = true`，升级加活力后观察 MaxHealth 不再实时变化。

---

*下一篇：`05-伤害计算与GameplayEffect流水线.md` —— ExecCalc_Damage 的捕获与公式、DuraAbilitySystemLibrary 全家桶、SetByCaller 注入。*
