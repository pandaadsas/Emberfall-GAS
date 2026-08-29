# 08 · UI 与 WidgetController 链路 —— MVC 模式下的数据流

> **逻辑链路一句话概括**：
> `HUD 持有三类 WidgetController 的"工厂+缓存"` → `初始化时把四大件（PC/PS/ASC/AS）装进 Controller` → `Controller 做"一次绑定"（BindCallbacksToDependencies 订阅游戏系统委托）+ "一次广播"（BroadcastInitialValue 刷初始值）` → `Widget（蓝图）只管把 Controller 的广播画出来、把按钮点击转调回 Controller` → **UMG 与游戏系统彻底解耦**。
> **这是本项目 UI 架构的灵魂：控件是"哑巴"，逻辑全在 Controller。**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/UI/WidgetController/DuraWidgetController.h` / `.cpp` | Controller 基类：参数包、双指针、虚函数对 | ★★★★★ |
| 2 | `Source/Dura/Public/UI/HUD/DuraHUD.h` / `.cpp` | Controller 工厂 + Overlay 初始化 | ★★★★★ |
| 3 | `Source/Dura/Public/UI/UserWidget/DuraUserWidget.h` / `.cpp` | Widget 基类：SetWidgetController 钩子 | ★★★★☆ |
| 4 | `Source/Dura/Public/UI/WidgetController/DuraOverlayWidgetController.h` / `.cpp` | 主界面：血蓝/XP/消息/技能栏 | ★★★★★ |
| 5 | `Source/Dura/Public/UI/WidgetController/AttributeMenuWidgetController.h` / `.cpp` | 属性菜单：Tag→属性指针表消费 | ★★★★☆ |
| 6 | `Source/Dura/Public/UI/WidgetController/SpellMenuWidgetController.h` / `.cpp` | 技能菜单：最复杂的选择/装备状态机 | ★★★★★ |
| 7 | `Source/Dura/Public/UI/UserWidget/DamageTextComponent.h` | 伤害数字组件（纯蓝图实现） | ★★★☆☆ |

配套蓝图：`BP_DuraHUD`（配 Widget 类与 Controller 类）、`WBP_Overlay` / `WBP_AttributeMenu` / `WBP_SpellMenu`。

---

# 一、先讲清楚：为什么要多一个 WidgetController 层（★★★★★ 架构题）

**没有 Controller 的做法**（大多数教程的做法）：WBP_Overlay 蓝图里直接 `GetPlayerState → GetAbilitySystemComponent → GetGameplayAttributeValueChangeDelegate(Health) → BindEventToSetPercent`。

**问题**：
1. **UMG 蓝图与游戏类强耦合**——换个游戏（或换项目复用血条）就要重写；
2. **绑定逻辑散落在各控件里**——谁绑了什么无从追踪，重复绑定/忘解绑；
3. **控件无法单元测试**——它依赖整个世界。

**MVC 解法**（本项目）：
- **Model（模型）**：ASC/AttributeSet/PlayerState——游戏数据与规则。
- **View（视图）**：UUserWidget——纯展示，数据从哪来不知道。
- **Controller（控制器）**：UObject 子类——**把 Model 的"变化事件"翻译成 View 能理解的"展示信号"**，并接收 View 的操作转译给 Model。

- **通俗解释**：餐厅里 View 是传菜员（只管端盘子），Model 是后厨（只管做菜），Controller 是服务员——听后厨喊"菜好了"（订阅委托），通知传菜员上菜（广播给蓝图）；顾客点单（蓝图按钮）也由服务员转给后厨（Server RPC）。传菜员永远不用进后厨。

---

# 二、FWidgetControllerParams 与双指针设计 ★★★★★

## 2.1 参数包

```cpp
USTRUCT(BlueprintType)
struct FWidgetControllerParams
{
    FWidgetControllerParams() {}

    FWidgetControllerParams(APlayerController* pc, APlayerState* ps, UAbilitySystemComponent* asc, UAttributeSet* as)
        : PC(pc), PS(ps), ASC(asc), AS(as) {}

