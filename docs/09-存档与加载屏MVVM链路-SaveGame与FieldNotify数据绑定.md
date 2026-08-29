# 09 · 存档与加载屏 MVVM 链路 —— USaveGame 体系与 FieldNotify 数据绑定

> **逻辑链路一句话概括**：
> `主菜单 HUD（LoadScreenHUD）在 BeginPlay 依次装配：ViewModel（3 个槽位 VM）→ 控件 → 从磁盘读回槽位状态` → `玩家点"新档"→ 槽位 VM 写 SaveGame 对象落盘；点"开始"→ GameInstance 记住槽位名/索引/出生点 → 跳图` → `游戏内所有读写都经 GameMode 的存档 API（第 2 篇），SaveGame 结构是全项目的"数据地契"`。
> **本篇新知识点：MVVM 模式（UMVVMViewModelBase + FieldNotify）——与第 8 篇 MVC 并排学习的第二种 UI 数据绑定范式。**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/Game/LoadScreenSaveGame.h` | 存档数据结构（槽位/技能快照/世界状态三级结构） | ★★★★★ |
| 2 | `Source/Dura/Public/UI/ViewModel/MVVM_LoadSlot.h` / `.cpp` | 槽位 ViewModel（FieldNotify 全家桶） | ★★★★★ |
| 3 | `Source/Dura/Public/UI/ViewModel/MVVM_LoadScreen.h` / `.cpp` | 加载屏 ViewModel（槽位管理/按钮状态） | ★★★★★ |
| 4 | `Source/Dura/Public/UI/HUD/LoadScreenHUD.h` / `.cpp` | 主菜单 HUD（装配时序） | ★★★★☆ |
| 5 | `Source/Dura/Public/UI/Widget/LoadScreenWidget.h` / `.cpp` | 加载屏控件（纯蓝图壳） | ★★★☆☆ |

回顾依赖：第 2 篇的 `DuraGameModeBase`（存档 CRUD/世界状态）、`DuraGameInstance`（跨图槽位信息）。

---

# 一、LoadScreenSaveGame —— 存档数据结构 ★★★★★

## 1.1 USaveGame 的本质

```cpp
UCLASS()
class DURA_API ULoadScreenSaveGame : public USaveGame
```

- `USaveGame` 是一个**空壳基类**（无任何逻辑）——它的意义是**约定**："这个类的 UPROPERTY 都可以被 `SaveGameToSlot` 序列化进磁盘"。序列化的内容 = 所有标记 UPROPERTY 的字段（可再用 `SaveGame` specifier 精选，见世界状态）。
- **槽位寻址**：`SaveGameToSlot(对象, SlotName, SlotIndex)` 的后两个参数决定磁盘文件名（`<SlotName>.sav`，index 是版本扩展位）——对象里的 `SlotName/SlotIndex` 成员是**冗余存储**（读回来才知道自己是谁，便于调试与改名）。

## 1.2 槽位状态机

```cpp
UENUM(BlueprintType)
enum ESaveSlotStatus : uint8
{
    Vacant,       // 空槽
    EnterName,    // 点了"新档"正在输入名字
    Taken         // 已有档
};
```

- **三态对应 UI 的 WidgetSwitcher 三个面板**（空槽显示"+NEW"、输入态显示文本框、有档显示玩家信息）——**枚举值即切换器索引**（`InitSlotStatus` 里 `WidgetSwitcherIndex = SlotStatus` 直接转换）。
- **语法细节（UE5.5+）**：`enum ESaveSlotStatus : uint8`——**不带 `class` 关键字的枚举**配合 `TEnumAsByte<ESaveSlotStatus>` 成员。这是引擎新式 UENUM 写法（老写法 `enum class + UPROPERTY(ESaveSlotStatus)` 在 5.5 后的反射里有限制）。**项目内两种用法并存**（这里 TEnumAsByte、第 2 篇 ECharacterClass 用 enum class）——读代码时注意区分。
- **状态流转**：`Vacant →(点 NEW)→ EnterName →(输入名字确认)→ Taken →(删除)→ Vacant`。

## 1.3 FSavedAbility —— 技能快照行

```cpp
USTRUCT(BlueprintType)
struct FSavedAbility
{
    UPROPERTY(...) TSubclassOf<UGameplayAbility> GameplayAbilityClass;
    UPROPERTY(...) FGameplayTag AbilityTag;
    UPROPERTY(...) FGameplayTag AbilityStatus;
    UPROPERTY(...) FGameplayTag AbilitySlot;
    UPROPERTY(...) FGameplayTag AbilityType;
    UPROPERTY(...) int32 AbilityLevel = 1;
};

