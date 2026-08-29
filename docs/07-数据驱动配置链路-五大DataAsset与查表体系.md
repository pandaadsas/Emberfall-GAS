# 07 · 数据驱动配置链路 —— 五大 DataAsset 与"查表"体系

> **逻辑链路一句话概括**：
> `策划在编辑器里填 DataAsset（技能表/属性表/职业表/升级表/掉落表）` → `GameMode 持有三份全局表，其余系统通过 AbilitySystemLibrary 获取` → `运行时按 GameplayTag/枚举/等级查表` → **C++ 逻辑零改动地表达"新技能/新职业/新掉落"**。
> 本篇是五个小而精的文件合辑——**数据资产类代码不长，但"字段为什么这样设计"全是要点**。

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/AbilitySystem/Data/AbilityInfo.h` / `.cpp` | 技能表：Tag→图标/类型/等级需求/技能类 | ★★★★★ |
| 2 | `Source/Dura/Public/AbilitySystem/Data/AttributeInfo.h` / `.cpp` | 属性表：Tag→名称/描述（本地化文本） | ★★★★☆ |
| 3 | `Source/Dura/Public/AbilitySystem/Data/CharacterClassInfo.h` / `.cpp` | 职业表：初始化 GE/技能/XP 奖励/伤害系数曲线 | ★★★★★ |
| 4 | `Source/Dura/Public/AbilitySystem/Data/LevelUpInfo.h` / `.cpp` | 升级表：XP 门槛/奖励点数 + FindLevelForXP | ★★★★☆ |
| 5 | `Source/Dura/Public/AbilitySystem/Data/LootTiers.h` / `.cpp` | 掉落表：概率×数量的掉落物生成 | ★★★☆☆ |

配套资产（蓝图）：`DA_AbilityInfo` / `DA_AttributeInfo` / `DA_CharacterClassInfo` / `DA_LevelUpInfo` / `DA_LootTiers`。项目根目录 `Data/` 文件夹里还有 CSV/JSON 形态的同名表（`CT_PrimaryAttributes_*.json`、`CT_Damage.json`）——**编辑器外的策划数据交换格式**（Excel 导出→JSON→编辑器导入曲线的常见流水线）。

---

# 一、共同模式：先看懂"一张表"的通用骨架 ★★★★☆

五个 DataAsset 全是这个形状：

```cpp
USTRUCT(BlueprintType)
struct F_XxxInfo {          // 行结构体
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag Key;       // Tag 作为主键
    ...元数据字段...
};

UCLASS()
class U_XxxInfo : public UDataAsset {   // 表
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<F_XxxInfo> Rows;

    F_XxxInfo Find_Xxx_ForTag(const FGameplayTag& Tag, bool bLogNotFound = false) const;   // 查询函数
};

// 查询函数实现（全部同构）：
F_XxxInfo U_XxxInfo::Find_Xxx_ForTag(const FGameplayTag& Tag, bool bLogNotFound) const
{
    for (const F_XxxInfo& Info : Rows)
    {
        if (Info.Key.MatchesTagExact(Tag))
        {
            return Info;
        }
    }
    if (bLogNotFound)
    {
        UE_LOG(LogDura, Error, TEXT("Can't find Info for Tag [%s] on [%s]."), *Tag.ToString(), *GetNameSafe(this));
    }
    return F_XxxInfo();   // 默认行兜底
}
```

**四个设计决策（每一处都有讲究）**：

1. **为什么用 `TArray<行结构体>` 而不是 `TMap<Tag, 行>`**：TMap 在蓝图编辑器里排序不稳定、不支持重排（策划想按顺序看/编辑），且数组可以塞**多个同 Key 行**（虽不推荐）。**编辑器体验优先于运行时查找**（N<20 的表线性查找无所谓）。
2. **`MatchesTagExact`**：表查询必须精确匹配——`Abilities.Fire.FireBolt` 不能被 `Abilities.Fire` 的行抢先命中。
3. **`bLogNotFound = false` 默认静默**：调用方语义决定（有的查询"允许找不到"——比如查一个还没解锁的技能）。同款设计在 InputConfig（第 3 篇）。
4. **返回默认结构体而不是 nullptr**：蓝图无法处理"结构体或空"（结构体是值语义），**返回默认行让调用方检查字段有效性**（如 `Info.Ability != nullptr`）。C++ 返回值语义简单。

**通俗解释**：DataAsset 是"策划的 Excel 表格进了游戏"。行结构体=表格的列定义，TArray=表格的行，Find 函数=WHERE 查询。**但查表键是 GameplayTag**——这就是第 1 篇 Tag 体系的红利：任何系统拿到 Tag 就能查到自己的"说明书"。

---

# 二、AbilityInfo —— 技能表 ★★★★★

## 2.1 行结构 FDuraAbilityInfo 逐字段

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag AbilityTag;      // 身份：Abilities.Fire.FireBolt
UPROPERTY(BlueprintReadOnly)                  FGameplayTag InputTag;        // 装备槽（运行时填，见下）
UPROPERTY(BlueprintReadOnly)                  FGameplayTag StatusTag;       // 状态（运行时填）
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag CooldownTag;     // 冷却：CoolDown.Fire.FireBolt
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag AbilityType;     // Offensive/Passive/None
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<const UTexture2D> Icon;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<const UMaterialInterface> BackgroundMaterial;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 LevelRequirement = 1;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSubclassOf<UGameplayAbility> Ability;
```