    UPROPERTY() TObjectPtr<APlayerController> PC;
    UPROPERTY() TObjectPtr<APlayerState> PS;
    UPROPERTY() TObjectPtr<UAbilitySystemComponent> ASC;
    UPROPERTY() TObjectPtr<UAttributeSet> AS;
};
```

- **四件套一次交付**：任何 UI 想要"游戏数据"都逃不出这四样（谁在玩/他的档案/他的能力系统/他的属性）。
- **`TObjectPtr` + UPROPERTY**：GC 可见——Controller 是 UObject，裸指针成员不标 UPROPERTY 会被 GC 回收（悬垂）。**UObject 成员指针必标 UPROPERTY 或用 TWeakObjectPtr**。
- **两参构造函数**（默认+全参）：调用方两种写法都顺手（`FWidgetControllerParams Params(PC, PS, ASC, AS)` 一行构造）。
- **回顾第 2 篇**：`HUD->InitOverlay(PC, PS, ASC, AS)` 在 InitAbilityActorInfo 尾部调用——**四大件的汇聚点正是 ASC 初始化完成那一刻**（时序闭环）。

## 2.2 基类的"双列指针"与惰性 Cast

```cpp
protected:
    UPROPERTY(BlueprintReadOnly, Category="WidgetController")
    TObjectPtr<APlayerController> PlayerController;      // 引擎基类型
    ...
    UPROPERTY(BlueprintReadOnly, Category="WidgetController")
    TObjectPtr<ADuraPlayerController> DuraPlayerController;   // 项目子类型
    ...

    ADuraPlayerController* GetDuraPC()
    {
        if(!DuraPlayerController)
        {
            DuraPlayerController = Cast<ADuraPlayerController>(PlayerController);
        }
        return DuraPlayerController;
    }
    // GetDuraPS / GetDuraASC / GetDuraAS 同构
```

- **为什么两列**：`PlayerController` 列喂给**蓝图**（蓝图拖不拖拽都通用）；`GetDuraPC()` 是 **C++ 内部**用——需要项目特有接口（PlayerState 的委托、ASC 的自定义委托）时 Cast 成子类。**缓存 Cast 结果**（判空再 Cast 一次）避免每次函数调用都反射转换。
- **BlueprintReadOnly**：蓝图能读到 Controller 自身引用（用于蓝图里再 Cast），但改不了。

## 2.3 虚函数对：初始化的两步曲

```cpp
UFUNCTION(BlueprintCallable)
virtual void BroadcastInitialValue();     // ① 一次性广播当前值（快照）

