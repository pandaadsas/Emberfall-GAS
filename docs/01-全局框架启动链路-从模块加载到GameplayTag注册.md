# 01 · 全局框架启动链路 —— 从模块加载到 GameplayTag 注册

> **逻辑链路一句话概括**：
> `引擎加载游戏模块（Dura.cpp）` → `引擎启动时调用资产管理器的 StartInitialLoading（DuraAssetManager）` → `在其中注册所有原生 GameplayTag（DuraGameplayTags）` → `GAS 系统在创建 GameplayEffectContext 时使用我们自定义的结构体（DuraAbilitySystemGlobals + DuraAbilitiesTypes）`。
> 这条链路是整个游戏的"地基"：后面的属性系统、技能系统、UI、AI 全部依赖这里注册的 Tag 和类型。

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Dura.h` | 全局常量与碰撞通道宏定义 | ★★★☆☆ |
| 2 | `Source/Dura/Dura.cpp` | 游戏主模块入口 | ★★☆☆☆ |
| 3 | `Source/Dura/DuraLogChannels.h` / `.cpp` | 自定义日志通道 LogDura | ★★★☆☆ |
| 4 | `Source/Dura/Public/DuraAssetManager.h` / `Private/DuraAssetManager.cpp` | 资产管理器子类，Tag 注册的触发点 | ★★★★☆ |
| 5 | `Source/Dura/Public/DuraGameplayTags.h` / `Private/DuraGameplayTags.cpp` | 全部原生 GameplayTag 的定义与注册（本篇核心） | ★★★★★ |
| 6 | `Source/Dura/Public/DuraAbilitiesTypes.h` / `Private/DuraAbilitiesTypes.cpp` | 自定义 GE 上下文结构体 + 手写网络序列化 | ★★★★★ |
| 7 | `Source/Dura/Public/AbilitySystem/DuraAbilitySystemGlobals.h` / `.cpp` | 让 GAS 使用自定义上下文的钩子 | ★★★★☆ |

---

# 一、Dura.h —— 全局常量与碰撞通道

## 1.1 官方层面：这个文件是什么

`Dura.h` 是 Unreal 项目模板自动生成的主头文件，每个包含 `IMPLEMENT_PRIMARY_GAME_MODULE` 的模块都会有一个同名头文件。它被几乎所有源文件 `#include`，因此适合放"全项目都需要、又轻量"的东西——本项目放了 4 个全局常量和 3 个碰撞通道宏。

## 1.2 逐行拆解

```cpp
const float HIGHLIGHT_COLOR_RED = 250.0f;
const float HIGHLIGHT_COLOR_BLUE = 251.0f;
const float HIGHLIGHT_COLOR_TAN = 252.0f;
```

- 这三个常量是**自定义深度（Custom Depth）/ 模板值（Stencil Value）**，用于角色高亮描边系统。
- **为什么取 250/251/252 这种奇怪的值？** 后处理材质（Post Process Material）里用 `SceneTexture:CustomStencil` 通道读到的值会被拿来当索引比较，比如 stencil == 250 就输出红色描边。选 250+ 是因为 0 是默认背景、1~249 常被引擎/其他系统占用，用高段位避开冲突。
- **为什么是 `const float` 而不是 `constexpr`？** 两者在 C++ 层面都行；`const` 在头文件中默认内部链接（internal linkage），每个翻译单元各有一份拷贝，安全性没问题。用 `constexpr` 会更符合现代 C++ 习惯（编译期常量），这是一个可以优化的点。
- **通俗解释**：把描边系统想象成"给角色贴不同颜色的荧光标签"。250 号标签=红色描边（敌人），251=蓝色（友方/玩家），252=黄褐色（可交互物体如宝箱、药水）。真正"贴标签"的代码在 `HighlightInterface` 相关类中（见第 11 篇），"读标签"的代码在后处理材质 `M_PostProcess` 里。

```cpp
#define ECC_Projectile ECollisionChannel::ECC_GameTraceChannel1
#define ECC_Target ECollisionChannel::ECC_GameTraceChannel2
#define ECC_ExcludePlayers ECollisionChannel::ECC_GameTraceChannel3
```

- 这三个宏把引擎预留给游戏的自定义碰撞通道映射为语义化名字：
  - `ECC_Projectile`（GameTraceChannel1）：**投射物**通道。火球等弹体用它，这样弹体只和"能被击中的东西"碰撞。
  - `ECC_Target`（GameTraceChannel2）：**目标拾取**通道。鼠标点击选目标、技能选点时用这个通道做 `GetHitResultUnderCursor`。
  - `ECC_ExcludePlayers`（GameTraceChannel3）：**排除玩家**通道。敌人的弹体碰撞检测要打中玩家/环境但穿过其他敌人时，用它过滤。
- **为什么用 `#define` 而不是 `constexpr`？** 因为 `ECollisionChannel` 是枚举，宏只是纯文本替换，展开后就是枚举成员，零开销且能在 `UPROPERTY`、函数参数等任何地方使用。缺点是宏没有类型检查、没有作用域，现代写法更推荐 `constexpr ECollisionChannel ECC_Projectile = ECollisionChannel::ECC_GameTraceChannel1;`——不过 UE 引擎源码自身也大量使用宏常量（如 `ECC_WorldStatic` 系列注释），这是引擎社区的习惯写法。
- **这些通道必须在 Project Settings → Collision 里手动配置**：打开 `Config/DefaultEngine.ini` 能看到 `GameTraceChannel1=Projectile, ...` 的绑定，代码宏和配置表是两回事，**宏只是给已有枚举起别名**，真正的名字映射在 ini 里。

## 1.3 拓展知识点

- **Custom Depth + Custom Stencil 的原理**：引擎渲染时，如果 Actor 勾选了 `bRenderCustomDepth`，会额外渲染一遍深度和模板值到 GBuffer。后处理材质读取这个通道即可实现"只有该物体被描边"的效果，无需改模型材质。性能开销与屏幕上被描边物体的像素覆盖面积成正比。
- **面试考点**：*"UE 里如何实现物体描边？"* 标准答案路线：`bRenderCustomDepth` + Stencil 值 + 后处理材质（CustomStencil 判断）→ 高亮接口动态开关（`HighlightInterface`，第 11 篇）。追问会到"描边怎么做（沿法线挤出 / 边缘检测）""Custom Depth 的开销"。

