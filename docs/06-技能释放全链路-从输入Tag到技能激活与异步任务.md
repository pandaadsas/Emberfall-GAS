# 06 · 技能释放全链路 —— 从 InputTag 到技能激活、异步任务与全部技能族

> **逻辑链路一句话概括**：
> `输入层转发的 InputTag 到达 ASC` → `ASC 遍历已授予技能的 Spec，找到带同款 InputTag 的技能` → `Held：TryActivateAbility / Pressed+Released：转发输入事件给激活中的技能` → `技能（蓝图/C++ 混合）执行：TargetDataUnderMouse 异步任务取目标 → 冷却/消耗 GE 校验 → 生成投射物/光束/召唤物` → `伤害参数走第 5 篇管线` → `冷却期间 WaitCooldownChange 监听 Tag 驱动 UI`。
> **本篇是 GAS 面试的"主战场"：激活流程、预测键、AbilityTask、InstancingPolicy、双 GE（Cost/Cooldown）全覆盖。**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/AbilitySystem/DuraAbilitySystemComponent.h` / `.cpp` | 输入转发、技能授予/升级/装备、客户端同步 | ★★★★★ |
| 2 | `Source/Dura/Public/AbilitySystem/Abilities/DuraGameplayAbility.h` / `.cpp` | 技能基类：描述生成、消耗/冷却查询 | ★★★★☆ |
| 3 | `Source/Dura/Public/AbilitySystem/Abilities/DuraDamageGameplayAbility.h` / `.cpp` | 伤害技能基类：参数工厂 | ★★★★★ |
| 4 | `Source/Dura/Public/AbilitySystem/AbilityTasks/TargetDataUnderMouse.h` / `.cpp` | 鼠标目标数据的网络同步任务 | ★★★★★ |
| 5 | `Source/Dura/Public/AbilitySystem/AsyncTasks/WaitCooldownChange.h` / `.cpp` | 冷却监听异步节点 | ★★★★☆ |
| 6 | `Source/Dura/Public/AbilitySystem/Abilities/DuraProjectileSpell.h` / `.cpp` | 投射物技能基类（SpawnActorDeferred 教科书） | ★★★★☆ |
| 7 | `Source/Dura/Public/AbilitySystem/Abilities/DuraFireBolt.h` / `.cpp` | 多弹道+追踪弹 | ★★★★☆ |
| 8 | `Source/Dura/Public/AbilitySystem/Abilities/DuraFireBlast.h` / `.cpp` | 360° 弹幕+回收爆炸 | ★★★☆☆ |
| 9 | `Source/Dura/Public/AbilitySystem/Abilities/ArcaneShards.h` / `.cpp` | 描述层（逻辑在蓝图） | ★★☆☆☆ |
| 10 | `Source/Dura/Public/AbilitySystem/Abilities/DuraBeamSpell.h` / `.cpp` | 光束/链电目标选择（C++/蓝图混合范式） | ★★★★☆ |
| 11 | `Source/Dura/Public/AbilitySystem/Abilities/Electrocute.h` / `.cpp` | 电击（描述层） | ★★☆☆☆ |
| 12 | `Source/Dura/Public/AbilitySystem/Abilities/DuraPassiveAbility.h` / `.cpp` | 被动技能的激活/停用回路 | ★★★★☆ |
| 13 | `Source/Dura/Public/AbilitySystem/Abilities/DuraSummonAbility.h` / `.cpp` | 召唤位置计算 | ★★★☆☆ |
| 14 | `Source/Dura/Public/AbilitySystem/Abilities/DuraMeleeAttack.h` | 纯数据技能（逻辑全蓝图） | ★★☆☆☆ |
| 15 | `Source/Dura/Public/AbilitySystem/Passive/PassiveNiagaraComponent.h` / `.cpp` | 被动光环组件（ASC 迟到的双保险） | ★★★★☆ |
| 16 | `Source/Dura/Public/AbilitySystem/Debuff/DebuffNiagaraComponent.h` / `.cpp` | 减益特效组件 | ★★★☆☆ |

---

# 一、DuraAbilitySystemComponent —— 技能系统的"总调度台" ★★★★★

## 1.1 六个委托：ASC 对外的"事件面板"

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer&);   // GE 资产标签
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);                                            // 初始技能授予完成
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);                // 遍历用函数委托
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAbilityStatusChanged, Tag, Tag, int32);         // 技能状态变化
DECLARE_MULTICAST_DELEGATE_FourParams(FAbilityEquipped, Tag, Tag, Tag, Tag);            // 装备事件
DECLARE_MULTICAST_DELEGATE_OneParam(FDeactivatePassiveAbility, const FGameplayTag&);    // 停被动
DECLARE_MULTICAST_DELEGATE_TwoParams(FActivatePassiveEffect, Tag, bool);               // 被动特效开关
```

- **委托的消费者全是 UI（第 8 篇）**：Overlay 监听 EffectAssetTags 显示buff 图标；SpellMenu 监听 AbilityStatusChanged/AbilityEquipped 刷新技能格子；PassiveNiagaraComponent 监听 ActivatePassiveEffect 开关光环。
- **`FForEachAbility` 是 `DECLARE_DELEGATE`（单播）**：遍历工具委托，每次 Bind 一个 Lambda——不是事件，是"函数参数"。`ExecuteIfBound` 带失败日志。
- **设计观察**：ASC 不直接碰 UI，全部通过委托广播"发生了什么"，UI 自己决定怎么显示——**发布订阅**。

## 1.2 匿名命名空间的预测键助手（★ 现代化改造点）

```cpp
namespace
{
    // 获取能力 Spec 的激活预测键。实例化能力必须从实例的 CurrentActivationInfo 读取；
    // 仅当能力未实例化（已弃用的旧策略）时才回退到 Spec 上的 ActivationInfo。
    FPredictionKey GetSpecActivationPredictionKey(const FGameplayAbilitySpec& AbilitySpec)
    {
        if (const UGameplayAbility* Instance = AbilitySpec.GetPrimaryInstance())
        {
            return Instance->GetCurrentActivationInfo().GetActivationPredictionKey();
        }
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        return AbilitySpec.ActivationInfo.GetActivationPredictionKey();
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
}
```

- **匿名 namespace**：C++ 的"文件内部可见"（等价旧式 static 函数），不污染外部链接。
- **预测键（Prediction Key，★★★★★ 面试高频）**：GAS 客户端预测的核心机制——客户端激活技能时生成一个键，服务器确认后**回执同一个键**，键匹配→预测成立不回滚；不匹配→回滚客户端表现。输入事件路由必须带着激活时的预测键，服务器才知道这是"哪个激活会话"的输入。
- **为什么从实例读**：InstancingPolicy=InstancedPerActor 的技能激活后产生**实例**，激活信息（含预测键）存在实例的 `CurrentActivationInfo`；直接读 Spec 的 ActivationInfo 是引擎已弃用的旧路径（PRAGMA 压弃用警告）。**本项目把课程代码里直接读 Spec 的写法升级成了这个助手函数**——这是 git 提交"现代化优化"的实例，面试聊"代码演进"的好素材。

## 1.3 输入三兄弟（与第 3 篇衔接）★★★★★

```cpp
void UDuraAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
    if (!InputTag.IsValid()) return;

    FScopedAbilityListLock ActiveScopeLock(*this);
    for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
        {
            AbilitySpecInputPressed(AbilitySpec);
            if (AbilitySpec.IsActive())
            {
                InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
                AbilitySpec.Handle, GetSpecActivationPredictionKey(AbilitySpec));
            }
        }
    }
}
```

- **`GetActivatableAbilities()`**：已授予技能的 Spec 数组（TArray<FGameplayAbilitySpec>）。
- **`FScopedAbilityListLock`**：**作用域锁**——遍历技能数组期间防止其他代码增删 Spec（数组失效/迭代器悬垂）。离开作用域自动解锁。**遍历容器又可能修改容器的场景必须锁**——GAS 内部很多路径会触发 GiveAbility/ClearAbility。
- **`GetDynamicSpecSourceTags()`**：Spec 的**动态标签**（授予时塞的，第 1.4 节）；`HasTagExact(InputTag)` 精确匹配。**这就是"技能认领按键"的匹配点**——技能 Spec 上挂着 `InputTag.LMB`，输入带着 `InputTag.LMB` 来认领。
- **Pressed 的双分支**：`AbilitySpecInputPressed` 通知技能"按下状态记录"（技能内部的 InputPressed 布尔），若技能**已激活**（比如按住持续施法中再点一下），`InvokeReplicatedEvent(InputPressed, Handle, 预测键)` 把"输入按下"这个事件**同步给服务器**的技能实例（技能蓝图里的 WaitInputPressed 等待节点被触发）。
- **Held 的逻辑不同**：