inline bool operator==(const FSavedAbility& Left, const FSavedAbility& Right)
{
    return Left.AbilityTag.MatchesTagExact(Right.AbilityTag);
}
```

- **六字段 = 技能的完整身份**（类/身份 Tag/状态/槽位/类型/等级）——恢复时足以重建 Spec（第 6 篇 AddCharacterAbilitiesFromSaveData）。
- **`operator==` 按 AbilityTag**（inline 自由函数）：**USTRUCT 相等语义自定义**——`AddUnique`（第 2 篇 SaveProgress）依赖它去重：同 Tag 的技能视为同一条，重复保存不堆积。**为容器操作定制 ==** 是 USTRUCT 的惯用法（FSaveActor 也定义了，按 ActorName）。
- **为什么存 GameplayAbilityClass 又存 Tag**：类用于恢复 GiveAbility；Tag 用于**对账**（存档里的技能与数据资产比对）。**冗余是容错**。

## 1.4 世界状态三级结构

```cpp
USTRUCT(BlueprintType)
struct FSaveActor
{
    UPROPERTY() FName ActorName;        // Actor 对象名（匹配键）
    UPROPERTY() FTransform Transform;   // 位置快照
    UPROPERTY() TArray<uint8> Bytes;    // SaveGame 标记属性的序列化字节
};
// operator== 按 ActorName

USTRUCT(BlueprintType)
struct FSaveMap
{
    UPROPERTY() FString MapAssetName;
    UPROPERTY() TArray<FSaveActor> SavedActors;
};

UCLASS()
class ULoadScreenSaveGame : public USaveGame
{
    ...
    UPROPERTY() TArray<FSaveMap> SavedMaps;    // 地图 → Actor 列表
};
```

- **层级**：`存档 → SavedMaps[]（按地图） → SavedActors[]（按 Actor） → Transform + Bytes`。第 2 篇的 SaveWorldState/LoadWorldState 就是这三级的读写循环。
- **`Bytes` 的设计**：不是为每个可存 Actor 定义专用结构，而是**反射序列化的字节流**——新增"可存属性"（UPROPERTY(SaveGame)）**零存档结构改动**。这是"通用序列化"与"强类型结构"的取舍（字节流紧凑但不可读、跨版本字段变更依赖序列化兼容性）。

## 1.5 玩家数据字段

```cpp
    bool bFirstTimeLoadIn = true;     // "处女档"标志（LoadProgress 分流）
    FName PlayerStartTag;             // 出生点（检查点传递）
    int32 PlayerLevel / XP / SpellPoints / AttributePoints;
    float Strength / Intelligence / Resilience / Vigor;   // 四维（读档重算派生树的根）
    TArray<FSavedAbility> SavedAbilities;
    TArray<FSaveMap> SavedMaps;
```

- **`GetSavedMapWithMapName` / `HasMap`**：地图条目的查找/存在检查（第 2 篇 SaveWorldState 的"查到就替换"依赖它）。
- **全 UPROPERTY 无 SaveGame specifier**：整个对象都是存档用途，默认全序列化；对比世界状态的 Actor 侧（`Archive.ArIsSaveGame=true` 只存带 SaveGame 标记的属性）——**两种粒度策略并存**。

---

# 二、MVVM 模式先行课（★★★★★ 与第 8 篇 MVC 对照）

## 2.1 MVVM 是什么

- **Model**：磁盘存档（ULoadScreenSaveGame）——数据本体。
- **View**：WBP_LoadScreen 蓝图——展示。
- **ViewModel**：UMVVMViewModelBase 子类——**持有 View 要显示的数据（属性），数据变化时自动通知 View**。

**与 MVC（WidgetController）的本质区别**：

| | MVC（WidgetController，第 8 篇） | MVVM（本篇） |
|---|---|---|
| 数据流向 | Model →（委托）→ Controller →（动态委托）→ View 手动绑事件 | ViewModel 属性 →（**FieldNotify 自动绑定**）→ View 控件 |
| View 侧工作 | Bind Event to Delegate，手动 SetPercent | **绑定编辑器里拖一次**，属性变了 UI 自动跟 |
| 双向 | 单向（M→V 广播，V→M 调函数） | 数据绑定可双向（VM 属性 ⇄ 控件） |
| 适用 | 复杂事件流（GAS 满天飞的委托） | 表单型 UI（槽位/设置/背包格子） |

## 2.2 FieldNotify 三件套（UE5.0+ 的新机制）

```cpp
// ① 属性声明：FieldNotify + Setter + Getter
UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess="true"))
bool PlayButtonEnable = false;