---

# 二、Dura.cpp —— 游戏主模块入口

## 2.1 官方层面

每个 UE C++ 项目都有一个**主游戏模块**（Primary Game Module）。`IMPLEMENT_PRIMARY_GAME_MODULE` 宏完成三件事：
1. 实现模块接口 `FDefaultGameModuleImpl`（启动/关闭时被引擎调用）；
2. 声明模块名为 `Dura`；
3. 把它标记为主游戏模块（整个项目**必须且只能有一个**主游戏模块）。

```cpp
IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Dura, "Dura" );
```

三个参数分别是：**模块实现类**、**模块名（必须与 Build.cs / uproject 中的名字一致）**、**显示名**。

## 2.2 通俗解释

把模块想成一个"插件包"。引擎启动 = 先插好所有插线板（引擎模块），再插你的插头（游戏模块）。这个文件就是插头的"通电声明"。平时我们不在模块级写逻辑，因为 UE 的 Gameplay 层代码几乎都挂在 UObject/GActor 上，模块只负责"存在"。

## 2.3 拓展知识点

- **模块的 `.Build.cs`**：`Source/Dura/Dura.Build.cs` 里声明了依赖模块。本项目的关键依赖有 `"GameplayAbilities"`, `"GameplayTags"`, `"GameplayTasks"`（GAS 三件套）、`"UMG"`, `"EnhancedInput"`, `"Niagara"`, `"AIModule"`, `"GameplayTasks"` 等。**忘了在 Build.cs 加模块依赖是新手最常见编译错误**（找不到头文件 / unresolved external symbol）。
- **面试考点**：*"一个 UE 项目可以有多个模块吗？主模块是什么？"* 答案：可以，每个模块一个 Build.cs；主模块必须有且仅有一个 `IMPLEMENT_PRIMARY_GAME_MODULE`，其余模块用 `IMPLEMENT_MODULE`。

---

# 三、DuraLogChannels —— 自定义日志通道

## 3.1 官方层面

UE 日志宏 `UE_LOG(分类, 级别, 格式串, ...)` 的第一个参数是**日志分类（Log Category）**。引擎内置 `LogTemp`（随手用、无语义）、`LogAnimation`、`LogNet` 等。自定义分类通过两步声明：

```cpp
// .h
DECLARE_LOG_CATEGORY_EXTERN(LogDura, Log, All);
// .cpp
DEFINE_LOG_CATEGORY(LogDura);
```

- `DECLARE_LOG_CATEGORY_EXTERN(名字, 默认详细度, 编译期上限)`：在头文件里声明一个 `extern` 分类。第二个参数 `Log` 表示运行时默认输出级别及以上；第三个参数 `All` 表示编译时不裁剪任何级别（若写 `Verbose` 则 Verbose/VeryVerbose 的语句编译不出来）。
- `DEFINE_LOG_CATEGORY`：在某个 .cpp 里分配实际的 `FLogCategory` 对象（存储名字和运行时详细度）。

## 3.2 为什么不用 LogTemp？

- `LogTemp` 是万金油分类，所有临时日志混在一起，无法按模块过滤。
- 自定义 `LogDura` 后，可以在控制台输入 `Log LogDura Verbose` 单独调详细度、`Log LogDura Off` 静音，也可以在 Output Log 面板按分类过滤。
- **本项目已经把全部 `LogTemp` 替换为 `LogDura`**（见 git 提交 `chore: LogTemp 统一替换为项目日志分类 LogDura`），这是正规项目的做法——商业项目里日志分类就是"调试索引"。
- **通俗解释**：日志分类就像微信群。`LogTemp` 是"所有人"大群，刷屏且找不到重点；`LogDura` 是"本项目专属群"，想屏蔽就屏蔽，想@就@。

## 3.3 拓展知识点

- 使用处示例（在 `DuraCharacterBase.cpp` 等文件里常见）：
  ```cpp
  UE_LOG(LogDura, Warning, TEXT("Ability [%s] not found"), *Tag.ToString());
  ```
  注意 `FGameplayTag`/`FString` 要加 `*` 取底层 `TCHAR*`；浮点用 `%f`，向量用 `.ToString()`。
- **面试考点**：*"UE_LOG 的三个级别控制分别在哪里生效？"* —— `DECLARE_LOG_CATEGORY_EXTERN` 的第二参（运行时默认）与第三参（编译期裁剪），以及运行时控制台 `Log <Category> <Level>`（运行时动态）。

---

# 四、DuraAssetManager —— 资产管理器与 Tag 注册的"引爆点"

## 4.1 官方层面：UAssetManager 是什么

`UAssetManager` 是引擎的**资产管理单例**，负责异步加载、资产注册表（Asset Registry）、主资产 ID（Primary Asset Id，用于 Data Asset 的引用/卸载，如 `Dura:DA_AbilityInfo`）。项目要定制它只需：

1. 写 `UCLASS() class UDuraAssetManager : public UAssetManager`；
2. 在 `Config/DefaultEngine.ini` 的 `[/Script/Engine.Engine]` 段写 `AssetManagerClassName=/Script/Dura.DuraAssetManager`。

引擎启动早期（`FEngineLoop::Init` → `UEngine::Init`）会实例化这个类，并调用虚函数 **`StartInitialLoading()`**——这是**所有项目里最早、最适合做"全局一次性初始化"的 C++ 钩子**。

## 4.2 逐行拆解

```cpp
UDuraAssetManager& UDuraAssetManager::Get()
{
    check(GEngine);

    UDuraAssetManager* DuraAssetManager = Cast<UDuraAssetManager>(GEngine->AssetManager);
    return *DuraAssetManager;
}
```

逐个元素讲：