- **只读指针 `const UTexture2D*`**：表只引用不修改——语义声明。`BackgroundMaterial` 是技能图标的背景框材质（按元素配色：火=红框、雷=紫框）。
- **`InputTag`/`StatusTag` 没有 EditDefaultsOnly**：**这两列是运行时"当前状态"**——SpellMenuWidgetController 查表后把 ASC 里的实际装备槽/状态**填进副本**再给 UI。**静态配置与动态状态共用一个结构体**（配置列只读、状态列运行时写）——一份数据结构服务两阶段。
- **`LevelRequirement`**：解锁等级。三个消费者：ASC::UpdateAbilityStatuses（升级扫描）、UI 锁定显示（GetLockedDescription(LevelRequirement)）、SpellMenu 的可学习判断。
- **`Ability`（技能类引用）**：**为什么表里要存类**——ASC 里升级状态机用 Spec 就够了，但 **Eligible 授予**（UpdateAbilityStatuses）需要从表里拿类来 GiveAbility；存档恢复也从表取类（第 2 篇讲过"存 Tag 不存类"的对照逻辑）。**表是 Tag↔类的目录**。

## 2.2 消费场景总览（理解表的"上游下游"）

| 消费者 | 用哪些列 | 场景 |
|---|---|---|
| `ASC::UpdateAbilityStatuses` | LevelRequirement / Ability / AbilityTag | 升级时授予 Eligible 技能 |
| `SpellMenuWidgetController` | 全列 + 回填 Input/Status | 技能菜单格子渲染 |
| `ASC::GetDescriptionsByAbilityTag` | LevelRequirement | 锁定描述 |
| `ASC::IsPassiveAbility` | AbilityType | 装备逻辑判被动 |
| `ADuraCharacter::SaveProgress` | Ability（存档取类）/ AbilityType | 存档快照 |
| HUD 技能栏（蓝图） | Icon / CooldownTag | 冷却转圈图标 |

- **面试点**：*"为什么技能元数据不放技能类里，而要单独一张表？"* —— (a) **未解锁的技能还没有实例**，UI 仍需显示锁定图标/等级需求；(b) 表可以**集中查看**全部技能的平衡数值；(c) 类只放"行为"，元数据放数据——**行为与配置分离**。

---

# 三、AttributeInfo —— 属性表（本地化展示）★★★★☆

```cpp
USTRUCT(BlueprintType)
struct FDuraAttributeInfo
{
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag AttributeTag;      // Attributes.Primary.Strength
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText AttributeName;            // "力量"
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText AttributeDescription;     // "提高物理伤害"
    UPROPERTY(BlueprintReadOnly)                  float AttributeValue = 0.f;    // 运行时填
};
```

- **`FText` 而不是 FString（★★★★★ 本地化必考点）**：FText 是 UE 本地化系统的一等公民——支持**多语言查找表**（StringTable）、按 Culture 切换。**用户可见文本一律 FText**，FString 用于程序内部拼接。面试常问 FString/FName/FText 三兄弟：
  - **FName**：不可变、哈希存储、比较快（Tag 内部就是它）；
  - **FString**：可变字符串，拼接/格式化；
  - **FText**：本地化+不可变显示文本，开销最大但只用于 UI。