```cpp
void UDuraAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
    ...
    if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
    {
        AbilitySpecInputPressed(AbilitySpec);
        if (!AbilitySpec.IsActive())
        {
            TryActivateAbility(AbilitySpec.Handle);   // ← 激活发生在这里！
        }
    }
}
```

- **`TryActivateAbility(Handle)`**：**激活的正式入口**——内部依次检查：能否激活（CanActivateAbility：Cost 够不够、CD 中不中、Tag Block）、实例化、调用 ActivateAbility。**按住才触发激活**（LMB 点击移动游戏中"按住+松开"节奏的根源）。
- **Released**：

```cpp
        if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
        {
            AbilitySpecInputReleased(AbilitySpec);
            InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, GetSpecActivationPredictionKey(AbilitySpec));
        }
```

- 只对**激活中**的技能转发 Released（没激活谈不上松开）。技能蓝图里的 WaitInputReleased 节点等它。
- **服务器/客户端的双通道**：本地客户端 `TryActivateAbility` 走预测（客户端先播、服务器验证）；`InvokeReplicatedEvent` 是通用复制事件通道——**技能运行中"输入事件"本身要网络同步**（服务器技能实例也需要知道玩家何时按下/松开）。

## 1.4 技能授予三函数

```cpp
void UDuraAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities)
{
    for (const TSubclassOf<UGameplayAbility> AbilityClass : Abilities)
    {
        FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
        if (const UDuraGameplayAbility* DuraAbility = Cast<UDuraGameplayAbility>(AbilitySpec.Ability))
        {
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(DuraAbility->StartupInputTag);
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(FDuraGameplayTags::Get().Abilities_Status_Equipped);
            GiveAbility(AbilitySpec);
        }
    }
    bStartupAbilitiesGiven = true;
    AbilitiesGivenDelegate.Broadcast();
}
```

- **`FGameplayAbilitySpec(AbilityClass, 1)`**：构造时**立即创建技能实例**（Spec.Ability 就是实例/CDO——InstancedPerActor 情况下是实例）。Cast 它读 `StartupInputTag`（技能类编辑器里配的默认按键 Tag）。
- **Spec 的动态 Tag 二连**：`StartupInputTag`（**它认哪个键**——第 1.3 节匹配的另一半）+ `Abilities.Status.Equipped`（**初始即装备**）。**Tag 就是 Spec 的元数据总线**：键位、状态、槽位全挂在 Spec 上。
- **`GiveAbility(Spec)`**：正式授予（进 ActivatableAbilities 数组）。**服务器 only**（客户端调用会被拒——内部 `HasAuthority` 检查）。
- **收尾双件**：`bStartupAbilitiesGiven = true`（标志位，PassiveNiagaraComponent 用它判断"可以查状态了"）+ 广播 AbilitiesGivenDelegate（HUD 等待 UI 初始化的信号——技能授予完成后才创建技能栏图标）。

```cpp
void UDuraAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassiveAbilities)
{
    for (const TSubclassOf<UGameplayAbility> AbilityClass : PassiveAbilities)
    {
        FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
        AbilitySpec.GetDynamicSpecSourceTags().AddTag(FDuraGameplayTags::Get().Abilities_Status_Equipped);
        GiveAbilityAndActivateOnce(AbilitySpec);
    }
}
```

- **`GiveAbilityAndActivateOnce`**：授予后**立即激活**——被动技能（吸血光环）激活后内部的持续 GE 才生效。与主动技能的区别：主动等按键，被动立刻跑。

### 存档恢复版（AddCharacterAbilitiesFromSaveData）

```cpp
    for (const FSavedAbility& Data : SaveData->SavedAbilities)
    {
        FGameplayAbilitySpec LoadedAbilitySpec = FGameplayAbilitySpec(LoadedAbilityClass, Data.AbilityLevel);
        LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilitySlot);
        LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilityStatus);

        if(Data.AbilityType == Abilities_Type_Offensive) GiveAbility(LoadedAbilitySpec);
        else if(Data.AbilityType == Abilities_Type_Passive)
        {
            if(Data.AbilityStatus.MatchesTagExact(Abilities_Status_Equipped))
                GiveAbilityAndActivateOnce(LoadedAbilitySpec);
            else
                GiveAbility(LoadedAbilitySpec);
        }
    }
```

- **三段元数据恢复**：等级、槽位（InputTag）、状态（Locked/UnLocked/Equipped）——与存档的 FSavedAbility 字段一一对应（第 2 篇 SaveProgress 的逆过程）。
- **被动的条件激活**：只有存档状态是 Equipped 的被动才立即激活——**状态一致性**：UnLocked 未装备的被动不该发光/生效。

## 1.5 Spec 元数据查询族（8 个静态/成员函数）

```cpp
FGameplayTag UDuraAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
    for (FGameplayTag Tag : AbilitySpec.Ability->GetAssetTags())
    {
        if(Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
        {
            return Tag;
        }
    }
    return FGameplayTag();
}
```

- **`GetAssetTags()`**：技能**类**上的资产标签（蓝图 GA_FireBolt 的 AssetTags 里配了 `Abilities.Fire.FireBolt`）——**技能的身份牌**。
- **`MatchesTag("Abilities")`**：前缀匹配找出身份 Tag（Spec 可能带多个 AssetTag，只要 Abilities 开头的）。
- **`RequestGameplayTag(FName)`**：运行时按名字查 Tag——**能用但不如 Native Tag 成员**（运行时字符串查找+可能查不到返回空）。这里用于"根 Tag"常量可接受。
- 兄弟函数：`GetSlotFromSpec`（DynamicTags 里找 InputTag 开头）、`GetStatusFromSpec`（找 Abilities.Status 开头）、`GetStatusFromAbilityTag`/`GetSlotFromAbilityTag`（先按身份 Tag 查 Spec 再取值——**两层查询**）。

### 槽位管理（装备系统的基础设施）

```cpp
bool UDuraAbilitySystemComponent::SlotIsEmpty(const FGameplayTag& Slot)
{
    FScopedAbilityListLock ActiveScopeLock(*this);
    for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if(AbilityHasSlot(AbilitySpec, Slot)) return false;
    }
    return true;
}
```

- 槽位 = InputTag（LMB/1/2/Passive.1……）。**一个技能占一个槽，一个槽最多一个技能**。槽位操作全是"遍历数组+Tag 检查"——技能量小，线性够用。
- `ClearSlot`/`AssignSlotToAbility`/`ClearAbilitiesOfSlot`：Tag 的 Remove/Add 组合。**没有专门的槽位数据结构——槽位状态全在 Spec 的 DynamicTags 里**（单一数据源，不会出现"槽表和技能不同步"的 bug）。

## 1.6 ServerSpendSpellPoint —— 技能升级状态机 ★★★★★

```cpp
void UDuraAbilitySystemComponent::ServerSpendSpellPoint_Implementation(const FGameplayTag& AbilityTag)
{
    if(FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        AActor* Avatar = GetAvatarActor();
        if(Avatar && Avatar->Implements<UPlayerInterface>())
        {
            IPlayerInterface::Execute_AddToSpellPoints(Avatar, -1);   // 扣技能点
        }

        FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);
        if(Status.MatchesTagExact(GameplayTags.Abilities_Status_Eligible))
        {
            AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(Abilities_Status_Eligible);
            AbilitySpec->GetDynamicSpecSourceTags().AddTag(Abilities_Status_UnLocked);
            Status = Abilities_Status_UnLocked;
        }
        else if(Status.MatchesTagExact(Abilities_Status_Equipped) || Status.MatchesTagExact(Abilities_Status_UnLocked))
        {
            AbilitySpec->Level++;    // 已解锁/已装备 → 升一级
        }
        ClientUpdateAbilityStatus(AbilityTag, Status, AbilitySpec->Level);
        MarkAbilitySpecDirty(*AbilitySpec);
    }
}
```