- **`GEngine`**：引擎全局指针（`UEngine*`），只要引擎活着就非空。
- **`check(GEngine)`**：UE 断言宏。Debug/Development 下条件为假直接崩溃并打印调用栈；Shipping 下被编译剔除。这里保证"如果引擎还没建好就有人来拿 AssetManager，立即崩溃暴露问题"，比返回空引用再用时崩溃更容易定位。类似的宏强度梯度：`ensureMsgf`（可恢复+报错）< `check`（必崩）< `static_assert`（编译期）。本项目后来的提交特意把一些不当的 `check` 降级为日志，因为 Shipping 无法调试崩溃。
- **`GEngine->AssetManager`**：引擎持有的 `UAssetManager*`。因为 ini 里配置了我们的子类，这里实际存的是 `UDuraAssetManager` 实例。
- **`Cast<UDuraAssetManager>(...)`**：UE 的 RTTI 安全转换，失败返回 `nullptr`（若用 `CastChecked` 则会崩溃——这里没用，因为 ini 配置正确时不可能失败；这是一个可以写 `CastChecked` 的位置）。
- **返回 `引用` 而非指针**：调用方写起来省去判空，语义是"这个单例一定存在"。
- **为什么用静态 `Get()` 而不是 `GEngine->GetAssetManager()`**：封装 + 类型安全（返回子类引用，调用方不用自己 Cast）。

```cpp
void UDuraAssetManager::StartInitialLoading()
{
    Super::StartInitialLoading();

    FDuraGameplayTags::InitializeNativeGameplayTags();
}
```

- **`Super::StartInitialLoading()`**：UE 惯例，父类可能做资产注册表扫描等基础工作，永远先调父类。
- **`InitializeNativeGameplayTags()`**：注册所有原生 Tag。**为什么必须这么早？** 因为 GAS 启动时（`InitAbilitySystemComponent`、应用 Startup GE、初始化属性）就要用到 Tag，如果 Tag 在使用之后才注册，`UGameplayTagsManager::Get().AddNativeGameplayTag` 内部会触发 `ensure`（Tag 未注册就使用）或行为异常。放在 AssetManager 的启动钩子里，保证早于一切 Gameplay 逻辑。

## 4.3 通俗解释

把游戏启动想成做一桌菜：`StartInitialLoading` 是"开火前把所有调料摆上台面"——Tag 就是调料罐上的标签。后面所有菜（属性、技能、伤害）都按标签找调料。如果炒到一半才贴标签（晚注册），有的菜已经没味了（Tag 匹配失败）。

## 4.4 拓展知识点

- **UAssetManager 的另一个大用途**：管理 **PrimaryDataAssets**（带 `FPrimaryAssetId` 的 DataAsset）的异步加载/内存管理。本项目没有走这条路（DataAsset 直接在蓝图里引用），但库洛这类大型项目会用它做资产流式卸载。
- **面试考点**：*"GAS 项目里原生 Tag 为什么要在引擎启动时就注册？放在哪注册？"* —— 答：GAS 生命周期早于第一帧，ASC 初始化、StartupGE 应用、ExecCalc 捕获都会用 Tag；注册点选 `UAssetManager::StartInitialLoading()`（ini 配置子类），这是引擎官方推荐位置。

---

# 五、DuraGameplayTags —— 全项目的"通用语"（本篇核心 ★★★★★）

## 5.1 官方层面：GameplayTag 是什么

GameplayTag 是引擎提供的**层级化字符串标签**系统（如 `Abilities.Fire.FireBolt`），有全局管理器 `UGameplayTagsManager` 统一登记、去重、建父子关系。它解决的问题：**系统之间的松耦合通信**。技能、属性、UI、AI 之间不直接 include 对方，而是约定"谁身上挂着什么 Tag"来表达状态。

**Tag 的三种创建方式**：
1. **Native Tag（原生/C++ Tag）**：C++ 里 `AddNativeGameplayTag` 注册，蓝图只读。适合"代码要拿它当标识符"的 Tag。
2. **Config Tag（配置 Tag）**：在 `Config/DefaultGameplayTags.ini` 里配，或编辑器 Project Settings 里加。适合纯数据标记。
3. **蓝图 Tag**：在资产里临时填。最随意，重构最难。

本项目把**所有 C++ 需要用到的 Tag 全部做成 Native Tag**，这是工程化最佳实践：C++ 编译器帮我们检查拼写（`DuraGameplayTags.Damage_Fire` 写错直接编译不过，而字符串 Tag 写错只会在运行时静默失效）。

## 5.2 结构体设计拆解

```cpp
struct DURA_API FDuraGameplayTags
{
public:
    static const FDuraGameplayTags& Get() 
    {
        return DuraGameplayTags;
    }

    static void InitializeNativeGameplayTags();
    ...
private:
    static FDuraGameplayTags DuraGameplayTags;
};
```

- **`DURA_API`**：模块导出宏。只有加上它，其他模块（将来拆模块时）才能链接到这个类的符号。
- **`static Get()` 返回 const 引用**：Meyers 单例的变体。这里是"静态成员单例"：私有静态实例 + 公有静态访问器。返回 **const 引用**意味着调用方只能读不能改——Tag 是"常量表"，只读语义由类型系统强制。
- **为什么 Tag 成员不是 `static` 的？** 实例成员放在唯一的静态实例里，等价于静态，但初始化顺序可控（都在 `InitializeNativeGameplayTags` 里赋值）。如果直接写 `static FGameplayTag Damage_Fire;` 则是跨翻译单元的静态初始化顺序问题（Static Initialization Order Fiasco），`AddNativeGameplayTag` 依赖 `UGameplayTagsManager` 单例，而管理器自身的初始化时机不一定早于你的静态成员构造——**所以用"函数内赋值"而不是"静态构造"**。这是 C++ 经典坑：面试常问。
- **`FGameplayTag` 本身是什么**：一个只保存 `FName`（实际是 `TSharedPtr<FName>` 内部指针，见引擎源码 `FGameplayTag::InternalTagIsFName` 相关实现——本质存的是 Tag 节点指针）的轻量结构，可哈希、可比较、可进容器、可网络复制（通过 TagManager 的 Tag Network Id）。

## 5.3 Tag 清单分类精讲（对照 DuraGameplayTags.cpp）

### 5.3.1 主属性 Primary（4 个）

| Tag 名 | 成员变量 | 含义 |
|---|---|---|
| `Attributes.Primary.Strength` | `Attributes_Primary_Strength` | 力量：加物理伤害 |
| `Attributes.Primary.Intelligence` | `Attributes_Primary_Intelligence` | 智力：加法术伤害 |
| `Attributes.Primary.Resilience` | `Attributes_Primary_Resilience` | 韧性：加护甲与护甲穿透 |
| `Attributes.Primary.Vigor` | `Attributes_Primary_Vigor` | 活力：加生命 |