virtual void BindCallbacksToDependencies();   // ② 订阅数据源（建立持续更新）
```

- **为什么是两个函数、调用顺序是谁定**：HUD 工厂里 `SetWidgetControllerParams → BindCallbacksToDependencies`，InitOverlay 里 `SetWidgetController → BroadcastInitialValue`。**顺序铁律**：**先绑定、后广播**——反过来会丢事件（广播发生时监听还没建立，之后数据再也不变就永远黑屏）。
- **BroadcastInitialValue 的必要性**：委托只在"变化"时触发——刚进游戏血是满的且没变化，UI 需要一次"主动汇报"。**订阅 + 快照** 双管齐下才能保证 UI 从第一帧起就正确（第 2 篇敌人血条的手动首播同理）。

---

# 三、DuraHUD —— Controller 工厂 ★★★★★

```cpp
UDuraOverlayWidgetController* ADuraHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
    if (OverlayWidgetController == nullptr)
    {
        OverlayWidgetController = NewObject<UDuraOverlayWidgetController>(this, OverlayWidgetControllerClass);
        OverlayWidgetController->SetWidgetControllerParams(WCParams);
        OverlayWidgetController->BindCallbacksToDependencies();
    }
    return OverlayWidgetController;
}
```

- **三重职责封装**：**懒创建**（只造一次）、**参数注入**、**立即绑定**。调用方（蓝图的 GetOverlayWidgetController Library 函数，第 5 篇）不需要知道初始化细节——**"给我一个就绪的 Controller"**。
- **`NewObject<...>(this, OverlayWidgetControllerClass)`**：第二参是**类模板**（蓝图子类 BP_DuraOverlayWidgetController）——C++ 类型 + 蓝图覆写的组合。Outer=HUD（跟随 HUD 生命周期）。
- **三个 Controller 各配一个 Class 属性**（蓝图里可换实现）——**控制器也可被蓝图继承定制**。
- **为什么 Controller 由 HUD 管**：HUD 是 PC 的附属（每玩家一份、与视口同生命周期），Controller 数量少且全局唯一——挂在 HUD 上是"天然的拥有者"。

## 3.1 InitOverlay —— 主界面装配线

```cpp
void ADuraHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
    checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DuraHUD"));
    checkf(OverlayWidgetControllerClass, TEXT("..."));

    OverlayWidget = CreateWidget<UDuraUserWidget>(GetWorld(), OverlayWidgetClass);

    const FWidgetControllerParams Params(PC, PS, ASC, AS);
    UDuraOverlayWidgetController* Controller = GetOverlayWidgetController(Params);

    OverlayWidget->SetWidgetController(Controller);

    Controller->BroadcastInitialValue();

    OverlayWidget->AddToViewport();
}
```

**五步装配线（顺序敏感，每步都有理由）**：
1. `checkf` 双保险：蓝图漏配类，报错信息直接告诉你填哪里（checkf 的消息字符串是"配置文档"）。
2. **CreateWidget**：UMG 实例化（此时控件还没进视口、WidgetControllerSet 还没触发——**先建后连**）。
3. **GetOverlayWidgetController**：工厂内完成参数注入+委托绑定（此阶段 UI 还没接上，广播不会被收到——没关系，快照在第 5 步前由下面补）。
4. **SetWidgetController**：控件侧拿到 Controller → 触发 `WidgetControllerSet` 蓝图事件（控件在这里 Grab 绑定 Controller 的委托，**蓝图里 bind event 到 delegate 就在此刻**）。
5. **BroadcastInitialValue**：快照广播（此时蓝图绑定已就位，血条/蓝条一次刷到位）→ **AddToViewport** 最后上屏（数据就绪才可见——避免"半成品闪烁"）。

- **面试点**：*"为什么 BroadcastInitialValue 在 AddToViewport 之前？"* —— 数据先行，展示殿后；视口出现的第一帧就是完整状态。

---

# 四、DuraUserWidget —— 控件基类 ★★★★☆

```cpp
UCLASS()
class UDuraUserWidget : public UUserWidget
{
public:
    UFUNCTION(BlueprintCallable)
    void SetWidgetController(UObject* InWidgetController);

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UObject> WidgetController;

protected:
    UFUNCTION(BlueprintImplementableEvent)
    void WidgetControllerSet();
};