- **`Server` RPC（Client → Server Reliable）**：玩家在技能菜单点"升级"→ 客户端调 → 服务器执行。**服务器是权威**：扣点、改状态都在服务器。
- **状态机迁移**：`Eligible → UnLocked`（第一次花钱=学习）；`UnLocked/Equipped → Level+1`（之后花钱=升级）。**同一个 RPC 两种语义，按当前状态分流**。
- **`MarkAbilitySpecDirty(*AbilitySpec)`**：**把 Spec 标脏→网络复制到客户端**。Mixed 复制模式下 Spec 数组的复制依赖脏标记——**不加这行客户端的技能等级永远停在旧值**（又一个静默 bug 制造机）。
- **`ClientUpdateAbilityStatus`（Server → Client RPC）**：主动通知客户端"状态变了"→ 客户端广播 AbilityStatusChanged → 技能菜单 UI 刷新。**为什么不用复制**：Spec 复制是"最终一致"（可能延迟几秒），RPC 是"立刻"——UI 反馈要即时。

### UpgradeAttribute（属性加点，同构模式）

```cpp
void UDuraAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
    AActor* Avatar = GetAvatarActor();
    if(Avatar && Avatar->Implements<UPlayerInterface>())
    {
        if(IPlayerInterface::Execute_GetAttributePoints(Avatar) > 0)
        {
            ServerUpgradeAttribute(AttributeTag);
        }
    }
}

void UDuraAbilitySystemComponent::ServerUpgradeAttribute_Implementation(const FGameplayTag& AttributeTag)
{
    ...
    FGameplayEventData Payload;
    Payload.EventTag = AttributeTag;
    Payload.EventMagnitude = 1.f;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, AttributeTag, Payload);
    if(Avatar->Implements<UPlayerInterface>())
    {
        IPlayerInterface::Execute_AddToAttributePoints(Avatar, -1);
    }
}
```

- **客户端预检 + 服务器执行**：本地先查点数（UI 响应快），服务器再真正扣（防作弊重放）。
- **属性加成走 GameplayEvent**：`SendGameplayEventToActor(Avatar, Attributes.Primary.Strength, Payload)`——**蓝图技能 GA_ListenForEvent 监听这个 Tag**，收到事件后应用对应主属性 GE（+1 力量）。**为什么绕道事件而不是直接改属性**：属性修改必须走 GE（复制/聚合/委托全套），蓝图侧已有通用的"事件→GE"技能模板，复用它零 C++。

## 1.7 ServerEquipAbility —— 装备交互最复杂的一段 ★★★★☆

```cpp
void UDuraAbilitySystemComponent::ServerEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Slot)
{
    if(FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        const FGameplayTag& PrevSlot = GetSlotFromSpec(*AbilitySpec);
        const FGameplayTag& Status = GetStatusFromSpec(*AbilitySpec);
        const bool bStatusValid = (Status == Abilities_Status_Equipped || Status == Abilities_Status_UnLocked);
        if(bStatusValid)
        {
            if(!SlotIsEmpty(Slot))   // 目标槽已被占
            {
                if(FGameplayAbilitySpec* SpecWithSlot = GetSpecWithSlot(Slot))
                {
                    if(AbilityTag.MatchesTagExact(GetAbilityTagFromSpec(*SpecWithSlot)))
                    {
                        ClientEquipAbility(AbilityTag, Abilities_Status_Equipped, Slot, PrevSlot);
                        return;    // 装到自己的槽 = 无操作（取消装备）
                    }
                    if(IsPassiveAbility(*SpecWithSlot))   // 被顶掉的技能是被动 → 停用
                    {
                        MultiCastActivatePassiveEffect(GetAbilityTagFromSpec(*SpecWithSlot), false);
                        DeactivatePassiveAbility.Broadcast(GetAbilityTagFromSpec(*SpecWithSlot));
                    }
                    ClearSlot(SpecWithSlot);
                }
            }

            if(!AbilityHasAnySlot(*AbilitySpec))   // 自己没在槽上（UnLocked→首次装备）
            {
                if(IsPassiveAbility(*AbilitySpec))
                {
                    TryActivateAbility(AbilitySpec->Handle);   // 被动激活
                    MultiCastActivatePassiveEffect(AbilityTag, true);
                }
                AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GetStatusFromSpec(*AbilitySpec));
                AbilitySpec->GetDynamicSpecSourceTags().AddTag(Abilities_Status_Equipped);
            }
            AssignSlotToAbility(*AbilitySpec, Slot);
            MarkAbilitySpecDirty(*AbilitySpec);
        }
        ClientEquipAbility(AbilityTag, Abilities_Status_Equipped, Slot, PrevSlot);
    }
}
```

- **装备的四种情况**：①装到空槽（简单）；②装到**自己已占的槽**（=取消操作，客户端回执后 UI 复位）；③顶掉**主动技能**（旧技能 ClearSlot，回到 UnLocked 可再装备）；④顶掉**被动技能**（额外要停用：`DeactivatePassiveAbility.Broadcast` 让技能实例 EndAbility + `MultiCastActivatePassiveEffect(tag,false)` 让光环粒子全端熄灭）。
- **被动装备的激活**：UnLocked 被动首次装上 → `TryActivateAbility`（被动开始运行）+ Multicast 开光环。
- **`IsPassiveAbility` 的判定走数据资产**（AbilityInfo 查 AbilityType）而不是 Cast 技能类——**配置驱动**，新增被动类型不用改 ASC。
- **`ClientEquipAbility`（4 参数回执）**：新状态+新槽+旧槽——UI 需要旧槽位置来清空旧图标。**RPC 携带完整上下文，UI 无需反查**。

## 1.8 UpdateAbilityStatuses —— 升级解锁扫描

```cpp
void UDuraAbilitySystemComponent::UpdateAbilityStatuses(int32 Level)
{
    UAbilityInfo* AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
    if(!AbilityInfo) return;

    for (const FDuraAbilityInfo& Info : AbilityInfo->AbilityInformation)
    {
        if(!Info.AbilityTag.IsValid()) continue;
        if(Level < Info.LevelRequirement) continue;

        if(GetSpecFromAbilityTag(Info.AbilityTag) == nullptr)
        {
            FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Info.Ability, 1);
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(Abilities_Status_Eligible);
            GiveAbility(AbilitySpec);
            MarkAbilitySpecDirty(AbilitySpec);
            ClientUpdateAbilityStatus(Info.AbilityTag, Abilities_Status_Eligible, 1);
        }       
    }
}
```

- **升级时触发**（第 2 篇 AddToPlayerLevel 里调）：扫描 DA_AbilityInfo 全表，"等级达标 && 还没授予"的技能以 **Eligible（可学）状态**授予。**Locked 状态的技能没有 Spec**（UI 上显示锁定靠 AbilityInfo 的等级需求对比玩家等级），达标的才给 Spec。
- **三件套收尾**：Give + MarkDirty（复制）+ Client RPC（即时 UI）——与升级/装备同构。

## 1.9 GetDescriptionsByAbilityTag —— 描述查询的降级链

```cpp
bool UDuraAbilitySystemComponent::GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, 
    FString& OutDescription, FString& OutNextLevelDescription)
{
    if(const FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
    {
        if(UDuraGameplayAbility* DuraAbility = Cast<UDuraGameplayAbility>(AbilitySpec->Ability))
        {
            OutDescription = DuraAbility->GetDescription(AbilitySpec->Level);                
            OutNextLevelDescription = DuraAbility->GetNextLevelDescription(AbilitySpec->Level + 1);     
            return true;
        }
    }
    const UAbilityInfo* AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
    if(!AbilityTag.IsValid() || AbilityTag.MatchesTagExact(Abilities_None))
    {
        OutDescription = FString();
    }
    else
    {
        OutDescription = UDuraGameplayAbility::GetLockedDescription(AbilityInfo->FindAbilityInfoForTag(AbilityTag).LevelRequirement);
    }
    OutNextLevelDescription = FString();
    return false;
}
```