- **为什么属性要有 Tag？** 因为：(a) ExecCalc_Damage 里要按 Tag 从 AttributeSet **捕获（Capture）** 属性值参与伤害公式；(b) UI 的属性菜单按 Tag 显示中文/英文名与描述（`DA_AttributeInfo` 里以 Tag 为 Key）；(c) GE 修改属性靠 Tag 匹配（`GE_DuraPrimaryAttribute` 的 Modifiers 挂的就是这些 Tag）。
- **每个 Tag 第二个参数是描述字符串**：如 `FString("Increases physical damage")`。它出现在编辑器 Tag 选择器的 tooltip 里，给策划看。⚠️ 本项目里大部分 Secondary Tag 的描述是从 Attack 复制的（都写着 "Reduces damage taken, improves Block Chance"），属于注释瑕疵，不影响功能——学习时要能看穿"文案≠逻辑"。

### 5.3.2 次要属性 Secondary（10 个）

`Armor`（护甲）、`Armor_Penetration`（护甲穿透）、`Block_Chance`（格挡几率）、`CriticalHitChance`（暴击率）、`CriticalHitDamage`（暴击伤害）、`CriticalHitResistance`（暴击抗性）、`HealthRegeneration`（生命回复）、`ManaRegeneration`（法力回复）、`MaxHealth`（最大生命）、`MaxMana`（最大法力）。

- 这些是**由主属性派生计算出来的属性**（例如 `MaxHealth = 80 + Vigor * 2.5`），派生公式写在 `MMC_MaxHealth/MMC_MaxMana` 与 GE 常量修饰里（第 4 篇详讲）。
- 注意命名细节：`Attributes_Secondary_Armor_Penetration` 对应 Tag `Attributes.Secondary.Armor_Penetration`——**Tag 层级里下划线保留**，因为层级用 `.` 分隔，属性名内部用 `_` 连接单词。

### 5.3.3 元属性 Meta（1 个）

`Attributes.Meta.IncomingXP`（`Attributes_Meta_IncomingXP`）：暂存"即将到账的 XP"。**Meta 属性的特点**：不参与持久化、不复制、只作为计算中间值——击败敌人时把 XP 写进"给玩家的 IncomingXP"，ExecCalc/OnRep 之外再由 GE 转移到玩家的 XP/Level 属性。

### 5.3.4 输入 Tag InputTag（8 个）

`InputTag.LMB / RMB / 1 / 2 / 3 / 4 / Passive.1 / Passive.2`

- **设计动机（重要）**：增强输入（Enhanced Input）的 `FGameplayTagContainer` 里放一个 InputTag，输入触发时带着这个 Tag 找"绑定了相同 Tag 的技能"。这样**输入与技能完全解耦**：换键位=改输入资产；换技能=改 ASC 授权，两边互不知道对方存在。
- `Passive.1/Passive.2` 是被动技能槽（被动不占主动键位，但仍需输入 Tag 用于装备/卸下操作）。

### 5.3.5 伤害与抗性（Damage & Resistance）

- 伤害类型：`Damage.Fire / Lightning / Arcane / Physical`。
- 抗性：`Resistance.Fire / Lightning / Arcane / Physical`（注意 Tag 根是 `Resistance`，成员变量却叫 `Attributes_Resistance_*`——成员变量名和 Tag 名不必一致，但**变量名对齐"它是个属性"的语义**，因为四个抗性都是 AttributeSet 里的真实属性）。

```cpp
TMap<FGameplayTag, FGameplayTag> DamageTypesToResistances;
TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;
```

- **这两张映射表是本文件最精妙的设计**：`DamageTypesToResistances[Damage_Arcane] = Attributes_Resistance_Arcane`——"什么伤害查什么抗性"。伤害计算（ExecCalc_Damage）拿到伤害类型 Tag 后查表得到抗性属性 Tag，再按 Tag 捕获对方属性，**完全免 switch/if**。
- 新增第五种伤害（比如 `Damage.Ice`）需要改几处？答：加 2 个 Tag（伤害+抗性）、AttributeSet 加 1 个属性、两张表各加 1 行、ExecCalc 不用动。这就是"数据驱动+查表"的可扩展性，面试可以拿它举例讲开闭原则。
- `DamageTypesToDebuffs` 同理把伤害类型映射到减益（火→Burn 灼烧，雷→Stun 眩晕，奥术→Arcane，物理→Physical）。

### 5.3.6 减益 Debuff（8 个）

- 四种类型：`Debuff.Burn/Stun/Arcane/Physical`。
- 四个参数：`Debuff.Chance`（触发概率，作为 **SetByCaller** 的 Key）、`Debuff.Damage`（每跳伤害）、`Debuff.Duration`（持续秒数）、`Debuff.Frequency`（跳伤间隔）。
- **工作方式**：攻击方算出 debuff 参数 → 通过 `FDuraGameplayEffectContext` 网络复制到目标 → 目标的 `DuraAbilitySystemComponent::OnRegister`/`DebuffNiagaraComponent` 监听这些 Tag 的 GE 应用事件 → 挂上对应 Debuff GE（第 6 篇技能链路里完整走一遍）。

### 5.3.7 能力 Ability Tag（技能身份与状态机）

- 身份：`Abilities.Attack / Abilities.Summon / Abilities.Fire.FireBolt / Abilities.Lightning.Electrocute / Abilities.Arcane.ArcaneShards / Abilities.Fire.FireBlast`，被动：`Abilities.Passive.HaloOfProtection / LifeSiphon / ManaSiphon`，以及 `Abilities.None`（占位，等于技能 Tag 界的 nullptr）、`Abilities.HitReact`（受击反应）。
- **技能状态机四态**：`Abilities.Status.Locked → Eligible（可学）→ UnLocked（已学）→ Equipped（已装备）`。技能菜单 UI 就是围绕这个状态机做交互的：等级不够=Locked，升级解锁=Eligible，花点数学习=UnLocked，拖到技能栏=Equipped。
- **技能类型**：`Abilities.Type.Offensive / Passive / None`——决定它在技能菜单里的列位置与能否拖到输入槽。
- **Cooldown Tag**：`CoolDown.Fire.FireBolt`——冷却 GE 的 GrantedTag。技能在 CD 中=身上挂着带该 Tag 的 GE；`WaitCooldownChange` 异步任务监听这个 Tag 的新增/移除来驱动 UI（第 6 篇详讲）。