void UDuraUserWidget::SetWidgetController(UObject* InWidgetController)
{
    WidgetController = InWidgetController;
    WidgetControllerSet();
}
```

- **Controller 类型是 UObject**：**不引用具体 Controller 类**——同一个控件基类可接任何 Controller（子类 WBP 里再 Cast 成具体类型取特定委托）。**反向解耦**：View 只认 Controller 协议，不知道实现。
- **`WidgetControllerSet()`（BlueprintImplementableEvent）**：**"接线时刻"钩子**——蓝图子类在收到这个事件时把 Controller 的委托 bind 到自己的函数（`Bind Event to OnHealthChanged`）。C++ 调完 SetWidgetController，蓝图立刻有机会接线——**C++ 驱动蓝图初始化时机**。
- **消费者**：WBP_Overlay/WBP_AttributeMenu/WBP_SpellMenu 都继承它；连**敌人血条**（DuraEnemy 的 HealthBar）也用它——SetWidgetController(敌人自己)（第 2 篇）。

---

# 五、DuraOverlayWidgetController —— 主界面控制器 ★★★★★

## 5.1 FUIWidgetRow 与 DataTable

```cpp
USTRUCT(BlueprintType)
struct FUIWidgetRow : public FTableRowBase
{
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag MessageTag;        // Message.HealthPotion 等
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Message;                  // "拾取了生命药水"
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UDuraUserWidget> MessageWidget;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UTexture2D* Image;
};
```

- **`FTableRowBase`**：**DataTable 行基类**——这个结构体可以直接当 DataTable 的行类型（编辑器建 DataTable → 选 FUIWidgetRow → 填行）。**DataAsset 存对象引用（UDataAsset），DataTable 存行列数据（可 Excel 导入）**——消息这类"纯文案+图标"用 DataTable 更顺手。
- **消费链**：ASC 的 EffectAssetTags（第 6 篇：GE 应用的资产标签广播）→ Controller 筛选 Message 开头的 Tag → 查表 → 广播行 → 蓝图弹 Toast 提示。

```cpp
template<typename T>
T* GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag)
{
    return DataTable->FindRow<T>(Tag.GetTagName(), TEXT(""));
}
```

- **模板 + `FindRow<T>(行名, 上下文)`**：`Tag.GetTagName()` 把 Tag 转回 FName 当行名——**Tag 名与表行名一一对应**的约定。模板放头文件 inline（模板必须对编译器可见）。第二参是错误上下文字符串（查找失败日志里显示）。

## 5.2 BindCallbacksToDependencies —— 四路绑定

```cpp
void UDuraOverlayWidgetController::BindCallbacksToDependencies()
{
    // ① PlayerState 的 XP/等级委托
    GetDuraPS()->OnXPChangedDelegate.AddUObject(this, &UDuraOverlayWidgetController::OnXPChanged);
    GetDuraPS()->OnLevelChangedDelegate.AddLambda([](int32 NewLevel, bool bLevelUp){ OnPlayerLevelChangedDelegate.Broadcast(...); });

    // ② ASC 的属性变化委托（血/蓝/上限四个）
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AS->GetHealthAttribute()).
        AddLambda([this](const FOnAttributeChangeData& Data){ OnHealthChanged.Broadcast(Data.NewValue);});
    ...×4...

    // ③ ASC 的技能装备委托
    GetDuraASC()->AbilityEquipped.AddUObject(this, &UDuraOverlayWidgetController::OnAbilityEquipped);

    // ④ 初始技能授予信号（时序敏感！）
    if(GetDuraASC()->bStartupAbilitiesGiven)
    {
        BroadcastAbilityInfo();       // 已授予 → 直接广播
    }
    else
    {
        GetDuraASC()->AbilitiesGivenDelegate.AddUObject(this, &UDuraOverlayWidgetController::BroadcastAbilityInfo);
    } 

    // ⑤ GE 资产标签 → 消息行
    GetDuraASC()->EffectAssetTags.AddLambda([this](const FGameplayTagContainer& AssetTags)
    {
        for (const FGameplayTag& Tag : AssetTags)
        {
            FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message"));
            if (Tag.MatchesTag(MessageTag) && MessageWidgetDataTable)
            {
                const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag);
                if(Row) MessageWidgetRowDelegate.Broadcast(*Row);   // 缺行静默跳过
            }
        }
    });
}
```

- **绑定工具的选择**：成员函数用 `AddUObject`（生命周期安全，对象销毁自动断开——UObject 委托的 GC 感知）；Lambda 用 `AddLambda`（捕获 this——**注意这里没有 WeakLambda**，Controller 与 ASC 同寿（都归 HUD），无悬垂风险；若 Controller 可能先亡则应 AddWeakLambda）。
- **④ 的时序分支（★★★★☆）**：Controller 初始化可能早于/晚于"技能授予完成"——**两种可能都处理**：标志位已置 → 立即广播；未置 → 订阅广播信号。这就是"**事件 + 状态快照**"双保险的又一次应用（与 InitAbilityActorInfo 双轨、敌人血条首播同一设计哲学——**枚举所有时序可能性，每条都正确**）。
- **⑤ 的细节**：`MatchesTag(MessageTag)` **非 Exact**——`Message.HealthPotion` 前缀匹配 "Message" 根；`FindRow` 可能返回空（表没配这行）→ 静默跳过（注释"数据表缺少对应 Message 行时静默跳过"——可加日志）。

## 5.3 BroadcastInitialValue 与 OnXPChanged

```cpp
void UDuraOverlayWidgetController::BroadcastInitialValue()
{
    UDuraAttributeSet* AS = CastChecked<UDuraAttributeSet>(AttributeSet);
    OnHealthChanged.Broadcast(AS->GetHealth());
    OnMaxHealthChanged.Broadcast(AS->GetMaxHealth());
    OnManaChanged.Broadcast(AS->GetMana());
    OnMaxManaChanged.Broadcast(AS->GetMaxMana());
}
```
- 四连广播快照。**没有广播 XP/等级**——XP 条/等级的初始值由蓝图直接从 PlayerState 读（或首次变化触发）——**快照范围按需**（不全广播也行，但要与蓝图端约定一致）。

```cpp
void UDuraOverlayWidgetController::OnXPChanged(int32 NewXP) 
{
    const ULevelUpInfo* LevelUpInfo = GetDuraPS()->LevelUpInfoDataAsset;
    checkf(LevelUpInfo, TEXT("Unabled to find LevelUpInfo. Please fill out in DuraPlayerState Blueprint"));

    const int32 Level = LevelUpInfo->FindLevelForXP(NewXP);
    const int32 MaxLevel = LevelUpInfo->LevelUpInformation.Num();

    if(Level <= MaxLevel && Level > 0)
    {
        const int32 LevelUpRequirement = LevelUpInfo->LevelUpInformation[Level].LevelUpRequirement;
        const int32 PrevioutLevelUpRequirement = LevelUpInfo->LevelUpInformation[Level - 1].LevelUpRequirement;

        const int32 DeltaLevelRequirement = LevelUpRequirement - PrevioutLevelUpRequirement;       
        const int32 XPForThisLevel = NewXP - PrevioutLevelUpRequirement;

        const float XPBarPercent = static_cast<float>(XPForThisLevel) / static_cast<float>(DeltaLevelRequirement);
        OnXPPercentChangedDelegate.Broadcast(XPBarPercent);
    }
}
```

- **XP 条进度公式**：`进度 = (当前XP - 本级起始累计) / (本级门槛 - 上级门槛)`——**累计值转段内百分比**。第 7 篇的 LevelUpRequirement（累计）在这里被换算。
- **`static_cast<float>(int)/static_cast<float>(int)`**：两显式转换防整数除法截断（不转的话 int/int=0——经典陷阱，显式转换是自文档）。
- **Controller 做"数据整形"**：PlayerState 给"裸 XP"，Controller 换算成"0~1 百分比"再广播——**View 收到的永远是"展示态"数据**（这就是 Controller 存在的第三重意义：翻译/整形）。

## 5.4 OnAbilityEquipped —— 双广播的清屏逻辑

```cpp
void UDuraOverlayWidgetController::OnAbilityEquipped(...) const
{
    FDuraAbilityInfo LastSlotInfo;
    LastSlotInfo.StatusTag = GameplayTags.Abilities_Status_UnLocked;
    LastSlotInfo.InputTag = PrevSlot;
    LastSlotInfo.AbilityTag = GameplayTags.Abilities_None;
    //若 PrevSlot 是有效槽位则广播空信息（用于清空已装备技能的旧槽位显示）
    AbilityInfoDelegate.Broadcast(LastSlotInfo);

    FDuraAbilityInfo Info = AbilityInfoDataTable->FindAbilityInfoForTag(AbilityTag);
    Info.StatusTag = StatusTag;
    Info.InputTag = Slot;
    AbilityInfoDelegate.Broadcast(Info);
}
```

- **第一次广播**：一个 `Abilities_None` + 旧槽位的"空信息"——技能栏收到后**清空旧槽图标**（因为技能搬家了）。**用"空行广播"代替专门的清屏委托**——数据流只有一条（AbilityInfoDelegate），View 只处理一种消息。**单通道设计**的取舍：多一种消息类型 vs 少一条委托线。本项目选了前者（AbilityInfoDelegate 广播一切技能格变化）。

---

# 六、AttributeMenuWidgetController ★★★★☆

## 6.1 消费 TagsToAttributes（第 4 篇函数指针表的兑现）

```cpp
void UAttributeMenuWidgetController::BroadcastInitialValue()
{
    check(AttributeInfo);
    for (auto& Pair : GetDuraAS()->TagsToAttributes)
    {
        BroadcastAttributeInfo(Pair.Key, Pair.Value());    // Pair.Value() = 调用函数指针
    }
    AttributePointsChangedDelegate.Broadcast(GetDuraPS()->GetAttributePoints());
}