// ② Setter 里用宏
void UMVVM_LoadScreen::SetPlayButtonEnable(bool InPlayButtonEnable)
{
    UE_MVVM_SET_PROPERTY_VALUE(PlayButtonEnable, InPlayButtonEnable);
}

// ③ 蓝图绑定：控件属性 → "按 FieldNotify 绑定" → 选 VM 的 PlayButtonEnable
```

- **`FieldNotify`**：给 UPROPERTY 加**字段变更通知**能力——属性被设置时发 `FieldChanged` 信号，**UMG 绑定系统监听它**自动刷新控件。
- **`UE_MVVM_SET_PROPERTY_VALUE(字段, 新值)`**：宏展开 ≈ `if (字段 != 新值) { 字段 = 新值; UE_MVVM_NOTIFY_FIELD_CHANGED(字段); }`——**值没变不通知**（去抖）、变了才广播。
- **`Setter, Getter` + `AllowPrivateAccess`**：UPROPERTY 声明指定访问器（蓝图走 Getter/Setter 路径），private 字段暴露给蓝图。**C++ 属性 + 自动通知 + 蓝图绑定 = 无手写委托的响应式 UI**。
- **通俗解释**：MVC 是"服务员每上一道菜都喊一嗓子（广播），桌子自己决定要不要听"；MVVM 是"桌子跟后厨签了自动传送带协议（FieldNotify 绑定），菜好了直接到桌上"。**绑定声明一次，永续自动**。

---

# 三、MVVM_LoadSlot —— 槽位 ViewModel ★★★★★

## 3.1 属性区（每个槽位的"名字牌"）

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess="true"))
int32 SlotStatus;
UPROPERTY(..., FieldNotify, ...) FString LoadSlotName;
UPROPERTY(..., FieldNotify, ...) FString PlayerName;
UPROPERTY(..., FieldNotify, ...) FString MapName;
UPROPERTY(..., FieldNotify, ...) int32 PlayerLevel;
UPROPERTY(..., FieldNotify, ...) FString EnterNameString = TEXT("Enter Name: ");
...NewSlotString / SelectSlotString / NewGameString（按钮文案，全部可本地化覆盖）...
UPROPERTY(..., FieldNotify, ...) bool SelectSlotButtonEnable = true;
```

- **按钮文案做成 FieldNotify 属性**：蓝图绑定 Text 块 → VM 改属性 → 按钮文字自动更新——**文案与布局解耦**（策划改默认文案/本地化都不动蓝图逻辑）。
- **非 FieldNotify 的普通 UPROPERTY**：`SlotIndex`（内部键，无需通知）、`PlayerStartTag`/`MapAssetName`（传输用，不显示）——**通知属性按需开启**（通知有开销：信号广播+蓝图绑定查找）。

## 3.2 交互函数（VM 侧的"半逻辑"）

```cpp
void UMVVM_LoadSlot::NewGameButtonPressed()
{
    SetSlotStatus(EnterName);      // 点 NEW → 进入输入名字面板
}

void UMVVM_LoadSlot::SelectSlotButtonPressed()
{
    SelectSlotButtonClick.Broadcast(this);   // 上抛给 LoadScreen VM（带 this）
}

void UMVVM_LoadSlot::NewSlotButtonPressed(const FString& EnteredPlayerName)
{
    ADuraGameModeBase* DuraGameMode = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this));
    if(!IsValid(DuraGameMode))
    {
        GEngine->AddOnScreenDebugMessage(1, 15.f, FColor::Magenta, TEXT("Please switch to Single Player"));
        return;      // 多人模式无 GameMode 语义，保护性退出
    }

    PlayerStartTag = DuraGameMode->DefaultPlayerStartTag;
    MapAssetName = DuraGameMode->DefaultMap.ToSoftObjectPath().GetAssetName();
    SetSlotStatus(Taken);
    SetPlayerName(EnteredPlayerName);
    SetMapName(DuraGameMode->DefaultMapName);
    SetPlayerLevel(1);

    DuraGameMode->SaveSlotData(this, SlotIndex);   // 落盘

    UDuraGameInstance* DuraGameInstance = DuraGameMode->GetGameInstance<UDuraGameInstance>();
    DuraGameInstance->LoadSlotName = GetLoadSlotName();
    DuraGameInstance->LoadSlotIndex = GetSlotIndex();
    DuraGameInstance->PlayerStartTag = DuraGameMode->DefaultPlayerStartTag;
}
```