### 5.3.8 战斗插槽 / 蒙太奇 / 屏蔽 / Cue

- `CombatSocket.Weapon / RightHand / LeftHand / Tail`：**武器/身体插槽**。发射火球前要问"从哪发射？"——用 `GetSocketLocation(CombatSocket_Weapon)` 按类型取位置：玩家从法杖尖、敌人可能从爪子或尾巴。Tag 化之后 C++ 不用关心具体骨骼名，蓝图资产里按 Tag 配骨骼名映射（`CombatInterface::GetCombatSocketLocation` 的实现）。
- `Montage.Attack.1~4`：敌人攻击蒙太奇的**随机池索引 Tag**。敌人攻击时按 Tag 从 `AttackMontages` 数组里随机取一段播放（`DuraEnemy` 里的 `AttackMontageTag` 逻辑）。
- `Player.Block.InputPressed / InputHeld / InputReleased / CursorTrace`：**输入屏蔽 Tag**。本质是 4 个普通的 Tag，但约定用途：当技能正在执行（如鼠标拖动施法）时，`DuraPlayerController` 检查 `AbilitySystemComponent->HasMatchingGameplayTag(Player_Block_InputPressed)` 决定是否把输入事件转发给技能。**用 Tag 做互斥锁**——技能开始时 `AddLooseGameplayTag`，结束时移除，输入层"挂起"——这是 GAS 里控制输入流向的惯用法。
- `GameplayCue.FireBlast`：FireBlast 爆炸的 GameplayCue Tag。GAS 里视觉/音效效果统一走 Cue 系统（自动处理网络复制、拆分粒子/音效/震动），C++ 端只触发 Tag。

## 5.4 注册函数本体模式

```cpp
DuraGameplayTags.Attributes_Primary_Strength = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Attributes.Primary.Strength"),
    FString("Increases physical damage"));
```

- `UGameplayTagsManager::Get()`：Tag 管理器单例。
- `AddNativeGameplayTag(FName, FString)`：参数 1 是层级化 Tag 名（**必须是 `FName`，因为 Tag 内部按 FName 哈希查找**）；参数 2 是编辑器显示的注释。返回填好的 `FGameplayTag`（内部指针指向管理器维护的 Tag 节点）。
- **重复注册同一个名字会怎样？** `AddNativeGameplayTag` 内部会 `ensure` 失败并报错——所以 `InitializeNativeGameplayTags` 只能被调用一次；本项目在 AssetManager 启动钩子里调用，天然只跑一次。
- **为什么 Tag 字符串用点分层级？** 管理器会构建 Tag 树，`Abilities.Fire.FireBolt` 隐含拥有 `Abilities.Fire` 与 `Abilities` 两个父 Tag 的语义（`MatchesTag` 前缀匹配）。比如身上有 `Abilities.Fire.FireBolt` 的角色 `HasTag(Abilities.Fire)` 为 true——火系通用逻辑可以只匹配父 Tag。

## 5.5 通俗解释：把 Tag 系统类比成"快递面单"

游戏里每个状态变化都要让多个系统知道：敌人中了火球 → 要扣血（属性系统）、要显示伤害数字（UI）、可能要灼烧（Debuff）、要播受击动画（动画系统）。
如果这些系统互相调用，就像每家店铺之间拉专线电话——线越多越乱。
Gameplay Tag 相当于**统一格式的快递面单**：火球事件在包裹上贴 `Damage.Fire` 面单，属性系统看到 `Damage` 就签收扣血，UI 看到就打印数字，Debuff 系统看到 `Debuff.Burn` 就给目标续上灼烧。没人直接认识谁，全靠面单。

## 5.6 面试考点清单

1. **Native Tag 与 Config Tag 的区别与取舍？**（编译期安全 vs 策划可自由配置；代码要当标识符用的一定用 Native）
2. **`MatchesTag` vs `MatchesTagExact`？**（前缀层级匹配 vs 精确匹配；`HasAnyMatchingGameplayTags` 用于容器）
3. **Loose Tag 与普通 Tag？**（`AddLooseGameplayTag` 不进复制与 Tag 计数管理的完整流程，只在本进程生效，常用于本地输入屏蔽）
4. **为什么不用枚举而用 Tag？**（枚举要改代码+switch 分发，Tag 可由策划在蓝图配置、支持层级、支持容器运算（并/交/差））
5. **Tag 的网络复制原理？**（Tag 被压缩成 `FActiveGameplayTag` 的 NetworkId，双方各维护一张对照表）

---

# 六、DuraAbilitiesTypes.h —— 自定义 GE 上下文（本篇第二核心 ★★★★★）

这是**整个项目网络战斗数据流的"隐形血管"**：格挡、暴击、Debuff、击退、死亡冲量、径向伤害……所有"伤害附带信息"都靠这个结构体从攻击方服务器流到受击方（含客户端表现）。

## 6.1 两个结构体的分工

```cpp
FDamageEffectParams      // "伤害参数包"：施法者 → GE 的入参集合（蓝图可读写）
FDuraGameplayEffectContext  // "伤害上下文"：GE 执行期间的伴随信息包（可网络复制）
```

- **Param 是"发起方填的单子"**：施法者把伤害数值、类型、debuff 参数、击退方向填进 Param，一次性传给 `AbilitySystemLibrary::ApplyDamageEffect`。
- **Context 是"随行证"**：GE 内部计算（ExecCalc）时从 Context 读/写格挡暴击结果；GE 应用到目标后，目标侧（如 `DuraAbilitySystemComponent`、`DebuffNiagaraComponent`、`DamageTextComponent`）再从 Context 读结果做表现。
- **为什么要两份？** GE 的 API 面向"修改器/计算器"，只能携带 `FGameplayEffectContext`；Param 只是项目层方便聚合参数的 DTO。`ApplyGameplayEffectSpecToTarget` 时 Param 的内容被拆解写进 Context 与 SetByCaller，各走各的通道。

## 6.2 FDamageEffectParams 逐字段精讲