void UAttributeMenuWidgetController::BindCallbacksToDependencies()
{
    for (auto& Pair : GetDuraAS()->TagsToAttributes)
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Pair.Value()).AddLambda(
            [this, Pair](const FOnAttributeChangeData& Data)
            {
                BroadcastAttributeInfo(Pair.Key, Pair.Value());
            }
        );
    }
    ...
}
```

- **`Pair.Value()`**：TMap 的值是 `TStaticFuncPtr<FGameplayAttribute()>`——**加括号调用**得到 FGameplayAttribute 句柄。**快照与订阅都用同一表达式**——一致。
- **Lambda 捕获 Pair**：`[this, Pair]` **按值捕获** TMap 的键值对（拷贝一份进 Lambda）——**必须按值**！按引用捕获循环变量（`[&Pair]`）在循环结束后悬垂（第 3 篇 InputTag 同款陷阱）。**每条属性委托各自带着自己的 Tag 拷贝**，回调时正确广播对应属性。
- **BroadcastAttributeInfo**：

```cpp
void UAttributeMenuWidgetController::BroadcastAttributeInfo(const FGameplayTag& AttributeTag, const FGameplayAttribute& Attribute) const
{
    FDuraAttributeInfo Info = AttributeInfo->FindAttributeInfoForTag(AttributeTag);
    Info.AttributeValue = Attribute.GetNumericValue(AttributeSet);
    AttributeInfoDelegate.Broadcast(Info);
}
```

- **三表合流的时刻**：`FindAttributeInfoForTag`（第 7 篇文案表）+ `Attribute.GetNumericValue(AttributeSet)`（第 4 篇属性值）→ 组装成完整行 → 广播。**Controller 是数据的"装配车间"**。
- **UpgradeAttribute**：一行转调 `GetDuraASC()->UpgradeAttribute(AttributeTag)`（第 6 篇的 Server RPC 链）——View → Controller → ASC → Server 的完整操作链。

---

# 七、SpellMenuWidgetController —— 选择/装备状态机（★★★★★ 本篇最复杂）

## 7.1 状态成员

```cpp
struct FSelectedAbility
{
    FGameplayTag AbilityTag;    // 当前选中的技能
    FGameplayTag Status;        // 当前选中的技能状态
};