- **`AttributeValue` 运行时填**：AttributeMenuWidgetController 查表后把 ASC 的实际属性值填进去（与 AbilityInfo 的 Input/Status 同款"配置+状态双列"设计）。
- **属性菜单的完整数据流**：UI 蓝图遍历表 → 对每行：`TagsToAttributes[Tag]()` 拿属性句柄（第 4 篇的函数指针表！）→ 注册属性变化委托 → 变化时查表填 AttributeValue → 广播给 UI 刷新。**Tag 是贯穿三张表的轴线**：第 1 篇 Tag 注册 → 第 4 篇 Tag→属性映射 → 本篇 Tag→文案。

---

# 四、CharacterClassInfo —— 职业表（本篇核心）★★★★★★

## 4.1 枚举与职业行

```cpp
UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
    Elementalist,   // 元素使（玩家）
    Warrior,        // 战士（近战怪）
    Ranger          // 游侠（远程怪）
};

USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
    UPROPERTY(EditDefaultsOnly, Category="Class Defaults")
    TSubclassOf<UGameplayEffect> PrimaryAttributes;   // 主属性初始化 GE（按职业不同！）

    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;   // 职业初始技能

    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    FScalableFloat XPReward = FScalableFloat();       // 击杀 XP 奖励（可曲线）
};
```

- **`ECharacterClass` 枚举而非 Tag**：职业是**封闭集合**（三种，switch-able），用枚举获得编译期完整性；而技能/伤害类型是**开放集合**（会加新技能），用 Tag。**"封闭用枚举、开放用 Tag"** 的选型原则——面试讲数据设计的好素材。
- **`XPReward` 是 FScalableFloat**：战士 1 级给 10 XP、10 级战士给 200 XP——`GetValueAtLevel(击杀目标等级)`（第 5 篇 GetXPRewardForClassAndLevel）。

## 4.2 通用段（全职业共享）

```cpp
UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> PrimaryAttributesSetByCaller;  // 存档恢复用（SetByCaller 版主属性 GE）
UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> SecondaryAttributes;           // 次要属性初始化（通用）
UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> SecondaryAttributes_Infinite;  // 无限时长版（读档）
UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> VitalAttributes;               // 血蓝初始化（通用）
UPROPERTY(EditDefaultsOnly) TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;      // 全员技能（HitReact 等）
UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveTable> DamageCalculationCoefficients;      // 伤害系数曲线表

FCharacterClassDefaultInfo GetClassDefaultInfo(ECharacterClass CharacterClass) const;
```

- **为什么主属性按职业、次要/生命属性通用**：职业差异本质是"属性分配"（战士力量高、元素使智力高）——主属性 GE 按职业配；次要属性的**派生公式**（MMC）全职业一致，GE 里只是"引用 MMC"，所以一份通用。**分层复用**：变的按行配、不变的共享配。
- **两版次要属性 GE**（普通 + Infinite）：正常初始化用带时长版本，读档用 Infinite——**时长语义按场景选**（重进游戏后 GE 重新应用，Infinite 保证不过期）。
- **`DamageCalculationCoefficients`（CurveTable）**：第 5 篇 ExecCalc 查的三条曲线（ArmorPenetration / EffectiveArmor / CriticalHitResistance）都在这张表——**按行名查、按等级 Eval**。CurveTable 是策划在编辑器里维护的"多行多列数值表"，Excel 导入的最小形态。

## 4.3 GetClassDefaultInfo 的防御回退（★ 项目改造点）

```cpp
FCharacterClassDefaultInfo UCharacterClassInfo::GetClassDefaultInfo(ECharacterClass CharacterClass) const
{
    // 数据资产漏配该职业时回退到 Warrior 并告警，避免 FindChecked 断言崩溃
    if(const FCharacterClassDefaultInfo* Info = CharacterClassInformation.Find(CharacterClass))
    {
        return *Info;
    }

    UE_LOG(LogDura, Warning, TEXT("UCharacterClassInfo [%s] 缺少职业 [%d] 的配置，已回退到 Warrior"),
        *GetName(), static_cast<int32>(CharacterClass));
    return CharacterClassInformation.FindRef(ECharacterClass::Warrior);
}
```