| 字段 | 类型/默认值 | 用途与细节 |
|---|---|---|
| `WorldContextObject` | `TObjectPtr<UObject> = nullptr` | 提供"在哪个世界"的上下文。为什么需要？静态函数（如 Library 函数）没有 this/World，要靠它 `GEngine->GetWorldFromContextObject` 找到 UWorld 来做 Spawn、判断网络模式。传 Actor 就行（Actor 有 GetWorld()） |
| `DamageGameplayEffectClass` | `TSubclassOf<UGameplayEffect> = nullptr` | 要应用的伤害 GE 类，通常蓝图里配 `GE_Damage`。用 `TSubclassOf` 而非裸 UClass 是为了蓝图编辑器里只能选 GE 类型，类型安全 |
| `SourceAbilitySystemComponent` | `TObjectPtr<UAbilitySystemComponent>` | 攻击方 ASC：ExecCalc 要从它捕获攻击者属性（暴击率、伤害加成等），Context 里 Instigator 也由它提供 |
| `TargetAbilitySystemComponent` | 同上 | 受击方 ASC：GE 应用目标 |
| `BaseDamage` | `float = 0.f` | 基础伤害，通过 **SetByCaller(Magnitude, Data.Damage)** 注入 GE（名字是约定字符串，蓝图里配） |
| `AbilityLevel` | `float = 1.f` | 技能等级：伤害曲线、debuff 参数都按等级取表 |
| `DamageType` | `FGameplayTag()` | 伤害类型 Tag，进 Context 供目标侧查抗性/选 Debuff |
| `DebuffChance/Damage/Duration/Frequency` | `float = 0.f` | 四个 debuff 参数，SetByCaller 写入 GE，ExecCalc 掷骰后复制进 Context |
| `DeathImpulseMagnitude` / `DeathImpulse` | `float` / `FVector` | 致死冲击：目标死亡时尸体被击飞的力。Magnitude 是"策划配的力度"，Impulse 是"算完方向的实际向量" |
| `KnockbackMagnitude/Chance/Force` | 同上 | 击退：Chance 决定是否触发，Force 是方向+大小 |
| `bIsRadialDamage` + Inner/Outer/Origin | `bool`/`float`/`FVector` | 径向伤害（FireBlast 爆炸）：内外半径衰减 + 中心点。ExecCalc 判断此标记后走径向衰减公式 |

**设计模式观察**：这本质是 **Parameter Object（参数对象）重构**——把十几个散参数打包。函数签名从 `ApplyDamageEffect(World, GEClass, Source, Target, Damage, Level, Type, Chance, Damage, Duration, ...)` 十六个参数变成 2 个，可读性/可维护性大增。面试讲"函数参数过多怎么重构"，这是标准答案。

## 6.3 FDuraGameplayEffectContext 逐函数精讲

### 6.3.1 为什么继承 FGameplayEffectContext

`FGameplayEffectContext` 是 GE 的伴随信息基类，内含 Instigator（发起者）、EffectCauser（直接作用者）、HitResult、Actors 数组、WorldOrigin 等。GAS 内部所有"执行期"代码拿到的都是它的指针——**项目要附加自定义数据，标准做法就是继承它 + 替换 Alloc 函数**（见第 7 章 DuraAbilitySystemGlobals）。

### 6.3.2 Getter/Setter 风格

```cpp
bool IsBlockedHit() const { return bIsBlockedHit; }
void SetIsBlockedHit(bool bInIsBlockHit) { bIsBlockedHit = bInIsBlockHit; }
```

- 为什么封装而不是 public 成员？**控制写入口**：任何地方想改格挡状态必须走 Setter，便于打日志/断点/以后加校验。ExecCalc 会在一次执行中先读基础值再写回结果，封闭写口防止意外污染。
- `GetDamageType()` 返回 `TSharedPtr<FGameplayTag>`：注意不是裸指针也不是值。**为什么是 SharedPtr**——(a) Context 会被 Duplicate 深拷贝多份（服务器一份、客户端表现一份），值拷贝会无谓开销；(b) 引擎原版 Context 的 `DamageType` 字段就是 `TSharedPtr<FGameplayTag>`（`FGameplayEffectContext::DamageType` 官方即如此），跟引擎对齐可以无缝协作。

### 6.3.3 GetScriptStruct / Duplicate / NetSerialize —— 三大虚函数（GAS 面试必考）

```cpp
virtual UScriptStruct* GetScriptStruct() const override
{
    return StaticStruct();
}
```
- 作用：告诉引擎"我这个 Context 的反射类型是什么"。引擎序列化/复制 Context 时需要知道具体类型来调对应逻辑（`FGameplayEffectContext::GetScriptStruct` 默认返回自身静态结构）。`StaticStruct()` 是 `GENERATED_BODY()` 为 USTRUCT 生成的静态函数，返回本结构体的 `UScriptStruct`。
- **通俗解释**：快递单上盖一个"我是 FDura 版面单"的章，分拣系统照章处理。

```cpp
virtual FDuraGameplayEffectContext* Duplicate() const override
{
    FDuraGameplayEffectContext* NewContext = new FDuraGameplayEffectContext();
    *NewContext = *this;
    if (GetHitResult())
    {
        NewContext->AddHitResult(*GetHitResult(), true);  // 深拷贝命中结果
    }
    return NewContext;
}
```
- 作用：引擎在复制/内部传递时会拷贝 Context。默认 `FGameplayEffectContext::Duplicate` 拷贝基类字段；**不重写的话，派生字段（暴击/debuff/击退）在拷贝中丢失**——这就是许多"客户端看不到暴击数字"问题的根源。
- `*NewContext = *this`：USTRUCT 的 `=` 走 `UScriptStruct::CopyScriptStruct`（反射拷贝，逐 UPROPERTY 复制，包括派生字段）。
- `AddHitResult(*GetHitResult(), true)`：第二个参数 `bCopyHitResult=true` 表示**深拷贝** HitResult。为什么必要？HitResult 里 `HitObjectReference`/物理材质等是共享指针/外部数据，浅拷贝可能悬垂。
- 返回裸指针 `new`：引擎约定，谁拿走谁释放（Context 生命周期由 `FGameplayEffectSpec` 管理）。

```cpp
virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
```
- 作用：**手写网络序列化**。`Ar` 是序列化流（Saving=打包发出去，Loading=接收解包）；`Map` 把 Actor 引用压缩成编号；`bOutSuccess` 告诉引擎本次序列化成败（失败会重置连接的可靠通道）。
- **为什么不用 UPROPERTY 自动复制？** Context 是**结构体字段级**复制且要极致省流量——GE 的 Context 每次伤害都传输，字段自动复制会带齐所有字段+反射开销；手写位标记可以"哪个字段有值就只发哪个"，把带宽压到最低。这是网络优化经典手法。