FSelectedAbility SelectedAbility = {Abilities_None, Abilities_Status_Locked};  // 初始=没选中
int CurrentSpellPoints = 0;
bool bWaitingForEquipSelection = false;   // 正在等待玩家点槽位
FGameplayTag SelectedSlot;                 // 已装备技能自身的槽位（用于"点击原槽=卸下"）
```

- **交互状态全在 Controller**（View 是无状态的渲染器）——**状态与展示分离的实践**。

## 7.2 SpellGlobeSelected —— 选中技能

```cpp
void USpellMenuWidgetController::SpellGlobeSelected(const FGameplayTag& AbilityTag)
{
    if(bWaitingForEquipSelection && AbilityInfoDataTable)
    {
        const FGameplayTag SelectedAbilityType = AbilityInfoDataTable->FindAbilityInfoForTag(AbilityTag).AbilityType;
        StopWaitForEquipDelegate.Broadcast(SelectedAbilityType);
        bWaitingForEquipSelection = false;     // 装备等待中途改选 → 取消等待态
    }

    const int32 SpellPoints = GetDuraPS()->GetSpellPoints();
    const bool bTagValid = AbilityTag.IsValid();
    const bool bTagNone = AbilityTag.MatchesTag(GameplayTags.Abilities_None);
    FGameplayAbilitySpec* AbilitySpec = GetDuraASC()->GetSpecFromAbilityTag(AbilityTag);
    const bool bSpecValid = AbilitySpec != nullptr;

    FGameplayTag AbilityStatus;
    if(!bTagValid || bTagNone || !bSpecValid)
    {
        AbilityStatus = GameplayTags.Abilities_Status_Locked;   // 无效/无 Spec → 锁定
    }
    else
    {
        AbilityStatus = GetDuraASC()->GetStatusFromSpec(*AbilitySpec);
    }

    SelectedAbility.AbilityTag = AbilityTag;
    SelectedAbility.Status = AbilityStatus;
    ...ShouldEnableButtons + 描述查询 + 广播...
}
```

- **状态推导四分支**：无效 Tag / None / 无 Spec → Locked（**点击空技能格或锁定格的安全兜底**）；有 Spec → 查真实状态。
- **每次选中都全量广播按钮可用性+描述**——View 不需要 diff（收到啥画啥），**逻辑集中、渲染傻瓜**。

## 7.3 ShouldEnableButtons —— 状态×资源的按钮矩阵

```cpp
void USpellMenuWidgetController::ShouldEnableButtons(const FGameplayTag& AbilityStatus, int32 SpellPoints, 
    bool& bShouldEnableSpellPointsButton, bool& bShouldEnableEquipButton)
{
    if(AbilityStatus.MatchesTagExact(Abilities_Status_Eligible))
    {
        if(SpellPoints > 0) bShouldEnableSpellPointsButton = true;      // 可学+有点 → 学习可点
    }
    else if(AbilityStatus.MatchesTagExact(Abilities_Status_Equipped))
    {
        bShouldEnableEquipButton = true;                                 // 已装备 → 装备可点（=换槽/卸下）
        if(SpellPoints > 0) bShouldEnableSpellPointsButton = true;      // +有点 → 升级可点
    }
    else if(AbilityStatus.MatchesTagExact(Abilities_Status_UnLocked))
    {
        bShouldEnableEquipButton = true;                                 // 已解锁 → 装备可点
    }
    // Locked：两个都 false
}
```

- **二维决策表**（状态 × 点数）输出两个按钮的可用性——**策略集中在私有函数**，UI 按钮逻辑改这里一处。输出参数用引用（bool&）而非返回结构体——蓝图调用友好的风格。
- **状态机全图**（结合第 6 篇 ASC 的 ServerSpendSpellPoint/ServerEquipAbility）：

```
Locked ──(升级达 LevelRequirement)──► Eligible ──(SpendPoint)──► UnLocked ──(Equip)──► Equipped
                                                                                        │
                                            ┌───────────(Equip 到其他槽)───────────────┤
                                            ▼                                          │
                                         Equipped(新槽) ◄──────────────────────────────┘
              UnLocked ◄──(被顶掉)── 旧技能 ClearSlot