- **NewSlotButtonPressed = 建档事务**：默认值组装（出生点/地图/等级 1）→ **SaveSlotData 落盘**（第 2 篇：DeleteSlot+CreateSaveGameObject+SaveGameToSlot）→ **GameInstance 记账**（接下来按 PLAY 用）。**VM 直接调 GameMode/GameInstance**——MVVM 的 VM 允许依赖游戏系统（比 WidgetController 更"贴地"），但依然不碰控件。
- **`ToSoftObjectPath().GetAssetName()`**：软引用取资产名（"Map_GreenForest"）——存档里存名字字符串，加载时按名 Find（第 2 篇 Maps 表的 Key 对得上）。
- **`SelectSlotButtonClick`（非动态多播）**：槽位按钮上抛给自己所属的 LoadScreen VM——**VM 之间的父子通信**（槽位事件聚合到屏幕级 VM 统一处理选中态）。
- **InitSlotStatus**：

```cpp
void UMVVM_LoadSlot::InitSlotStatus()
{
    const int32 WidgetSwitcherIndex = SlotStatus;      // int32 ↔ 枚举互转的桥
    SetSlotStatus(static_cast<ESaveSlotStatus>(WidgetSwitcherIndex));
}
```
蓝图调它触发 SlotStatus 的 FieldNotify 重广播（蓝图里改了 int 后用它"刷一遍"）——**int/enum 桥接函数**。

---

# 四、MVVM_LoadScreen —— 屏幕级 ViewModel ★★★★★

## 4.1 槽位集合与选中逻辑

```cpp
UPROPERTY() int32 DefaultSlotNumber = 3;
UPROPERTY() TMap<int32, UMVVM_LoadSlot*> LoadSlots;    // 槽位索引 → 槽位 VM
UPROPERTY() UMVVM_LoadSlot* CurrentSelectedSlot;

void UMVVM_LoadScreen::CreateAndInitLoadSlots()
{ 
    for (int32 i = 0; i < DefaultSlotNumber; i++)
    {
        UMVVM_LoadSlot* NewSlot = NewObject<UMVVM_LoadSlot>(this, LoadSlotViewModelClass);
        NewSlot->SetLoadSlotName(FString::Printf(TEXT("LoadSlot_%d"), i));
        NewSlot->SetSlotIndex(i);
        NewSlot->SelectSlotButtonClick.AddUObject(this, &UMVVM_LoadScreen::SelectSlotButtonPressed);
        LoadSlots.Add(i, NewSlot);
    }
}
```

- **三个子 VM 的工厂**：VM 里 NewObject 子 VM（Outer=this）——**ViewModel 树**（屏幕 VM → 槽位 VM×3），与控件树平行。蓝图侧控件（WBP_Slot×3）各自绑定自己的槽位 VM。
- **槽位名约定** `LoadSlot_0/1/2`：**存档槽名的命名即身份**（磁盘上三个 .sav 文件）。硬编码格式串——改名要与 GameMode 侧一致（契约）。
- **按钮上抛接线**：创建时就把子 VM 的按钮事件接到自己的处理函数——**VM 自己完成子装配**（不需要 C++ 外部接线）。

```cpp
void UMVVM_LoadScreen::SelectSlotButtonPressed(UMVVM_LoadSlot* LoadSlot)
{
    check(LoadSlot);
    for (TTuple<int32, UMVVM_LoadSlot*> Tuple : LoadSlots)
    {
        UMVVM_LoadSlot* Slot = Tuple.Value;
        Slot->SetSelectSlotButtonEnable(Slot != LoadSlot);    // 选中者禁用自己按钮，其余可点
    }
    SetPlayButtonEnable(true);    // FieldNotify 自动点亮 PLAY/DELETE
    SetDeleteButtonEnable(true);
    CurrentSelectedSlot = LoadSlot;
}
```