### 6.3.4 NetSerialize 逐位拆解（NetSerialize.cpp 全文）

核心模式是**位掩码（bitmask）三段式**：

```cpp
uint32 RepBits = 0;
if (Ar.IsSaving())   // 第一段：发送方统计哪些字段非零，置位
{
    if (bReplicateInstigator && Instigator.IsValid()) RepBits |= 1 << 0;
    ...
}
Ar.SerializeBits(&RepBits, 19);   // 第二段：把 19 位掩码本体写进流（双方都要执行）
if (RepBits & (1 << 0)) { Ar << Instigator; }   // 第三段：按掩码逐字段收发
```

- **`RepBits` 的每个位**（1<<0 到 1<<19，共 20 个语义但掩码只传 19 位，因为位 16 是"径向伤害开关"另含 3 个子位——见下）：
  - 位0~6：引擎原生字段（Instigator、EffectCauser、AbilityCDO、SourceObject、Actors 数组、HitResult、WorldOrigin）。前四个有 `bReplicate*` 开关控制——**Context 上有 `bReplicateInstigator` 等标志位**，不勾就不复制，进一步省流量。
  - 位7~15：项目新增（bIsBlockedHit、bIsCriticalHit、bIsSuccessfulDebuff、DebuffDamage/Duration/Frequency>0、DamageType 有效、DeathImpulse 非零、KnockbackForce 非零）。
  - 位16~19：径向伤害四件套——**位16 是开关，位17/18/19 是嵌套子位**（内半径>0、外半径>0、原点非零）。嵌套结构说明"没开径向伤害时三个子字段根本不会出现"，树状压缩。
- **`Ar.SerializeBits(&RepBits, 19)`**：按位序列化 19 位（不足一字节也按位打包）。**注意**：`SerializeBits` 传的是位数；代码里最高用了 `1 << 19`，加上嵌套语义实际需要 20 位表达（0~19），传 19 是**潜在 off-by-one 隐患**——位19（RadialDamageOrigin）在校验 `RepBits & (1 << 19)` 时用的是接收端自己解出的 RepBits，而掩码本体只发了低 19 位，第 20 位（位19）会丢失。实际上这行原样来自课程代码，且因为位19 只在位16（径向）为真时才置位，其信息会随位16 一起丢失/恢复不可靠——**学习时要指出这是可修复的 bug**（改成 20 或 32）。*这是一个真实的代码审查点：说明"掩码位数必须 ≥ 最大置位位数"。*
- **`Ar << Instigator`**：`operator<<` 对 `TWeakObjectPtr`/`TObjectPtr` 的序列化会通过 `Map` 把 Actor 映射为 NetGUID，接收端还原为指针。跨机器引用同一 Actor 靠这个。
- **`SafeNetSerializeTArray_Default<31>(Ar, Actors)`**：带安全上限的数组序列化（最多 31 个元素，防止恶意/异常数据撑爆数组）。
- **HitResult 分支**：Loading 时先 `new FHitResult()` 再调 `HitResult->NetSerialize`（FHitResult 自己有网络序列化实现，压缩命中位置/法线/骨骼名）。
- **DamageType 分支**：同样 Loading 时先建空 `FGameplayTag`，再 `DamageType->NetSerialize(...)`——`FGameplayTag::NetSerialize` 内部把 Tag 压缩成一个整数 ID（通过 TagManager 的网络注册表），非常省。
- **收尾**：
  ```cpp
  if (Ar.IsLoading())
  {
      AddInstigator(Instigator.Get(), EffectCauser.Get());
  }
  ```
  `AddInstigator` 除了存两个指针，还**顺带解析出 Instigator 的 ASC 存进 `InstigatorAbilitySystemComponent`**（基类逻辑）。如果不调它，接收端的 Context 能拿到 Instigator Actor 但拿不到它的 ASC——而 ExecCalc/目标侧恰恰要用 InstigatorASC。**这行注释"只是为了初始化 InstigatorAbilitySystemComponent"精确点出了动机**。
- **`bOutSuccess = true; return true;`**：永远报告成功（本项目不做失败分支）。

### 6.3.5 TStructOpsTypeTraits —— 结构体的"能力声明"

```cpp
template<>
struct TStructOpsTypeTraits< FDuraGameplayEffectContext > : public TStructOpsTypeTraitsBase2< FDuraGameplayEffectContext >
{
    enum { WithNetSerializer = true, WithCopy = true };
};
```
- 这是 UE 的**编译期结构体能力标签**：`WithNetSerializer=true` 告诉通用序列化框架（如 `FScriptStructNetSerializer`、Property Replication）"本结构体自带 NetSerialize，走我的"；`WithCopy=true` 表示支持按值拷贝（Duplicate 需要）。
- **不加会怎样**：NetSerialize 根本不会被网络层调用，自定义数据在多人游戏里悄悄丢失——又是"客户端不显示暴击"的典型病因。
- **通俗解释**：像给快递箱贴"易碎/可平放"贴纸，物流系统按贴纸选择处理方式。不贴贴纸，你的"易碎"保护程序没人执行。

## 6.4 全链路串讲：一次火球命中的 Context 之旅

1. **发起**（`DuraProjectileSpell/DuraProjectile`，服务器）：命中回调里构造 `FDamageEffectParams`，填 BaseDamage（按等级取曲线）、DamageType=Fire、DebuffChance=0.15 等。
2. **打包**（`DuraAbilitySystemLibrary::ApplyDamageEffect`）：`MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, AbilityLevel)` 生成 Spec；`Spec.AddSetByCallerMagnitude(Tag_Damage, BaseDamage)` 注入伤害；`Spec.GetContext()` 拿到 Context（此时已是 FDura 类型，因为 Globals 替换了 Alloc）。
3. **执行**（`ExecCalc_Damage`，服务器）：捕获双方属性 → 查 `DamageTypesToResistances` 表 → 算抗性/护甲/暴击/格挡 → **把结果写进 Context**（`SetIsCriticalHit(true)`、SetDebuffDamage...）→ Context 随 Spec 被应用。
4. **结算**：GE Execute 后修改 Health 属性 → `DuraAttributeSet::PostGameplayEffectExecute` 读回 Context 判断暴击/格挡 → 决定伤害数字颜色、死亡冲量（往 `DuraCharacterBase::Die` 传 DeathImpulse）。
5. **复制到客户端**：Spec 携带 Context 走 `FActiveGameplayEffect::NetSerialize` → 调我们重写的 `NetSerialize` → 客户端重建 Context → `DamageTextComponent` 监听属性变化后从 ASC 的 `GetEffectContext` 读出"是否暴击"显示不同数字样式。