- **三级降级**：有 Spec → 真实技能描述（当前级+下一级）；无效 Tag → 空描述；**已锁定** → "Spell Locked / Until Level N"（静态函数版）。**UI 永远拿到可显示的内容**。
- **返回值 bool 的含义**：是否是"已拥有"技能（UI 据此决定显示学习按钮还是升级按钮）。

## 1.10 OnRep_ActivateAbilities 与 ClientEffectApplied

```cpp
void UDuraAbilitySystemComponent::OnRep_ActivateAbilities()
{
    Super::OnRep_ActivateAbilities();
    if(!bStartupAbilitiesGiven)
    {
        bStartupAbilitiesGiven = true;
        AbilitiesGivenDelegate.Broadcast();        
    }    
}

void UDuraAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, 
    const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
    FGameplayTagContainer TagContainer;
    EffectSpec.GetAllAssetTags(TagContainer);
    EffectAssetTags.Broadcast(TagContainer);
}
```

- **`OnRep_ActivateAbilities`**：**客户端收到技能列表复制**的回调。客户端没有"服务器授予技能"的时刻（授予是服务器逻辑），客户端靠 **Spec 数组复制完成**这个事件来广播 AbilitiesGiven——**UI 初始化的客户端信号源**。`bStartupAbilitiesGiven` 防重复广播。
- **`ClientEffectApplied`**：`AbilityActorInfoSet()` 里注册到 `OnGameplayEffectAppliedDelegateToSelf`——**每次 GE 应用到自身**：把 GE 的**资产标签**（AssetTags，蓝图 GE 里配的展示用 Tag，如 "Effects.HitReact"）广播出去 → Overlay UI 收到后决定显示什么 buff 图标。
- **为什么 AssetTags 而不是 GrantedTags**：AssetTag 是"给外部看的说明"（UI/音效/Cue 用），GrantedTag 是"给系统用的机制"（影响属性/状态）——**展示与机制分离**。

## 1.11 面试考点清单（ASC 部分）

1. **TryActivateAbility 内部流程？**（CanActivateAbility 检查 Cost/Cooldown/BlockTags → 实例化 → ActivateAbility → InternalTryActivateAbility 带预测）
2. **Spec 的 DynamicTags vs 技能 AssetTags vs GrantedTags？**（实例元数据 / 类身份 / GE 授予的状态）
3. **客户端技能 UI 怎么知道初始化完成？**（服务器：AbilitiesGivenDelegate；客户端：OnRep_ActivateAbilities）
4. **为什么 MarkAbilitySpecDirty 必须调？**

---

# 二、DuraGameplayAbility —— 技能基类 ★★★★☆

## 2.1 StartupInputTag 与描述系统

```cpp
UPROPERTY(EditDefaultsOnly, Category="Input")
FGameplayTag StartupInputTag;
```

- **技能类编辑器里配"我认哪个键"**——AddCharacterAbilities 授予时把它抄进 Spec 的 DynamicTags。**类属性 → 实例元数据**的一次性搬运（之后改按键只改 Spec 的 Tag）。

```cpp
float UDuraGameplayAbility::GetManaCost(float InLevel) const
{
    float ManaCost = 0.0f;
    if(const UGameplayEffect* CostEffect = CostGameplayEffectClass.GetDefaultObject())
    {
        for(FGameplayModifierInfo Mod : CostEffect->Modifiers)
        {
            if(Mod.Attribute == UDuraAttributeSet::GetManaAttribute())
            {
                Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, ManaCost);
                break;
            }
        }
    }
    return ManaCost;
}
```

- **`CostGameplayEffectClass.GetDefaultObject()`**：拿 GE 类的 **CDO**（默认对象）——**不实例化就读取 GE 资产的配置**（Modifiers 数组）。读配置的标准手法。
- **遍历 Modifier 找 Mana 属性的那个** → `GetStaticMagnitudeIfPossible(InLevel, Out)`：**静态量值**查询（常数或曲线可查；SetByCaller 查不了返回 false）——技能描述里显示的消耗数值与实际扣的一致（同一数据源）。
- **GetCooldown 同构**：读 `CooldownGameplayEffectClass` 的 `DurationMagnitude`。
- **注意**：`for(FGameplayModifierInfo Mod : ...)` **按值拷贝** Modifier（应为 const 引用）——小对象无所谓，但是代码风格点。

## 2.2 描述的富文本格式

```cpp
FString UDuraGameplayAbility::GetDescription(int32 Level)
{
    return FString::Printf(TEXT("<Default>%s, </><Level>%d</>"), TEXT("Default ..."), Level);
}
```

- `<Default>`/`<Level>` 这类尖括号是 **UMG RichTextBlock 的富文本标签**（配 DataTable 的样式表渲染不同颜色字体）。子类（FireBolt/Electrocute/ArcaneShards/FireBlast）覆写后写**中文描述模板**，如：

```cpp
return FString::Printf(TEXT(
    "<Title>火弹术</>\n\n"
    "<Small>等级：</><Level>%d</>\n"
    "<Small>魔法消耗：</><ManaCost>%.1f</>\n"
    "<Small>冷却时间：</><Cooldown>%.1f</>\n"
    "\n<Default>发射 %d 枚火弹，命中时爆炸并造成</>"
    "<Damage>%d</>"
    "<Default> 点火焰伤害，并有概率点燃</>"),
    Level, ManaCost, Cooldown, FMath::Min(Level, NumProjectiles), DamageValue);
```

- **数据全部实时查询**（Damage 曲线按级、GetManaCost、GetCooldown）——**描述不会撒谎**：升级前后描述自动跟着数值走。这就是为什么消耗/冷却查询函数存在：UI 描述与实际数值同源。
- `FMath::Min(Level, NumProjectiles)`：等级 1~5 发弹数递增但封顶 5——**描述与逻辑共用同一公式**（SpawnProjectiles 里也是 Min）。
- **面试点**：*"技能描述如何保持与实际数值一致？"* —— 从 GE CDO/曲线实时取值 + 同一计算函数。

---

# 三、DuraDamageGameplayAbility —— 伤害技能基类 ★★★★★

## 3.1 数据成员（伤害技能的"策划面板"）

```cpp
TSubclassOf<UGameplayEffect> DamageEffectClass;   // GE_Damage
FGameplayTag DamageType;                          // Damage.Fire 等
FScalableFloat Damage;                            // 基础伤害曲线
float DebuffChance = 20.f;  DebuffDamage = 5.f;  DebuffFrequency = 1.f;  DebuffDuration = 5.f;
float DeathImpulseMagnitude = 1000.f;  KnockbackMagnitude = 1000.f;  KnockbackChance = 0.f;
bool bIsRadialDamage = false;  RadialDamageInnerRadius/OuterRadius = 0.f;
```

- **`FScalableFloat Damage`**：可配曲线的浮点——编辑器里可直接填常数或绑曲线资产。`Damage.GetValueAtLevel(等级)` 取值。**技能等级→伤害曲线**是数据驱动的核心一环。
- Debuff 默认值（20% 概率 / 5 伤害 / 1 秒间隔 / 5 秒持续）是"合理缺省"——子类蓝图覆盖。
- **每个字段都对应第 5 篇 FDamageEffectParams 的同名字段**——这个类就是 Params 的"技能侧数据源"。

## 3.2 MakeDamageEffectParamsFromClassDefaults —— 参数工厂（8 个参数的设计）★★★★★

```cpp
FDamageEffectParams UDuraDamageGameplayAbility::MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor, 
        FVector InRadialDamageOrigin,
        bool bOverrideKnockbackDirection, FVector InKnockbackDirectionOverride,
        bool bOverrideDeathImpulse, FVector InDeathImpulseDirectionOverride,
        bool bOverridePitch, float PitchOverride) const
```

- **蓝图签名解读**：前 1 个是目标（可为空），后面 6 个是**三对"覆盖开关+覆盖值"**（击退方向/死亡冲量方向/俯仰角）。**为什么需要覆盖机制**：
  - **默认行为**（不覆盖）：朝目标方向击退/死亡冲量，俯仰角固定 45°（朝上扬——尸体飞起来更帅）。
  - **覆盖场景**：爆炸类技能（FireBlast）方向从爆心算、向上弹射要压平 Pitch、击退方向跟随弹体飞行方向……
- **默认分支**：