- **选中互斥**：遍历所有槽位 `Slot != LoadSlot` 设按钮态——被选槽的"选择"按钮变灰（防止重复选中）。**三行代码的互斥选择器**。
- **对比 MVC**：这段若在 WidgetController 里要广播"选中变化"事件让蓝图刷新按钮；MVVM 里 `SetSelectSlotButtonEnable` 内部 FieldNotify 自动刷——**通知链路少一跳**。

## 4.2 存档回读与按钮事务

```cpp
void UMVVM_LoadScreen::LoadSavedSlotDatas()
{
    ADuraGameModeBase* DuraGameModeBase = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this));
    if(!IsValid(DuraGameModeBase)) return;

    for (const TTuple<int32, UMVVM_LoadSlot*>& LoadSlot : LoadSlots)
    {
        ULoadScreenSaveGame* SaveGame = DuraGameModeBase->GetOrCreateSaveSlotData(LoadSlot.Value->GetLoadSlotName(), LoadSlot.Key);
        check(SaveGame);
        LoadSlot.Value->PlayerStartTag = SaveGame->PlayerStartTag;
        LoadSlot.Value->SetSlotStatus(SaveGame->SlotStatus);
        LoadSlot.Value->SetMapName(SaveGame->MapName);
        LoadSlot.Value->SetPlayerName(SaveGame->PlayerName);
        LoadSlot.Value->SetPlayerLevel(SaveGame->PlayerLevel);
    }
}
```

- **读档三注意**：
  1. `GetOrCreateSaveSlotData`（第 2 篇）：不存在的槽**新建空对象**（Vacant 状态）——**首次进主菜单磁盘无档也不崩**。
  2. **直接赋值 vs Set**：`PlayerStartTag = ...` 直赋（无 FieldNotify、纯传输）vs `SetSlotStatus`（有通知、驱动 UI）——**写法差异即语义差异**。
  3. `check(SaveGame)`：GetOrCreate 内 Cast 失败才可能空（配置错 LoadScreenSaveGameClass）——配置错误当场暴露。
- **执行时机**（LoadScreenHUD::BeginPlay 尾部）：**CreateAndInitLoadSlots 之后**——先有 VM 再读档填数据。

```cpp
void UMVVM_LoadScreen::PlayButtonPressed()
{
    ADuraGameModeBase* DuraGameMode = CastChecked<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this));
    UDuraGameInstance* DuraGameInstance = DuraGameMode->GetGameInstance<UDuraGameInstance>();
    DuraGameInstance->PlayerStartTag = CurrentSelectedSlot->PlayerStartTag;
    DuraGameInstance->LoadSlotName = CurrentSelectedSlot->GetLoadSlotName();
    DuraGameInstance->LoadSlotIndex = CurrentSelectedSlot->GetSlotIndex();

    if(IsValid(CurrentSelectedSlot))
    {
        DuraGameMode->TravelToMap(CurrentSelectedSlot);
    } 
}

void UMVVM_LoadScreen::DeleteButtonPressed()
{
    if(!IsValid(CurrentSelectedSlot)) return;
    ADuraGameModeBase::DeleteSlot(CurrentSelectedSlot->GetLoadSlotName(), CurrentSelectedSlot->GetSlotIndex());
    CurrentSelectedSlot->SetSlotStatus(Vacant);
    CurrentSelectedSlot->SetSelectSlotButtonEnable(true);
    EnablePlayAndDeleteButton(true);
}
```

- **PlayButtonPressed 的交接仪式**：把选中槽的**三件套**（槽名/槽位号/出生点 Tag）交给 GameInstance（跨图存续）→ TravelToMap 跳图。**GameInstance 是主菜单与游戏关卡之间唯一的桥**（第 2 篇的伏笔在此兑现）。
- **细节**：`CastChecked`（这里必须有 GameMode）+ `IsValid(CurrentSelectedSlot)` 后置检查（顺序上前面的访问已解引用——**若 CurrentSelectedSlot 为空第一行就崩**，检查应在最前。这是可挑剔的防御顺序问题，学习时标注）。
- **DeleteButtonPressed**：静态 DeleteSlot → 槽位 VM 复位为 Vacant（FieldNotify 自动切回"NEW"面板）→ 按钮恢复。**删除事务的 UI 复位全靠属性通知**——零手写刷新。
- **`DeleteSlot` 是 static**：不需要 GameMode 实例（纯 UGameplayStatics 包装）——**无状态操作用静态**。