```

## 7.4 装备交互流程（EquipButtonPressed → SpellRowGlobePressed → OnAbilityEquipped）

```cpp
void USpellMenuWidgetController::EquipButtonPressed()
{
    const FGameplayTag& AbilityType = AbilityInfoDataTable->FindAbilityInfoForTag(SelectedAbility.AbilityTag).AbilityType;
    WaitForEquipDelegate.Broadcast(AbilityType);     // ① 通知 View 进入"选槽模式"（高亮同类槽）
    bWaitingForEquipSelection = true;

    const FGameplayTag SeletedStatus = GetDuraASC()->GetStatusFromAbilityTag(SelectedAbility.AbilityTag);
    if(SeletedStatus.MatchesTagExact(Abilities_Status_Equipped))
    {
        SelectedSlot = GetDuraASC()->GetSlotFromAbilityTag(SelectedAbility.AbilityTag);   // ② 记住原槽
    }
}

void USpellMenuWidgetController::SpellRowGlobePressed(const FGameplayTag& SlotTag, const FGameplayTag& AbilityType)
{
    if(!bWaitingForEquipSelection || !AbilityInfoDataTable) return;

    // 校验：攻击技能不能装被动槽
    const FGameplayTag& SelectedAbilityType = ...FindAbilityInfoForTag(SelectedAbility.AbilityTag).AbilityType;
    if(!SelectedAbilityType.MatchesTagExact(AbilityType)) return;

    GetDuraASC()->ServerEquipAbility(SelectedAbility.AbilityTag, SlotTag);   // ③ 服务器裁决
}
```

- **两阶段交互**：①点装备按钮 → 广播 WaitForEquip（View 高亮所有"同类型"槽位，AbilityType 参数让 View 过滤——被动槽不亮）→ ②玩家点某个槽 → 类型校验 → ServerEquipAbility。
- **服务器回执链**：ServerEquipAbility → ClientEquipAbility RPC → ASC 广播 AbilityEquipped → **两个 Controller 都收到**（SpellMenu 的 OnAbilityEquipped 处理菜单状态；Overlay 的 OnAbilityEquipped 更新技能栏）——**一个事件、多视图消费**。

```cpp
void USpellMenuWidgetController::OnAbilityEquipped(...)
{
    bWaitingForEquipSelection = false;                     // 退出等待态
    ...广播旧槽空信息（同 Overlay 双广播）...
    StopWaitForEquipDelegate.Broadcast(...);               // View 退出选槽高亮
    SpellGlobeReassignedDelegate.Broadcast(AbilityTag);    // 技能格"重新归属"信号
    GlobeDeSelect();                                       // 取消选中，复位面板
}
```

- **收尾五连**：状态复位、双广播、停止等待、重分配信号、取消选中——**UI 事务的完整回滚/复位**，与服务器端的装备逻辑（第 6 篇）严丝合缝。
- **面试点**：*"描述这个装备流程中客户端/服务器/两块 UI 的交互时序。"* —— 能按"点击→RPC→服务器处理→ClientRPC→双 Controller 广播→各 View 刷新"讲清楚就是理解了。

---

# 八、DamageTextComponent —— 极简组件 ★★★☆☆

```cpp
UCLASS()
class UDamageTextComponent : public UWidgetComponent
{
public:
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
    void SetDamageText(float Damage, bool bBlockedHit, bool bCriticalHit);
};
```

- **C++ 只声明、蓝图全实现**：继承 WidgetComponent（3D 空间挂 UMG 的组件——世界场景中的浮空控件），SetDamageText 是蓝图事件（WBP 里面做数字滚动/暴击变色/上浮消失动画）。
- **生命周期回顾**（第 3 篇）：ShowDamageNumber RPC 里 NewObject+RegisterComponent+Attach→Detach——**短命组件**，动画播完蓝图里 `DestroyComponent` 自杀。**为什么不用对象池**：教学项目从简；商业项目数字高频出现会池化——面试的"性能优化拓展"话题。

---

# 九、本章数据流总图

```
┌────────────────────────── Model 层（游戏数据）──────────────────────────┐
│  DuraPlayerState          DuraASC                     AttributeSet     │
│  OnXP/Level/Points*Delegate  EffectAssetTags/AbilitiesGiven/Equipped    │
│  (第2篇定义)                 AbilityStatusChanged        (第4篇)         │
└──────────┬──────────────────────┬─────────────────────────┬────────────┘
           │        BindCallbacksToDependencies 订阅          │