```cpp
    if(IsValid(TargetActor) && GetAvatarActorFromActorInfo())
    {
        FRotator Rotation = (TargetActor->GetActorLocation() - GetAvatarActorFromActorInfo()->GetActorLocation()).Rotation();
        if(bOverridePitch) Rotation.Pitch = PitchOverride;
        else Rotation.Pitch = 45.f;                      // ← 默认 45° 仰角
        const FVector ToTarget = Rotation.Vector();
        if(!bOverrideKnockbackDirection) Params.KnockbackForce = ToTarget * KnockbackMagnitude;
        if(!bOverrideDeathImpulse) Params.DeathImpulse = ToTarget * DeathImpulseMagnitude;
    }
```

- **朝向计算**：`目标位置 - 我的位置` 取 Rotation——"攻击者指向受击者"的单位方向。**只有给了 TargetActor 才有方向**（投射物飞行途中还不知道谁被击中，Target=nullptr 时方向留空，命中回调时再覆盖——**Params 的增量填充模式**）。
- **覆盖分支**（击退示例）：

```cpp
    if(bOverrideKnockbackDirection)
    {
        InKnockbackDirectionOverride.Normalize();   // 归一化防呆
        Params.KnockbackForce = InKnockbackDirectionOverride * KnockbackMagnitude;
        if(bOverridePitch)
        {
            FRotator KnockbackRotation = InKnockbackDirectionOverride.Rotation();
            KnockbackRotation.Pitch = PitchOverride;
            Params.KnockbackForce = KnockbackRotation.Vector() * KnockbackMagnitude;
        }
    }
```

- **优先级**：显式覆盖 > 目标方向默认。Pitch 覆盖可以把击退压成纯水平（爆炸把人**推开**而不是抛飞）或拉高（击飞）。**两套自由度正交组合**（方向来源 × 俯仰角）。
- **风险点**：`InKnockbackDirectionOverride.Normalize()` 对零向量 Normalize 会产生 NaN（UE 的 Normalize 零向量安全返回零向量，OK）。

## 3.3 CauseDamage —— 直伤快捷路径

```cpp
void UDuraDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if(!SourceASC || !TargetASC) return;

    FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, 1.f);
    const float ScaledDamage = Damage.GetValueAtLevel(GetAbilityLevel());
    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, DamageType, ScaledDamage);
    SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
}
```

- **简化版伤害路径**（不走 Params/Context/Debuff）：MeleeAttack 等简单攻击用。**两条伤害路径的分工**：完整路径（Params→Library→Context/Debuff/击退）vs 简化路径（直接 Spec+SetByCaller）——按需选复杂度。
- `GetAbilityLevel()`：技能基类 API——当前 Spec 的等级。
- **注意**：这条路径的 Context 没有自定义数据（没有格挡/暴击标志）——ExecCalc 仍会写（它拿的是 Spec 的 Context，已被 Globals 替换成 FDura 版），但攻击方参数（击退）没有。

## 3.4 GetRandomTaggedMontageFromArray

```cpp
    const int32 Selection = FMath::RandRange(0, TaggedMontages.Num() - 1);
    return TaggedMontages[Selection];
```
- 空数组返回默认 FTaggedMontage（Tag 无效——调用方检查）。**近战攻击随机选一段攻击动画**的通用工具（敌人 AI 攻击用）。

---

# 四、TargetDataUnderMouse —— 目标数据的网络同步（★★★★★ 本篇最难）

## 4.1 它解决什么问题

火球技能蓝图需要"鼠标点的位置"来决定弹道方向。**问题**：鼠标位置只在**客户端**有意义（服务器没有鼠标）！如何让服务器的技能逻辑拿到客户端的鼠标点？——**TargetData（目标数据）**机制：客户端算好 → 打包成 FGameplayAbilityTargetDataHandle → 带预测键发给服务器 → 双方技能蓝图同时收到 ValidData 回调。

## 4.2 静态工厂与蓝图接入

```cpp
UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName="TargetDataUnderMouse", HidePin = "OwingAbility", DefaultToSelf = "OwingAbility", BlueprintInternalUseOnly = "true"))
static UTargetDataUnderMouse* CreateTargetDataUnderMouse(UGameplayAbility* OwingAbility);
```

- **`BlueprintInternalUseOnly = "true"`**：静态工厂**不在蓝图面板直接出现**——蓝图里的"TargetDataUnderMouse"节点是 **UK2Node**（异步任务节点模板）生成的，节点实际调这个工厂。**AbilityTask 节点的双面性**：蓝图看到的是"等待+输出执行脚"，C++ 是工厂+委托。
- **`NewAbilityTask<T>(OwingAbility)`**：Task 的正确创建宏（设置 Owner、注册进技能的任务列表——技能结束自动清理）。
- **`DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMouseTargetDataSignature, const FGameplayAbilityTargetDataHandle&, DataHandle)`**：**动态**委托（蓝图要绑定 ValidData 输出脚）。

## 4.3 Activate —— 双端分流

```cpp
void UTargetDataUnderMouse::Activate()
{
    Super::Activate();
    const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
    if (bIsLocallyControlled)
    {
        SendMouseCursorData();
    }
    else
    {
        const FGameplayAbilitySpecHandle SpecHandler = GetAbilitySpecHandle();
        const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();

        AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandler, ActivationPredictionKey).
            AddUObject(this,&UTargetDataUnderMouse::OnTargetDataReplicatedCallback);

        const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandler, ActivationPredictionKey);
        if (!bCalledDelegate)
        {
            SetWaitingOnRemotePlayerData();
        }
    }
}
```

- **`IsLocallyControlled()`**：本地端（玩家自己的客户端/独立服务器）→ **主动发送**；远端（服务器收到客户端激活）→ **等待接收**。
- **接收端三步**（注释是原文翻译）：
  1. **`AbilityTargetDataSetDelegate(SpecHandle, PredictionKey)`**：注册"这个技能实例+这次激活"的 TargetData 到达回调。
  2. **`CallReplicatedTargetDataDelegatesIfSet`**：**竞态兜底**——数据可能已经到了（复制回调先于 Activate 执行）。已设置→立即手动触发委托（返回 true）；未设置→挂起等待（返回 false）。
  3. **`SetWaitingOnRemotePlayerData()`**：标记任务等待远程数据（数据到达自动触发已注册的委托）。
- **为什么要竞态兜底**：服务器执行顺序是"收到激活 RPC → ActivateAbility → Task.Activate"，而 TargetData 的复制包**可能先到**（网络乱序/同帧批量）——不查"已设置"会永久漏接。
- **通俗解释**：点外卖（TargetData）跟搬进新家（Activate）赛跑。若外卖先到（数据已设置），搬完家立刻查收；若人先到，挂个"到了请放门口"（注册委托）等快递。

## 4.4 SendMouseCursorData —— 发送端

```cpp
void UTargetDataUnderMouse::SendMouseCursorData()
{
    FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get(), true);

    APlayerController* Controller = Ability->GetCurrentActorInfo()->PlayerController.Get();
    FHitResult hitResult;
    Controller->GetHitResultUnderCursor(ECC_Target, false, hitResult);

    FGameplayAbilityTargetDataHandle DataHandle;
    FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
    Data->HitResult = hitResult;
    DataHandle.Add(Data);

    AbilitySystemComponent->ServerSetReplicatedTargetData(
        GetAbilitySpecHandle(), 
        GetActivationPredictionKey(), 
        DataHandle, 
        FGameplayTag(), 
        AbilitySystemComponent->ScopedPredictionKey
    );

    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}
```

- **`FScopedPredictionWindow(ASC, true)`**：**预测窗口**——作用域内创建一个新的 ScopedPredictionKey（作用域结束自动回收注册）。RPC 携带它=告诉服务器"我预测这些内容，键是 X"。**RAII 式预测管理**。
- **`GetHitResultUnderCursor(ECC_Target, ...)`**：注意通道是 **ECC_Target**（第 1 篇的宏，GameTraceChannel2）——**技能瞄准专用通道**（与高亮的 Visibility 区分：Target 只命中可被技能瞄准的物体）。
- **TargetData 的内容**：`FGameplayAbilityTargetData_SingleTargetHit`（命中结果型目标数据）装 HitResult（命中点+命中 Actor 一起带走）。**TargetData 是可扩展的**（还有 ActorArray/LocationInfo 等子类）。
- **`ServerSetReplicatedTargetData(Handle, Key, Data, Tag, ScopedKey)`**：客户端→服务器 RPC（GAS 内建），把目标数据绑定到"技能实例+激活预测键"上。
- **`ShouldBroadcastAbilityTaskDelegates()`**：Task 委托广播守卫——技能已结束/任务被清理时不广播。**Task 委托广播前的标准检查**。