---

# 五、LoadScreenHUD 与 LoadScreenWidget —— 装配时序 ★★★★☆

```cpp
void ALoadScreenHUD::BeginPlay()
{
    Super::BeginPlay();

    LoadScreenViewModel = NewObject<UMVVM_LoadScreen>(this, LoadScreenViewModelClass);
    LoadScreenViewModel->CreateAndInitLoadSlots();          // ① VM 树就绪

    LoadScreenWidget = CreateWidget<ULoadScreenWidget>(GetWorld(), LoadScreenWidgetClass);
    LoadScreenWidget->AddToViewport();                       // ② 控件上屏
    LoadScreenWidget->BlueprintInitializeWidget();           // ③ 蓝图绑定（控件 ↔ VM 的 FieldNotify 绑定建立）

    LoadScreenViewModel->LoadSavedSlotDatas();               // ④ 最后读档回填
}
```

- **四步时序的铁律**：**VM 先于控件**（控件蓝图要绑定 VM，VM 必须存在）；**读档最后**（数据回填时绑定已建立，FieldNotify 自动刷 UI——**顺序放错=读档不显示**）。
- **与 MVC 装配对比**：MVC 是"CreateWidget → SetController(触发蓝图接线) → Broadcast(快照) → Viewport"；MVVM 是"造 VM → 上屏 → 蓝图绑定 → 灌数据"。**共同点**：数据在展示就绪后才到。
- **`BlueprintInitializeWidget`**（BlueprintImplementableEvent）：MVVM 的"接线钩子"对应 MVC 的 WidgetControllerSet——蓝图在这里把三个 WBP_Slot 绑到 LoadScreenViewModel.GetLoadSlotViewModelByIndex(0/1/2)，把主按钮绑到 LoadScreen VM 的 FieldNotify 属性。

## 5.1 LoadScreenWidget —— 极简壳

```cpp
UCLASS()
class DURA_API ULoadScreenWidget : public UUserWidget
{
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
    void BlueprintInitializeWidget();
};
```
- C++ 零逻辑——**MVVM 下控件壳比 MVC 更薄**（连 WidgetController 成员都不需要，绑定由 FieldNotify 系统自动管）。

---

# 六、槽位生命周期总览（跨第 2/9 篇合流）

```
主菜单（本篇）                          游戏内（第 2 篇）
──────────────                        ──────────────
LoadScreenHUD::BeginPlay
  ① 造 3 个槽位 VM
  ② WBP 上屏、蓝图绑定
  ④ LoadSavedSlotDatas：逐槽
     GetOrCreateSaveSlotData ──────────► (磁盘 .sav)
     回填 VM → FieldNotify 刷 UI
点 NEW → EnterName 态
点确认 → NewSlotButtonPressed
  默认值组装 → SaveSlotData 落盘 ──────► (磁盘)
  GameInstance 记账
点 PLAY → PlayButtonPressed
  GameInstance 三件套 ────────────────► TravelToMap
                                          │ 新关卡
                                       GameMode.RetrieveInGameSaveData
                                        ◄─ GameInstance.LoadSlotName/Index
                                       Character.PossessedBy → LoadProgress
                                        （首次：初始化属性/技能）
                                       CheckPoint → SaveProgress 落盘
                                       死亡 → PlayerDied → OpenLevel
                                        ◄─ ChoosePlayerStart 按 PlayerStartTag
```

**面试考点**：
1. **MVC 与 MVVM 的选择依据**（事件流复杂度 vs 表单绑定；本项目两套并存示范）
2. **FieldNotify 机制与普通委托的区别**（属性级通知、UE_MVVM_SET_PROPERTY_VALUE 去抖、蓝图绑定编辑器直连）
3. **USaveGame 序列化的内容控制**（UPROPERTY 全量 vs SaveGame specifier 精选 vs Bytes 自定义流）
4. **GameInstance 在槽位流程中的角色**（跨地图唯一上下文）

---

*下一篇：`10-敌人AI与战斗链路.md` —— AIController、行为树服务与任务、黑板驱动的战斗循环。*