## 6.5 面试考点清单

1. **如何给 GAS 伤害附带自定义数据（暴击/击退）并保证多人正确？** —— 继承 FGameplayEffectContext + GetScriptStruct/Duplicate/NetSerialize 三件套 + TStructOpsTypeTraits 声明 + AbilitySystemGlobals::AllocGameplayEffectContext 替换（下一章）。
2. **NetSerialize 的位掩码模式为什么高效？**（有值才传，O(有效字段)；一次 SerializeBits + 条件字段）
3. **为什么 Duplicate 要深拷贝 HitResult？**（共享指针跨拷贝悬垂）
4. **SetByCaller 是什么，和 Modifier Op 数值写死有何区别？**（运行时按 FGameplayTag 注入数值，同一份 GE 资产配不同参数；写入方 `Spec.AddSetByCallerMagnitude`，GE 里 Modifier 配 Data 分支）
5. **上下文与参数包（Param）的职责划分？**（Param=蓝图友好 DTO；Context=引擎管线携带物+网络复制载体）

---

# 七、DuraAbilitySystemGlobals —— 把自定义上下文"插"进 GAS

## 7.1 官方层面

`UAbilitySystemGlobals` 是 GAS 的全局配置单例（读取 `DefaultGame.ini` 的 `[/Script/GameplayAbilities.AbilitySystemGlobals]` 段），负责：初始化全局 Tag 表、管理 GameplayCueManager、以及**每次新建 GE Context 时调用 `AllocGameplayEffectContext()`**。

项目定制方式同 AssetManager：ini 里配
```ini
[/Script/Engine.Engine]
; （项目里实际配置位置）
GameSubsystem / 或
[/Script/GameplayAbilities.AbilitySystemGlobals]
GlobalSystemGameplayTags...
```
关键在于 `Config/DefaultEngine.ini` 或 `DefaultGame.ini` 中：
```ini
[/Script/GameplayAbilities.AbilitySystemGlobals]
AbilitySystemGlobalsClassName="/Script/Dura.DuraAbilitySystemGlobals"
```
（子类必须叫 `*AbilitySystemGlobals` 才能被引擎识别替换——引擎对类名后缀有约定校验。）

## 7.2 逐行拆解

```cpp
virtual FGameplayEffectContext* AllocGameplayEffectContext() const override
{
    return new FDuraGameplayEffectContext();
}
```

- **调用时机**：每次 `FGameplayEffectSpec` 构造（即每次 `MakeOutgoingGameplayEffectSpec`/`MakeEffectSpec`）都会 `GetAbilitySystemGlobals().AllocGameplayEffectContext()` 拿一个新 Context。
- **const 成员函数**：不改全局状态，纯工厂方法。
- **返回裸指针**：所有权转移给 Spec，由 `FGameplayEffectSpec::EffectContext` 的 TSharedPtr 管理。
- **如果不做这个替换**：所有 Context 都是基类类型，ExecCalc 里 `Context.IsCriticalHit()` 这样的派生接口根本不存在（编译期就写不了），运行期 Cast 也会失败——**这是"自定义数据进 GAS 管线"的第一块多米诺骨牌**。

## 7.3 通俗解释

GAS 管线上有一个"证件发放窗口"（AllocGameplayEffectContext），每个技能效果出生时都要来这里领证件。默认窗口只发"通用身份证"，我们在 ini 里把自己的窗口挂上去，从此每个效果都自带"Dura 特别版证件"，上面可以写暴击、击退这些私有字段。

## 7.4 面试考点

1. **UAbilitySystemGlobals 能定制什么？**（Context 分配、CueManager、全局 Tag、GameplayCue 的 replication 策略）
2. **为什么不直接改引擎源码？**（引擎升级兼容性、模块边界；UE 的替换点设计就是为此）

---

# 八、本章全链路总图

```
引擎启动
 └─ FEngineLoop::Init
     └─ UEngine::Init
         └─ UAssetManager::Get()            ← ini: AssetManagerClassName=UDuraAssetManager
             └─ UDuraAssetManager::StartInitialLoading()
                 ├─ Super::StartInitialLoading()
                 └─ FDuraGameplayTags::InitializeNativeGameplayTags()
                     └─ UGameplayTagsManager::AddNativeGameplayTag(...) ×N
                        （属性/输入/伤害/抗性/Debuff/技能/冷却/插槽/屏蔽/Cue 全注册）

第一次 ApplyGameplayEffectSpec
 └─ UAbilitySystemGlobals::AllocGameplayEffectContext()   ← ini: AbilitySystemGlobalsClassName
     └─ new FDuraGameplayEffectContext()
         ├─ ExecCalc_Damage 写入 暴击/格挡/Debuff/击退
         └─ NetSerialize 位掩码复制 → 客户端表现（伤害数字/Debuff 挂件/死亡冲量）
```

# 九、动手实验建议（学完必做）

1. 在 `DuraGameplayTags.cpp` 里新增 `FGameplayTag Damage_Ice;` + 注册 `Damage.Ice`，并在两张映射表加行，编译通过后打开技能蓝图验证 Tag 选择器里能搜到。
2. 故意把 `Ar.SerializeBits(&RepBits, 19)` 保持不变，给 `bIsBlockedHit` 之外新增一个位 20 的字段，观察 PIE 网络模式下数据错乱/丢失，再修复位数——体感理解掩码 bug。
3. PIE 里开两个窗口（Network Mode: Listen Server + Client），在 `NetSerialize` 打日志，验证服务器 Saving / 客户端 Loading 各调一次，理解序列化的对称调用。

---

*下一篇：`02-游戏模式与角色诞生链路.md` —— GameMode/GameInstance 如何把 ASC/AttributeSet 安装到玩家和敌人身上。*