## 4.5 OnTargetDataReplicatedCallback —— 接收端回调

```cpp
void UTargetDataUnderMouse::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
    AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}
```

- **`ConsumeClientReplicatedTargetData`**：**消费**（清除）这份数据——防重复触发（服务器收到后从缓存摘除）。
- **之后广播 ValidData** → 服务器端技能蓝图的"ValidData"输出脚执行 → 服务器开始生成投射物（权威逻辑）。
- **★ 面试满分答法**（"客户端技能如何获得鼠标目标"）：客户端 Task 算鼠标命中 → 预测键打包 ServerSetReplicatedTargetData → 服务器按（SpecHandle+预测键）路由到同技能的任务实例 → Consume + Broadcast → 两端蓝图同步继续执行。追问点：为什么要预测键（绑定激活会话）、为什么 Consume（防重放）、竞态兜底（CallReplicatedTargetDataDelegatesIfSet）。

---

# 五、WaitCooldownChange —— 冷却监听节点 ★★★★☆

## 5.1 与 AbilityTask 的区别

`UWaitCooldownChange : UBlueprintAsyncActionBase`——**不是 AbilityTask**！它是**蓝图异步动作**（BlueprintAsyncActionBase）。区别：

| | AbilityTask | BlueprintAsyncAction |
|---|---|---|
| 归属 | 技能实例，随技能结束清理 | 挂任意 UObject（常驻） |
| 激活权限 | 只能在技能内用 | 任何蓝图 |
| 生命周期 | 技能 EndAbility 自动清 | 手动 EndTask |

冷却监听用于**技能栏 UI**（不在技能内部跑）——所以选 BlueprintAsyncAction（UI 蓝图里也调得到）。

## 5.2 双监听器设计

```cpp
UWaitCooldownChange* UWaitCooldownChange::WaitForCooldownChange(UAbilitySystemComponent* AbilitySystemComponent, 
    const FGameplayTag& InCooldownTag)
{
    UWaitCooldownChange* WaitCooldownChange = NewObject<UWaitCooldownChange>();
    WaitCooldownChange->ASC = AbilitySystemComponent;
    WaitCooldownChange->CooldownTag = InCooldownTag;

    if(!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
    {
        WaitCooldownChange->EndTask();
        return nullptr;
    }

    // To know when a cooldown effect has ended
    AbilitySystemComponent->RegisterGameplayTagEvent(InCooldownTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(WaitCooldownChange, &UWaitCooldownChange::CooldownTagChanged);

    // To know when a cooldown effect has been applied
    AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf
        .AddUObject(WaitCooldownChange, &UWaitCooldownChange::OnActiveEffectAdded);

    return WaitCooldownChange;
}
```

- **为什么需要两个监听（★★★★★ 设计精要）**：
  - **冷却开始**（CD GE 应用）时：GE 应用**不改变** Cooldown Tag 的"身上有无"从无到有？——会！CD GE 的 GrantedTags 带 CoolDown.Fire.FireBolt → Tag 事件也能感知开始。**但是**：Tag 事件带不了"剩余秒数"——UI 需要"还剩几秒"来画转圈。所以开始事件走 **GE 添加委托**（能拿到 Spec → 查剩余时间）；结束事件走 **Tag 事件**（GE 移除 → Tag 计数归零）。
  - **`CooldownTagChanged`**：`NewCount == 0` → `CooldownEnd.Broadcast(0.f)`——Tag 移除=冷却结束。
- **OnActiveEffectAdded（开始侧）**：

```cpp
    FGameplayTagContainer AssetTags / GrantTags = ...GetAllAssetTags/GetAllGrantedTags...;
    if(AssetTags.HasTagExact(CooldownTag) || GrantTags.HasTagExact(CooldownTag))
    {
        FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTag.GetSingleTagContainer());
        TArray<float> TimeRemaining = InASC->GetActiveEffectsTimeRemaining(Query);
        if(TimeRemaining.Num() > 0)
        {
            float MaxTimeRemaining = ...手动遍历取最大...;
            CooldownStart.Broadcast(MaxTimeRemaining);
        }
    }
```

- **两道过滤**：先看这个新 GE 的 Tag 是否含冷却 Tag（**任何 GE 添加都会触发这个委托**——不筛就是"放个 Buff 也刷新冷却 UI"的 bug）；再按 Tag 查询活动 GE 的剩余时间数组。
- **取最大值**：理论上可能同时有多个冷却 GE（叠加装备效果）——**UI 显示剩余最久的**。
- **`FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags`**：构造"OwningTags 含此 Tag 的活动 GE"查询——GAS 的 GE 查询 DSL。

## 5.3 EndTask —— 清理的对称性

```cpp
void UWaitCooldownChange::EndTask()
{
    if(IsValid(ASC))
    {
        ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
        ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
    }
    SetReadyToDestroy();
    MarkAsGarbage();
}
```

- **注册了多少就解绑多少**（对称原则）。`RemoveAll(this)` 摘掉本对象的全部绑定。`SetReadyToDestroy` + `MarkAsGarbage`：异步节点标准销毁双步——**忘记 EndTask 的后果**：UI 关了，监听还在广播（悬挂对象+内存泄漏）。
- **调用方**：SpellMenuWidgetController 关闭时对每个监听调 EndTask；技能栏 Widget 的 Destruct 里清理。

---

# 六、技能族逐个精讲

## 6.1 DuraProjectileSpell —— 投射物基类（SpawnActorDeferred 教科书）★★★★★

```cpp
void UDuraProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    const bool bIsServer = Avatar && Avatar->HasAuthority();
    if (!bIsServer) return;          // ① 投射物只在服务器生成

    if (Avatar->Implements<UCombatInterface>())
    {
        const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, SocketTag);
        FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();   // ② 朝目标
        if(bOverridePitch) Rotation.Pitch = PitchOverride;

        FTransform SpawnTransform;
        SpawnTransform.SetLocation(SocketLocation);
        SpawnTransform.SetRotation(Rotation.Quaternion());

        AActor* Owner = GetOwningActorFromActorInfo();
        ADuraProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADuraProjectile>(   // ③ 延迟生成
            ProjectileClass, SpawnTransform, Owner, Cast<APawn>(Owner),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();   // ④ 生成前注入参数

        if(!Projectile) return;
        Projectile->FinishSpawning(SpawnTransform);   // ⑤ 正式出生
    }
}
```

- **为什么 `SpawnActorDeferred` 而不是 `SpawnActor`（★★★★★ 面试必考）**：
  - SpawnActor：一步到位——构造、BeginPlay、注册渲染，出生即"活"。
  - **SpawnActorDeferred**：**先给半成品**——构造完成但 BeginPlay 未跑、渲染未注册，**允许调用方先改属性**（④ 注入伤害参数），然后 `FinishSpawning` 补完生命周期。
  - **为什么不先 Spawn 再改**：Projectile 的 BeginPlay 里会读 DamageEffectParams（注册碰撞回调、初始化移动组件依赖参数）——**参数必须在 BeginPlay 之前就位**，Deferred 模式唯一可行。
- **参数逐个**：类、变换、`Owner`（归谁——网络所有权/日志归因）、`Cast<APawn>(Owner)`（Instigator——"谁发的"引擎语义）、`AlwaysSpawn`（碰撞处理：无视出生点重叠，照样生成——防止敌人贴脸发弹时出生体积重叠导致生成失败）。
- **朝向计算**：`目标点 - 发射点` 的 Rotation——**从插槽出发指向目标**（不是从角色中心），弹道视觉正确。
- **`ProjectileClass`** 是 EditAnywhere——子类蓝图 GA_FireBolt 配 BP_FireBolt，敌人技能配敌人的弹。