┌──────────▼──────────────────────▼─────────────────────────▼────────────┐
│  WidgetController 层（翻译/整形/状态机）                                    │
│  基类：FWidgetControllerParams + 双指针 + 虚函数对                          │
│  Overlay：血蓝属性→百分比   XP→段内进度   EffectTag→查表→消息行              │
│  AttributeMenu：TagsToAttributes→查文案表→整行广播                        │
│  SpellMenu：选中/装备状态机 → Server RPC 转发                              │
└──────────┬─────────────────────────────────────────────────────────────┘
           │  BlueprintAssignable 委托（BroadcastInitialValue 快照先行）
┌──────────▼─────────────────────────────────────────────────────────────┐
│  View 层（UMG）：WBP_Overlay / WBP_AttributeMenu / WBP_SpellMenu          │
│  WidgetControllerSet 接线 → 收到广播刷 UI → 按钮点击转调 Controller         │
└────────────────────────────────────────────────────────────────────────┘
装配时序：HUD::InitOverlay → CreateWidget → 工厂造Controller(绑定) → SetWidgetController(接线) → BroadcastInitialValue(快照) → AddToViewport
```

# 十、动手实验建议

1. 把 InitOverlay 里的 `BroadcastInitialValue()` 与 `AddToViewport()` 交换顺序，观察进图瞬间血条从 0 跳到满的闪烁——理解"数据先行"。
2. 在 AttributeMenuWidgetController 的 Lambda 里把 `[this, Pair]` 改成 `[this, &Pair]`，运行点开属性菜单再升级属性——观察错误的属性行刷新（悬垂引用读垃圾值）。
3. 给 Overlay 的消息系统新增一条 `Message.LevelUp` 行（DataTable + Tag），在升级 GE 里加该 AssetTag，验证"Tag → 查表 → Toast"全链路（串起第 1/6/7/8 篇）。

---

*下一篇：`09-存档与加载屏MVVM链路.md` —— USaveGame 体系、LoadScreen 的 MVVM 与槽位生命周期。*