- **TMap 的 Find vs FindRef vs FindChecked**：
  - `Find` → 返回指针（可空）——**先判后用**；
  - `FindRef` → 返回值拷贝（缺失给默认）；
  - `FindChecked` → 缺失直接 ensure 崩溃（断言配置完整性）。
- **这里的取舍**：课程原代码用 `FindChecked`（配置错误当场炸）；本项目改成 **Warn + 回退 Warrior**—— Shipping 里崩溃=玩家丢失进度，告警降级=可玩性保住。**开发期断言、运行期降级**的权衡思考，配合 git 提交"断言降级"系列食用。**注意**：如果 Warrior 也没配，FindRef 返回默认结构体（GE 类为空）——上游 InitializeDefaultAttributes 里有 `if(!CharacterClassInfo) return` 但 GE 空会走到 MakeOutgoingSpec 空类——**回退链的尽头仍需上游判空**，防御是分层的。

---

# 五、LevelUpInfo —— 升级表与 FindLevelForXP ★★★★☆

## 5.1 行结构

```cpp
USTRUCT(BlueprintType)
struct FDuraLevelUpInfo
{
    UPROPERTY(EditDefaultsOnly) int32 LevelUpRequirement = 0;   // 升到下一级需要的累计 XP
    UPROPERTY(EditDefaultsOnly) int32 AttributePointAward = 1;  // 该级奖励的属性点
    UPROPERTY(EditDefaultsOnly) int32 SpellPointAward = 1;      // 该级奖励的技能点
};
```

- **LevelUpRequirement 是累计值**（不是增量）：10 级要求累计 500 XP——`FindLevelForXP` 直接比较。
- **数组的下标语义**（源码注释）：`LevelUpInformation[1] = 等级 1 的信息`——**下标=等级**，第 0 格浪费（UE 数组从 0 开始，等级从 1 开始的错位约定，配图说明比代码更直观）。

## 5.2 FindLevelForXP —— 查找算法精讲

```cpp
int32 ULevelUpInfo::FindLevelForXP(int32 XP) const
{
    int32 Level = 1;
    bool bSearching = true;
    while(bSearching)
    {
        // LevelUpInformation[1] = 等级 1 的信息
        // LevelUpInformation[2] = 等级 2 的信息
        if(LevelUpInformation.Num() - 1 <= Level) return Level;   // 已到表末 = 满级

        if(XP >= LevelUpInformation[Level].LevelUpRequirement)
        {
            ++Level;
        }
        else
        {
            bSearching = false;
        }
    }
    return Level;
}
```

- **线性爬梯算法**：从 1 级开始，"我的 XP ≥ 当前级的门槛？"→ 是则爬一级，否则停在当前。**O(等级数)**——50 级表最多 50 次比较，无需二分。
- **边界一：满级**。`Num()-1 <= Level` 判断"下标已到表尾"——XP 再多也只能停在最高级（表有 41 行 = 满级 40）。**这个分支同时兼任越界保护**（下一行访问 `LevelUpInformation[Level]` 前 Level 已确认 < Num-1）。
- **边界二：XP 不足一级**：Level 保持 1 返回。
- **消费场景**：HandleIncomingXP（第 4 篇）`FindLevelForXP(CurrentXP + 奖励XP) - CurrentLevel = 升了几级`；UI 的 XP 条进度（当前级门槛与下一级门槛之间插值）。
- **算法变体讨论（面试拓展）**：等级门槛**单调递增**时可用 `UpperBound` 二分（O(logN)）；表格保证有序的前提下，缓存上次等级+增量爬升（大多数 XP 只涨一点）更省。**当前实现的优点**：对乱序表鲁棒、代码零心智负担。

---

# 六、LootTiers —— 掉落表 ★★★☆☆

## 6.1 行结构与掷骰逻辑