## 6.2 DuraFireBolt —— 多弹道与追踪（★★★★☆）

```cpp
void UDuraFireBolt::SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag,
    bool bOverridePitch, float PitchOverride, AActor* HomingTarget)
{
    ...（同上：服务器检查、Socket、方向）
    const FVector Forward = Rotation.Vector();
    int32 EffectiveNumProjectiles = FMath::Min(NumProjectiles, GetAbilityLevel());   // ① 等级决定弹数

    TArray<FRotator> Rotators = UDuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 
        ProjectileSpread, EffectiveNumProjectiles);   // ② 扇形均布（第 5 篇工具）

    for (const FRotator& Rot : Rotators)
    {
        ...SpawnActorDeferred...
        Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

        if(IsValid(HomingTarget) && HomingTarget->Implements<UCombatInterface>())
        {
            Projectile->ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();   // ③a 真目标追踪
        }
        else
        {
            Projectile->HomingTargetSceneComponent = NewObject<USceneComponent>(USceneComponent::StaticClass());
            Projectile->HomingTargetSceneComponent->SetWorldLocation(ProjectileTargetLocation);
            Projectile->ProjectileMovement->HomingTargetComponent = Projectile->HomingTargetSceneComponent;   // ③b 虚拟追踪点
        }
        Projectile->ProjectileMovement->HomingAccelerationMagnitude = FMath::FRandRange(HomingAccelerationMin, HomingAccelerationMax);
        Projectile->ProjectileMovement->bIsHomingProjectile = bLaunchHomingProjectiles;
        Projectile->FinishSpawning(SpawnTransform);
    }
}
```

- **`Min(NumProjectiles, Level)`**：等级 1=1 发、3=3 发、5+=5 发封顶——**数值成长与上限**。
- **虚拟追踪点（③b 的精妙处 ★）**：`ProjectileMovementComponent::HomingTargetComponent` 要求**组件**——点击地面施法时没有"目标组件"（地面不是 Actor），于是**运行时 NewObject 一个空 SceneComponent 放在命中点**充当追踪标的。**空组件当虚拟锚点**——引擎 API 的适配技巧。注意 NewObject 的 Outer 默认为当前（技能），弹丸销毁后该组件也会被 GC（弹丸持有引用）。
- **`HomingAccelerationMagnitude = FRandRange(1600, 3200)`**：每发弹的追踪加速度**随机**——多弹不会整齐叠成一条线（自然散布），表现力细节。
- **HomingTarget 有效时的判定**：`Implements<UCombatInterface>`——只有"战斗单位"才当真目标（点到一个宝箱上也用虚拟点）。

## 6.3 DuraFireBlast —— 360° 弹幕（★★★☆☆）

```cpp
TArray<ADuraFireBall*> UDuraFireBlast::SpawnFireBalls()
{
    ...
    APawn* InstigatorPawn = (CurrentActorInfo && CurrentActorInfo->PlayerController.IsValid())
        ? CurrentActorInfo->PlayerController->GetPawn()
        : nullptr;   // 非玩家施放时退化为 nullptr

    TArray<FRotator> Rotators = UDuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 360.f, NumFireBalls);
    ...
    FireBall->ReturnToActor = Avatar;                       // 归宿：施法者
    FireBall->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();       // 直接伤害参数
    FireBall->ExplosionDamageParams = MakeDamageEffectParamsFromClassDefaults();    // 爆炸伤害参数
    ...
}
```

- **`360.f` 扇形 + 12 发**：整圆弹幕（Spread=360 时 EvenlySpaced 退化为圆周均布）。
- **`ReturnToActor`**：DuraFireBall 飞出去→调头飞回施法者→**归途爆炸**（ExplosionDamageParams 是**径向伤害**参数——`MakeDamageEffectParamsFromClassDefaults` 在蓝图里配 bIsRadialDamage）。
- **两套伤害参数**：命中途中敌人的直伤 + 归途爆炸的范围伤——**一次 Spawn 两次伤害语义**，故 Params 填两份。
- **Instigator 的退化处理**：CurrentActorInfo->PlayerController 对敌人是无效的（AI 没有 PC）——注释"非玩家施放时退化为用 Avatar 自身作为 Instigator"。**防御性解构 ActorInfo**。

## 6.4 ArcaneShards / Electrocute —— 纯描述类（★★☆☆☆）

两个类 C++ 里只有 **GetDescription/GetNextLevelDescription**（富文本模板）+ 数据成员（MaxNumShards/继承的 MaxNumShockTargets）。**激活逻辑全在蓝图**（GA_ArcaneShards：ShowMagicCircle → TargetDataUnderMouse → 等玩家点击 → 生成 N 个碎片 → 每个碎片延迟引爆径向伤害）。

- **C++/蓝图分工的教学样本**：**描述/数据在 C++（编译期安全+全技能统一格式），演出逻辑在蓝图（策划/TA 可迭代）**。Electrocute 继承 BeamSpell 复用链电目标选择。

## 6.5 DuraBeamSpell —— C++ 提供步骤函数、蓝图编排（★★★★☆）

```cpp
void UDuraBeamSpell::StoreMouseDataInfo(const FHitResult& HitResult)
{
    if(HitResult.bBlockingHit)
    {
        MouseHitLocation = HitResult.ImpactPoint;
        MouseHitActor = HitResult.GetActor();
    }
    else
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);   // 点空了直接取消
    }
}

void UDuraBeamSpell::TraceFirstTarget(const FVector& BeamTargetLocation)
{
    ...
    UKismetSystemLibrary::SphereTraceSingle(OwnerCharacter, SocketLocation, BeamTargetLocation, 10.f, 
        TraceTypeQuery1, false, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);
    ...
    if(ICombatInterface* CombatInterface = Cast<ICombatInterface>(MouseHitActor))
    {
        if(!CombatInterface->GetOnDeathDelegate().IsAlreadyBound(this, &UDuraBeamSpell::PrimaryTargetDied))
        {
            CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDuraBeamSpell::PrimaryTargetDied);
        }
    }
}
```

- **C++ 步骤函数族**：StoreMouseDataInfo（存鼠标数据，点空则 CancelAbility）/StoreOwnerVariables（缓存 Avatar/PC）/TraceFirstTarget（从武器 TipSocket 到鼠标点做**球体扫描**找主目标）/StoreAdditionalTargets（半径 850 内找 N 个次目标）。
- **蓝图负责编排**：GA_Electrocute 蓝图里按顺序调这些函数+Period 定时伤害+Cue。
- **`IsAlreadyBound` + `AddDynamic`**：**重复订阅防护**——闪电链到同一目标两次（弹墙回弹等）时防止委托堆积。**动态委托的防重三件**：IsAlreadyBound 查重 → AddDynamic 绑定。
- **PrimaryTargetDied / AdditionalTargetDied（BlueprintImplementableEvent）**：主/次目标死亡时蓝图做"闪电转移目标/提前结束"演出——**死亡事件驱动技能状态**。
- **`CancelAbility(..., true)`（最后参 bReplicateCancel）**：技能取消并网络同步。
- **StoreAdditionalTargets 的防御**：`IsValid(MouseHitActor) ? MouseHitActor->GetActorLocation() : MouseHitLocation`——主目标可能已销毁，退化用命中点做扩散中心。

## 6.6 DuraPassiveAbility —— 被动的激活/停用回路（★★★★☆）

```cpp
void UDuraPassiveAbility::ActivateAbility(...)
{
    Super::ActivateAbility(...);
    if(UDuraAbilitySystemComponent* ASC = Cast<UDuraAbilitySystemComponent>(
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo())))
    {
        // InstancedPerActor 实例可能被重复激活，先解绑避免委托堆积
        ASC->DeactivatePassiveAbility.RemoveAll(this);
        ASC->DeactivatePassiveAbility.AddUObject(this, &UDuraPassiveAbility::ReceiveDeactivate);
    }
}

void UDuraPassiveAbility::ReceiveDeactivate(const FGameplayTag& AbilityTag)
{
    if(GetAssetTags().HasTagExact(AbilityTag))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}
```

- **被动技能的"待机"形态**：ActivateAbility 只做一件事——**挂监听然后不结束**（被动激活后一直活着，等被卸下）。GE/光环逻辑在蓝图侧（GA_HaloOfProtection 蓝图：ApplyGameplayEffect（永久光环 GE）+ 循环等待）。
- **停用回路**：ASC 装备新被动顶掉旧的 → `DeactivatePassiveAbility.Broadcast(旧Tag)` → 每个被动实例收到 → **检查是不是叫我**（`GetAssetTags().HasTagExact(AbilityTag)`）→ 是则 EndAbility。
- **RemoveAll+Add 防重**：InstancedPerActor 技能可以重复激活（卸了再装）——每次激活都 Add 会堆积回调。注释点明。
- **通俗解释**：被动技能像"站岗士兵"——上岗时（Activate）记住哨位，听到"撤岗哨令"（DeactivatePassiveAbility 广播）核对番号（Tag）后下岗（EndAbility）。

## 6.7 DuraSummonAbility —— 召唤位置计算（★★★☆☆）

```cpp
TArray<FVector> UDuraSummonAbility::GetSpawnLocations()
{
    ...
    const float DeltaSpread = SpawnSpread / NumMinions;
    const FVector LeftOfSpread = Forward.RotateAngleAxis(-SpawnSpread / 2.f, FVector::UpVector);
    TArray<FVector> SpawnLocations;
    for(int32 i = 0; i < NumMinions; i++)
    {
        const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
        FVector ChosenSpawnLocation = Location + Direction * FMath::FRandRange(MinSpawnDistnace, MaxSpawnDistnace);

        FHitResult hit;
        GetWorld()->LineTraceSingleByChannel(hit, 
            ChosenSpawnLocation + FVector(0.f, 0.f, 400.f), 
            ChosenSpawnLocation - FVector(0.f, 0.f, 400.f), ECC_Visibility);

        if(hit.bBlockingHit)
        {
            ChosenSpawnLocation = hit.ImpactPoint;   // 贴地
        }
        SpawnLocations.Add(ChosenSpawnLocation);
    }
    return SpawnLocations;
}
```

- **扇形位置**：与弹幕同款算法（但注意 `DeltaSpread = SpawnSpread / NumMinions` **没有 -1**！与 EvenlySpacedRotators 的 `/(N-1)` 不同——召唤是"从左到右含两端"的另一种等分语义，若想要均匀含首尾应该 /(N-1)。这是**两处算法不一致**的细节，学习时辨析）。
- **随机距离**：每召唤物距离 150~400 随机——**环形散布**（同方向不同距离）。
- **落地贴地**：从目标点上/下 400cm 各打一条竖直射线（ECC_Visibility），命中点作为出生点——**防止召唤物悬浮/入地**（斜坡上尤其重要）。
- **GetRandomMinionClass**：空数组的 RandRange(0,-1) 是未定义行为——源码注释点明"未配置召唤物类别时避免未定义行为与越界访问"，返回 nullptr。

## 6.8 PassiveNiagaraComponent —— "ASC 迟到"的双保险（★★★★☆）

```cpp
void UPassiveNiagaraComponent::BeginPlay()
{
    Super::BeginPlay();

    UDuraAbilitySystemComponent* OwnerASC = Cast<UDuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
    if(OwnerASC)
    {
        OwnerASC->ActivatePassiveEffect.AddUObject(this, &UPassiveNiagaraComponent::OnPassiveActivate);
        ActivateIfEquipped(OwnerASC);
    }
    else if(ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetOwner()))
    {
        CombatInterface->GetOnASCRegisteredDelegate().AddWeakLambda(this, [this](UAbilitySystemComponent* NewASC)
        {
            if(UDuraAbilitySystemComponent* ASC = Cast<UDuraAbilitySystemComponent>(NewASC))
            {
                ASC->ActivatePassiveEffect.AddUObject(this, &UPassiveNiagaraComponent::OnPassiveActivate);
                ActivateIfEquipped(ASC);
            }
        });
    }
}
```

- **时序问题**：组件 BeginPlay 时 Owner 的 ASC 可能**还没就绪**（玩家的 ASC 在 PlayerState，PossessedBy/OnRep 之后才 InitAbilityActorInfo）。**双保险**：
  1. 立刻查——查到直接绑+检查装备状态（ActivateIfEquipped：`bStartupAbilitiesGiven && 状态==Equipped → Activate()`——**开局已装备的被动直接亮光环**）。
  2. 查不到 → 订阅 `OnASCRegistered` 委托（第 2 篇 InitAbilityActorInfo 里广播的那个）——**ASC 就绪的瞬间**补绑。
- **`AddWeakLambda(this, ...)`**：弱 Lambda 绑在**接口委托**上（CombatInterface 的非动态委托），this 是组件——弱引用防"组件死了回调还来"。
- **组件自注册模式总结**：**"查一次 + 等委托"** 是所有"依赖 Owner 系统就绪"组件的通用范式（DebuffNiagaraComponent 同款）。

```cpp
void UDebuffNiagaraComponent::DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    // 短路求值：Owner 无效时不能继续调用其接口
    const bool bOwnerValidAndAlive = IsValid(GetOwner()) && GetOwner()->Implements<UCombatInterface>()
        && !ICombatInterface::Execute_IsDead(GetOwner());

    if(NewCount > 0 && bOwnerValidAndAlive) Activate();
    else Deactivate();
}
```

- **Tag 驱动特效**：`Debuff.Stun/Burn` Tag 数量 >0 且活着 → 播粒子。**短路求值顺序**（先 IsValid 再 Implements 再 IsDead）——**顺序错误会在空指针上调用接口**，注释点明。
- **死者保护**：死亡时 Tag 还在（尸体上 GE 未清）但 IsDead 拦住——粒子不播。加上 OnOwnerDeath（死亡委托）强制 Deactivate——**双保险**。

---

# 七、本章全链路总图（玩家按住 LMB 放火球，从按下到爆炸）

```
[输入] LMB Held（第 3 篇）→ PC 检查 Block → ASC->AbilityInputTagHeld(InputTag.LMB)
[ASC]  遍历 ActivatableAbilities → Spec.DynamicTags 含 LMB → 未激活 → TryActivateAbility
       └─ CanActivateAbility：Mana 够（Cost GE）？不在 CD（Cooldown GE 的 GrantedTag）？无 BlockTag？
[GA_FireBolt 蓝图]
       ├─ UTargetDataUnderMouse 任务
       │    ├─ 客户端：GetHitResultUnderCursor(ECC_Target) → FScopedPredictionWindow
       │    │          → ServerSetReplicatedTargetData(Handle, 预测键, Data) → 本地 ValidData
       │    └─ 服务器：AbilityTargetDataSetDelegate 注册 → Consume → ValidData
       ├─ ValidData（两端同步）→ MakeDamageEffectParamsFromClassDefaults（击退/死亡冲量方向）
       ├─ 消耗 GE（扣 Mana）→ 冷却 GE（挂 CoolDown.Fire.FireBolt）
       └─ SpawnProjectiles（服务器）→ SpawnActorDeferred → 注入 Params/Homing → FinishSpawning
[投射物]（第 12 篇）飞行 → 命中 → ApplyDamageEffect → ExecCalc（第 5 篇）→ AttributeSet（第 4 篇）
[冷却 UI] GA 激活时蓝图内 WaitCooldownChange → CooldownStart(剩余秒) → 技能栏转圈
          GE 过期 → Tag 归零 → CooldownEnd → 图标恢复
```

# 八、动手实验建议

1. 把 `AbilityInputTagHeld` 的 `TryActivateAbility` 挪到 Pressed 里，体验"点按即发"与"按住即发"的手感差异，并思考 Released 时长判断（第 3 篇）为什么与 Held 激活配合更自然。
2. 在 TargetDataUnderMouse 的两个分支打日志（`IsLocallyControlled` / OnTargetDataReplicatedCallback），PIE 双窗观察客户端发送与服务器接收的顺序，验证"数据先于 Activate"的竞态。
3. 给 UWaitCooldownChange 加一个"冷却中被再次监听"的场景（连续开合技能菜单），在 EndTask 缺失的情况下观察 UI 重复响应——体会异步节点清理纪律。

---

*下一篇：`07-数据驱动配置链路.md` —— AbilityInfo/AttributeInfo/CharacterClassInfo/LevelUpInfo/LootTiers 五大数据资产。*