```cpp
USTRUCT(BlueprintType)
struct FLootItem
{
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<AActor> LootClass;   // 掉落物类
    UPROPERTY(EditAnywhere) float ChanceToSpawn = 0.f;                        // 每个生成位的概率
    UPROPERTY(EditAnywhere) int32 MaxNumberToSpawn = 0;                       // 掷骰次数
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bLootLevelOverride = true; // 掉落等级覆盖标志
};

TArray<FLootItem> ULootTiers::GetLootItems()
{
    TArray<FLootItem> ReturnItems;
    for (FLootItem& Item : LootItems)
    {
        for (int32 i = 0; i < Item.MaxNumberToSpawn; ++i)
        {
            if(FMath::FRandRange(1.f, 100.f) < Item.ChanceToSpawn)
            {
                FLootItem NewItem;
                NewItem.LootClass = Item.LootClass;
                NewItem.bLootLevelOverride = Item.bLootLevelOverride;
                ReturnItems.Add(NewItem);   // 注意：Chance/MaxNumber 没拷贝——生成结果行
            }
        }
    }
    return ReturnItems;
}
```

- **"次数×概率"模型**：`MaxNumberToSpawn=5, ChanceToSpawn=30` = 掷 5 次骰、每次 30% 掉——**期望产出 1.5 个**。与"一次掷骰决定数量"（Binomial 的另一种采样）相比，这种写法让策划用"次数+概率"两个直觉参数逼近任意期望值。
- **浮点掷骰**：`FRandRange(1,100) < Chance`——左闭右开区间的细节（100.0 不可能取到，边界安全）。
- **返回的行是"生成结果"**：Chance/MaxNumber 被抹掉、只留类与覆盖标志——**配置行与结果行的字段语义不同**（结果行 = "要生成一个什么"）。消费者（蓝图 SpawnLoot）遍历结果逐个 SpawnActor。
- **`bLootLevelOverride`**：掉落物是否用"击杀者等级"覆盖自身默认等级——**掉落成长**（10 级怪掉的药水是 10 级药水）。
- **调用链**：敌人 Die（第 2 篇）→ 蓝图 SpawnLoot 事件 → `GetLootTiers(WorldContext)` 拿表（GameMode 持有）→ GetLootItems 掷骰 → 逐个生成。**掉落概率表全局一份**（DA_LootTiers），想按怪物分级就配多张表/多套行——本项目的简化取舍。

---

# 七、本章数据流总图

```
                    GameMode（第 2 篇）持三份表
   ┌──────────────────────┬───────────────────────┬──────────────────┐
   ▼                      ▼                       ▼                  
CharacterClassInfo        AbilityInfo             LootTiers
（职业行+通用GE+曲线表）    （技能行表）              （掉落行）
   │                      │                       │
   ├─ 初始化：InitializeDefaultAttributes按职业取GE    └─ 敌人Die→蓝图掷骰生成
   ├─ 技能授予：GiveStartupAbilities（Common+职业）
   ├─ XP：GetXPRewardForClassAndLevel
   └─ 伤害：ExecCalc 查 DamageCalculationCoefficients 曲线
                          │
              ┌───────────┴────────────┐
              ▼                        ▼
      ASC::UpdateAbilityStatuses   SpellMenuWidgetController
      （升级解锁扫描）               （UI 全列渲染 + 状态回填）

PlayerState 持 LevelUpInfo → FindLevelForXP（XP 爬梯）→ 升级奖励点数
AttributeInfo ← UI 属性菜单：Tag → 文案（FText 本地化） + Tag→属性（第 4 篇函数指针表）→ 实时数值
```

**贯穿全篇的一句话**：**Tag 是钥匙、DataAsset 是抽屉、Library 是走廊**——所有系统通过 Tag 找数据，通过 Library 拿表，谁也不直接认识谁。

# 八、动手实验建议

1. 在 DA_AbilityInfo 里新增一行 IceBolt（Tag 用新的 `Abilities.Ice.IceBolt`，Ability 先随便指个 GA），升级到需求等级后观察技能菜单出现可学图标——**全程不改 C++**，体感数据驱动。
2. 把 `FindLevelForXP` 改成二分查找版本，单元对比两个函数在 40 级表上的返回值一致性。
3. 把 CharacterClassInfo 的回退逻辑改回 FindChecked，漏配一个职业，PIE 生成敌人观察崩溃——再改回来，体会降级设计的价值。

---

*下一篇：`08-UI与WidgetController链路.md` —— MVC 模式下 HUD/WidgetController/委托绑定的完整数据流。*
