# 02 · 游戏模式与角色诞生链路 —— ASC 安装在谁身上、服务器/客户端双轨初始化

> **逻辑链路一句话概括**：
> `GameMode 持有全局数据资产与存档职责` → `玩家角色 DuraCharacter 诞生时，ASC/AttributeSet 不在角色身上，而是放在 DuraPlayerState 里（服务器走 PossessedBy，客户端走 OnRep_PlayerState，两条轨道汇合到 InitAbilityActorInfo）` → `敌人 DuraEnemy 把 ASC 挂在自己身上，PossessedBy 时一次性初始化并启动行为树` → `所有战斗相关的外部问答（等级/插槽/死亡/蒙太奇）统一走 ICombatInterface`。
> **这条链路回答了 GAS 第一个必考问题：ASC 到底该放在哪？为什么？**

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/Interaction/CombatInterface.h` | 战斗接口 + 三大委托 + FTaggedMontage | ★★★★★ |
| 2 | `Source/Dura/Public/Character/DuraCharacterBase.h` / `.cpp` | 玩家与敌人的公共基类 | ★★★★★ |
| 3 | `Source/Dura/Public/Player/DuraPlayerState.h` / `.cpp` | 玩家 ASC 的宿主 + 经验/等级/点数 | ★★★★★ |
| 4 | `Source/Dura/Public/Character/DuraCharacter.h` / `.cpp` | 玩家角色：双轨初始化、存读档 | ★★★★★ |
| 5 | `Source/Dura/Public/Character/DuraEnemy.h` / `.cpp` | 敌人角色：自持 ASC、AI 启动 | ★★★★☆ |
| 6 | `Source/Dura/Public/Game/DuraGameModeBase.h` / `.cpp` | 数据资产中心、存档、重生 | ★★★★☆ |
| 7 | `Source/Dura/Public/Game/DuraGameInstance.h` / `.cpp` | 跨地图存续的"会话背包" | ★★★☆☆ |

---

# 一、先建立世界观：UE 网络角色框架（铺垫，务必先懂）

在读懂本篇之前必须理解 UE 的 **Pawn/Controller/PlayerState 三分法**：

- **Pawn（角色）**：可以销毁重生（死了就换一个），是"临时的身体"。
- **Controller**：控制身体的"灵魂"，玩家的是 PlayerController，AI 是 AIController。换身体不换灵魂。
- **PlayerState**：玩家的"户籍档案"，**跨身体存续**——死亡重生后 Pawn 换了新的，PlayerState 还是同一个。且天然被复制到所有客户端（每个人都能看到别人的分数/等级）。

**由此推出 GAS 的经典设计决策**：玩家的 ASC 和 AttributeSet 应该挂在 **PlayerState** 上（数据跟随"户口"而不是"身体"）；敌人的 ASC 直接挂在**敌人自己**身上（敌人死透就没了，不需要跨身体）。

- **通俗解释**：玩家像打工人换工牌换电脑（Pawn），但社保账号（PlayerState）不变；敌人像一次性 NPC，死了连档案带尸体一起扔。
- **面试考点（GAS 高频第一题）**：*"ASC 放在 Pawn 还是 PlayerState？"* 标准答案：多人游戏玩家放 PlayerState（重生不丢技能/属性、全客户端可见），单人/AI 放 Pawn（简单直接）。本项目两种都实现了，正好对照学习。

---

# 二、CombatInterface —— 战斗系统的"问答总线" ★★★★★

## 2.1 为什么需要接口

技能释放过程中，C++ 需要问执行者一大堆问题：
- "你几级？"（算伤害曲线）→ `GetPlayerLevel`
- "火球从哪发出来？"（发射点）→ `GetCombatSocketLocation`
- "你死了吗？"（能否继续攻击）→ `IsDead`
- "你的死亡/受伤/ASC注册委托给我一份"（订阅事件）→ `GetOnDeathDelegate` 等

这些问题玩家和敌人都要答，但**答案的实现完全不同**（玩家等级在 PlayerState，敌人等级是自己的成员变量）。UE 的解法就是 **Interface**：约定"问题清单"，谁实现谁回答。调用方 `ICombatInterface::Execute_GetPlayerLevel(Actor)` 只管问，不关心对面是谁。

## 2.2 三大委托声明精讲

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCRegistered, UAbilitySystemComponent*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, DeadActor);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDamageSignature, float /* DamageAmount */);
```

- **两种委托宏的区别（高频考点）**：
  - `DECLARE_MULTICAST_DELEGATE_*`：**非动态**委托，只能 C++ 绑定（AddLambda/AddUObject），更快（直接函数指针调用），**不能**被蓝图绑定。
  - `DECLARE_DYNAMIC_*`：**动态**委托，走反射按名字调用，慢一点，但 **蓝图可以绑定**（`UPROPERTY(BlueprintAssignable)` 只能挂动态委托）。
- 为什么 OnDeath 用动态的？因为死亡要通知蓝图（比如蓝图写的尸体处理、任务奖励）；OnASCRegistered/OnDamage 纯 C++ 消费（HUD 初始化、伤害数字），所以用非动态。
- **委托命名约定**：`F` 开头 + 签名语义（Signature）。

## 2.3 FTaggedMontage —— "带标签的攻击蒙太奇"

```cpp
USTRUCT(BlueprintType)
struct FTaggedMontage
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) UAnimMontage* Montage = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag MontageTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag SocketTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) USoundBase* ImpactSound = nullptr;
};
```

- 四个字段合起来描述"一次攻击动画"：动画本体、动画的 Tag（`Montage.Attack.1`）、发射点 Tag（`CombatSocket.Weapon`）、命中音效。
- **为什么打包成结构体？** 攻击技能（近战/弹道）需要"动画+发射点+音效"三位一体地随机选择/传递。打包后 `GetAttackMontages()` 返回数组、`GetTaggedMontagedByTag(Tag)` 按 Tag 查找——数据原子性，避免传三个散参数。
- **SocketTag 的妙用**：播完动画后技能要问"武器/左手/尾巴在哪"，把 SocketTag 传给 `GetCombatSocketLocation`——上一章 CombatSocket Tag 的消费点。

## 2.4 接口函数声明的三种形态（重要语言知识点）

CombatInterface 里混用了三种声明，这是 UE 接口的完整教科书：

```cpp
UFUNCTION(BlueprintNativeEvent)
int32 GetPlayerLevel() const;          // ① 蓝图可覆盖、C++ 也有默认实现的接口函数

virtual void Die(const FVector& DeathImpulse) = 0;   // ② 纯虚 C++ 函数（不走反射）

UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
void UpdateFacingTarget(const FVector& Target);      // ③ 纯蓝图实现：C++ 只声明"必须有人做"，实现全在蓝图
```

| 形态 | C++ 实现 | 蓝图实现 | 调用方式 |
|---|---|---|---|
| ① BlueprintNativeEvent | 可写 `_Implementation` 默认版 | 可覆盖 | `Execute_GetPlayerLevel(Actor)` |
| ② 纯虚 | 必须实现 | 不可见 | 直接 `Actor->Die(...)`（Cast 后）或 `Execute_*` 均可（无需反射标记） |
| ③ BlueprintImplementableEvent | 不允许 | 必须实现 | `Execute_UpdateFacingTarget(Actor, ...)` |

- **`Execute_` 前缀的原理**：UHT（UnrealHeaderTool）为接口函数生成静态转发函数，内部先 `Cast` 到接口再调用；对未实现接口的 Actor 调用会 `check` 失败。**先 `Actor->Implements<UCombatInterface>()` 判断再 Execute 是标准防御写法**（本项目多处如此，如 GameMode 的存档循环）。
- **`Die` 为什么是纯虚而不是 BlueprintNativeEvent？** 死亡流程必须 C++ 可靠执行（物理、蒙太奇、委托），不允许蓝图覆盖绕过；而"蓝图可能想改的"（如 UpdateFacingTarget 的动画朝向）才开放给蓝图。
- **面试考点**：*"BlueprintImplementableEvent 与 BlueprintNativeEvent 区别？接口函数为什么有 Execute_ 前缀？"*

---

# 三、DuraCharacterBase —— 玩家与敌人的公共基类 ★★★★★

## 3.1 类声明拆解

```cpp
UCLASS(Abstract)
class DURA_API ADuraCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
```

- **`UCLASS(Abstract)`**：抽象类，禁止在编辑器里直接把它当父类放置/生成实例。语义防护：基类没有 ASC 初始化的具体实现，放出来就是坏的。
- **`IAbilitySystemInterface`**：GAS 的"通行证"接口，只要求实现 `GetAbilitySystemComponent()`。**为什么必须有它**：`UAbilitySystemGlobals`、`GameplayCueManager` 等引擎 GAS 代码会 `Cast<IAbilitySystemInterface>(Actor)` 来找 ASC——不实现这个接口，很多引擎内部逻辑（如 Cue 广播、AbilitySystemLibrary 的便捷函数）找不到你的 ASC。
- **成员变量命名注意**：本项目的 ASC 成员叫 `AbilitiesSystemComponent`（少个 y，笔误但全局一致，读代码时别懵）。

## 3.2 构造函数逐行精讲（每一行都在解决一个实际问题）

```cpp
ADuraCharacterBase::ADuraCharacterBase()
{
    PrimaryActorTick.bCanEverTick = true;
```
- 开 Tick 是为了第 64 行的小需求（EffectAttachComponent 保持世界旋转）。**代价**：每个角色每帧 Tick。可优化点：这种"每帧做一件小事"其实可用 Timer 或只在旋转时更新——学习时的批判点。

```cpp
    BurnDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("BurnDebuffComponent");
    BurnDebuffComponent->SetupAttachment(GetRootComponent());
    BurnDebuffComponent->DebuffTag = FDuraGameplayTags::Get().Debuff_Burn;
```
- `CreateDefaultSubobject`：**构造函数里**创建组件的唯一正确方式（CDO 拥有组件，实例共享结构）。名字字符串是组件对象名，必须唯一。
- `SetupAttachment(GetRootComponent())`：挂到胶囊体根下，跟随整体移动。
- `DebuffTag = ...Debuff_Burn`：**构造函数里就能读 Tag**！因为 AssetManager 的 StartInitialLoading（第 1 篇）早于任何 UObject 构造（CDO 创建之后才可能进关卡），这就是"Tag 为什么必须尽早注册"的直接受益。DebuffNiagaraComponent 拿到 Tag 后自己监听"身上挂了带该 Tag 的 GE"事件，激活/关闭对应粒子（灼烧火焰/眩晕星星）——**组件自管理**，基类不用写任何激活逻辑。

```cpp
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetCapsuleComponent()->SetGenerateOverlapEvents(false);
    GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
    GetMesh()->SetGenerateOverlapEvents(true);
```
- **碰撞布局的意图**：
  - 相机通道 Ignore：俯视角相机不与角色体积碰撞（不然镜头被弹开）。
  - **胶囊体关 Overlap、骨骼体（Mesh）开 Overlap**：子弹/拾取判定在"身体"上而不是"圆柱"上——胶囊是抽象柱体，Mesh 是真实模型，命中 Mesh 才有"打在身体上"的准确反馈。
  - `ECC_Projectile`（第 1 篇的宏）设为 **Overlap**：火球飞过来与 Mesh 重叠即触发 `OnComponentBeginOverlap` 回调（在 DuraProjectile 里，第 12 篇）。

```cpp
    Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
    Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
    Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```
- 武器是**骨骼网格**（有骨骼才能随挥动动画摆动），挂在骨架的 `WeaponHandSocket` 插槽上——注意是挂 Mesh 不是根，武器跟着手走。
- 默认无碰撞：武器只是视觉，伤害判定走 GAS（GE），不是物理。

```cpp
    EffectAttachComponent = CreateDefaultSubobject<USceneComponent>("EffectAttachComponent");
    // 三个被动光环 Niagara 挂它下面，各自配置 PassiveSpellTag
```
- `EffectAttachComponent` 是**特效挂点容器**：三个被动技能光环（保护光环/吸血/吸蓝）统一挂它下面。它存在的唯一意义是**统一管理旋转**——见 Tick：

```cpp
void ADuraCharacterBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    EffectAttachComponent->SetWorldRotation(FRotator::ZeroRotator);
}
```
- **为什么每帧把它旋回零？** 角色转身时，父级（角色）旋转会**连带**子级（光环）旋转，而光环特效（一圈光柱）应该永远朝上。每帧强制世界旋转归零 = 抵消父级旋转，效果等价"不受角色朝向影响"。Niagara 本身也有 `bFixedBounds`/局部空间设置，但组件级归零是最简单粗暴的稳定做法。

## 3.3 网络复制声明

```cpp
void ADuraCharacterBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADuraCharacterBase, bIsStunned);
    DOREPLIFETIME(ADuraCharacterBase, bIsBurned);
    DOREPLIFETIME(ADuraCharacterBase, bIsBeingShocked);
}
```
- **约定三连**：`UPROPERTY(ReplicatedUsing=OnRep_X)` 声明 + `GetLifetimeReplicatedProps` 里 `DOREPLIFETIME` 注册 + `OnRep_X` 客户端回调。
- `bIsStunned/bIsBurned` 用 `ReplicatedUsing`（要回调），`bIsBeingShocked` 用纯 `Replicated`（只是状态标志，客户端无表现需求，Electrocute 技能防止双重麻痹的互斥标志）。
- **面试考点**：*"Replicated 和 ReplicatedUsing 区别？DOREPLIFETIME 与 DOREPLIFETIME_CONDITION 有何不同？"*

## 3.4 TakeDamage —— 引擎伤害管线与 GAS 的桥

```cpp
float ADuraCharacterBase::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float DamageTaken = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
    OnDamageDelegate.Broadcast(DamageTaken);
    return DamageTaken;
}
```

- **背景**：UE 有自己的伤害管线（`AActor::TakeDamage`，由 `UGameplayStatics::ApplyDamage` / 点伤害/范围伤害触发）。本项目 **GAS 伤害不走这条路**（走 GE），但引擎的其他系统（如某些碰撞伤害、AI 行为）仍可能调它。
- **这里的用途**：把"传统管线收到的伤害"广播给 `OnDamageDelegate`——谁订阅了？**DamageTextComponent（伤害数字）**订阅它显示数字。也就是说本项目有两路伤害：GAS 路（GE 修改 Health 属性 → PostGameplayEffectExecute → 广播 OnDamageDelegate）和引擎路（TakeDamage → 广播）。两路最终都汇到同一个 UI 组件。
- 参数精讲：`Damage` 原始伤害；`DamageEvent` 具体伤害类型（点伤害/径向伤害/自定义），含命中位置；`EventInstigator` 伤害的"意图发起者"Controller（记仇/AI 仇恨用）；`DamageCauser` 直接作用者（哪颗子弹）。返回实际扣掉的血（Super 里可能被 modifier 类改写）。

## 3.5 死亡链路：Die → MulticastHandleDeath（★★★★★ 多人设计典范）

```cpp
void ADuraCharacterBase::Die(const FVector& DeathImpulse)
{
    Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true)); //自动网络复制
    MulticastHandleDeath(DeathImpulse);
}
```

- **两步走的设计**：
  1. **Detach 武器**：武器原本挂在手上，死亡后要变成"掉落的物理道具"。`EDetachmentRule::KeepWorld` 保持世界坐标（人倒下武器留在原地附近），第二参 `bCallModify=true`。**注释"自动网络复制"**：组件 Detach 的结果会由引擎属性复制同步到客户端（AttachmentReplication）。
  2. **Multicast RPC**：死亡表现（音效、物理、溶解、委托广播）要在**所有机器**上播放。

```cpp
UFUNCTION(NetMulticast, Reliable)
virtual void MulticastHandleDeath(const FVector& DeathImpulse);
```
- **`NetMulticast`**：服务器调用，服务器+所有客户端执行。
- **`Reliable`**：可靠传输（丢包重发）。死亡是关键一次性事件，宁可占用带宽必须到达。对比：粒子拖尾这类高频可丢的用 Unreliable。
- **为什么用 RPC 而不是复制属性？** 死亡是**事件**不是**状态**（音效只播一次、物理只施加一次）。复制属性适合"状态"（bDead）；一次性表现用 RPC。

```cpp
void ADuraCharacterBase::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());

    Weapon->SetSimulatePhysics(true);
    Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Weapon->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Weapon->SetEnableGravity(true);
    Weapon->AddImpulse(DeathImpulse * 0.1, NAME_None, true);

    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    GetMesh()->SetEnableGravity(true);
    GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Dissolve();
    bDead = true;
    BurnDebuffComponent->Deactivate();
    StunnedDebuffComponent->Deactivate();
    OnDeathDelegate.Broadcast(this);
}
```

逐块讲：
- **武器/尸体的"布娃娃化"三件套**：`SetSimulatePhysics(true)` 开物理模拟；`PhysicsOnly` 碰撞只参与物理不挡子弹；`SetEnableGravity(true)` 落地。`AddImpulse(DeathImpulse * 0.1, NAME_None, true)`：武器轻（0.1 缩放），`NAME_None` 表示全身（不指定骨骼），第三参 `bVelChange=true` 表示把冲量转成"速度改变量"（对轻小物体更稳定，不受质量影响太大）。
- **Mesh 的区别**：`SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block)`——尸体要**挡地面**（不然穿地）。人体布娃娃需要骨骼体物理（建过的 Physics Asset），这就是为什么 Mesh 能 SimulatePhysics。
- **胶囊关碰撞**：尸体不再是"角色"，玩家/AI 不该再撞到/选中它。注意 `IsDead()` 读的是 bDead，而 **bDead 在 Multicast 里设置**——客户端也同步为 true，一致。
- **Dissolve()**（见下节溶解效果）。
- **关闭 Debuff 特效**：死人不再燃烧/眩晕（组件监听 Tag，但尸体不再处理）。
- **`OnDeathDelegate.Broadcast(this)`**：通知订阅者。谁订阅了？（a）`DuraEnemy` 的蓝图事件做掉落/任务计数；（b）`ArcaneShards` 等技能的清理逻辑；（c）`DuraGameModeBase` 不直接订阅，玩家死亡走 `DuraCharacter::Die` 里的 Timer → PlayerDied。
- **服务器与客户端各广播一次的注意点**：委托是非动态多播，每台机器各自执行（OnDeath 签名是 Dynamic，但绑定的蓝图对象在每端都存在）。C++ 逻辑要写"幂等"——重复广播不崩。

## 3.6 Dissolve —— 溶解消失效果

```cpp
void ADuraCharacterBase::Dissolve()
{
    if (IsValid(DissolveMaterialInstance))
    {
        UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
        GetMesh()->SetMaterial(0, DynamicMatInst);
        StartDissolveTimeline(DynamicMatInst);
    }
    ...
}
```

- `UMaterialInstanceDynamic::Create(父材质, Outer)`：从**材质实例**（`DissolveMaterialInstance`，编辑器配好溶解参数的 MID 模板）动态克隆出一个可运行时改参数的 MID。
- `SetMaterial(0, ...)`：替换 **Element 0**（第一个材质槽）。⚠️ 硬编码 0 意味着多材质槽模型只换第一槽——本项目角色单材质槽，可接受，但通用库要遍历。
- `StartDissolveTimeline(...)` 是 **BlueprintImplementableEvent**：材质参数推进（溶解进度 0→1）在蓝图时间轴里做——**为什么给蓝图？** 因为动画节奏是"表现层"需求，策划/TA 在蓝图里调曲线最方便；C++ 只负责准备好 MID 递过去。
- **溶解原理**：材质里用 `DissolveAmount` 与噪声图比较，小于阈值的像素 discard——"烧掉"效果，不用每帧改顶点。
- **面试拓展**：为什么不直接 `Destroy()`？尸体需要淡出过渡；且 Destroy 时机要等特效播完（DuraEnemy 用 LifeSpan 定时销毁，DuraCharacter 由 GameMode 决定重生）。

## 3.7 GetCombatSocketLocation —— Tag 驱动的发射点查询

```cpp
FVector ADuraCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) const
{
    const FDuraGameplayTags& GameplayTags = FDuraGameplayTags::Get();

    if(MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Weapon))
        return Weapon->GetSocketLocation(WeaponTipSocketName);
    if(MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftHand))
        return GetMesh()->GetSocketLocation(LeftHandTipSocketName);
    ...
    return FVector();
}
```

- **四路分发**：武器尖/左手/右手/尾巴。每种角色在编辑器里配置自己的骨骼插槽名（`WeaponTipSocketName` 等），C++ 只知道"查武器尖"这个抽象请求，具体骨骼名是数据。
- `MatchesTagExact`：**精确匹配**。这里必须 Exact——`CombatSocket.Weapon` 与 `CombatSocket.WeaponX` 这类前缀混淆不容发生（若用 MatchesTag，`CombatSocket.Weapon` 会匹配所有 CombatSocket 开头的查询，语义错乱）。
- 兜底 `return FVector()`：零向量（世界原点）。**隐患提示**：如果 Tag 没配对，火球会从地图原点飞出——这种静默错误用 `ensureMsgf(false, ...)` 报警更好，学习时的批判点。

## 3.8 StunTagChanged —— 眩晕 Tag 的"状态守卫"

```cpp
void ADuraCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    bIsStunned = NewCount > 0;
    GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? 0.0f : BaseWalkSpeed;
}
```

- 这是被 `RegisterGameplayTagEvent(Debuff_Stun, NewOrRemoved)` 注册的回调：**身上 `Debuff.Stun` Tag 数量变化**时触发（GE 的 GrantedTags 会增减 Tag）。`NewCount` 是当前计数——同一 GE 叠两层就是 2，只要 >0 就算眩晕。
- 眩晕的实现 = **移速归零**（`MaxWalkSpeed = 0`）。简单可靠：不能移动但仍可能攻击，所以还需要配合输入屏蔽（DuraCharacter::OnRep_Stunned 里给 ASC 加 `Player_Block_*` Loose Tags，禁止鼠标操作）。
- **注意**：基类只做移速；DuraEnemy 覆写它还通知黑板（AI 停止追击），DuraCharacter 不覆写但 OnRep_Stunned 处理客户端表现——**同一个 Tag 变化，三种消费场景**，这就是 Tag 事件总线的价值。

## 3.9 属性初始化与技能授予（基类提供的公共流程）

```cpp
void ADuraCharacterBase::InitializeDefaultAttributes() const
{
    ApplyAttributeInitEffectToSelf(PrimaryInitEffectClass, 1.0f);
    ApplyAttributeInitEffectToSelf(SecondaryInitEffectClass, 1.0f);
    //必须放在 SecondaryInit 之后
    //因为它依赖于次要属性 MaxHealth 和 MaxMana
    ApplyAttributeInitEffectToSelf(VitalInitEffectClass, 1.0f); 
}
```

- **三个 GE 依次应用**：主属性（力量/智力/韧性/活力初值）→ 次要属性（由主属性派生的护甲/暴击等）→ 生命属性（Health=MaxHealth、Mana=MaxMana）。
- **顺序为什么不能乱**：Vital GE 里 `Health` 的初值公式引用 `MaxHealth`（GE 里用 Attribute-to-Attribute 的 Modifier），MaxHealth 又由 MMC（按 Vigor 算）在 Secondary 阶段得出。**先有派生量，才能引用它**——GAS 的 GE 应用是同步顺序执行的，这正是依赖顺序生效的机制。注释直接点明了这一点。
- ⚠️ 玩家侧覆写了这个函数（走 CharacterClassInfo 数据资产按职业取 GE 集），这里是默认实现（编辑器直接配三个 GE 类）。

```cpp
void ADuraCharacterBase::ApplyAttributeInitEffectToSelf(TSubclassOf<UGameplayEffect> AttributeInitEffectClass, float Level) const
{
    check(AbilitiesSystemComponent);
    check(AttributeInitEffectClass);

    FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponent()->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle GameplaySpecHandle = GetAbilitySystemComponent()
        ->MakeOutgoingSpec(AttributeInitEffectClass, Level, EffectContext);
    GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*GameplaySpecHandle.Data.Get(), AbilitiesSystemComponent);
}
```

这是**全项目应用 GE 的标准四步曲**（后面所有地方都是这个模式的变体，必须背下来）：

1. **`MakeEffectContext()`**：向 ASC 要一个空的 EffectContext 句柄（这里已经是 FDura 版，因为 Globals 替换）。
2. **`EffectContext.AddSourceObject(this)`**：记录"这次 GE 是谁给的"。**为什么必填**：Instigator/Source 用于（a）后续逻辑溯源（谁加的血——拾取物效果要判断给谁）；（b）GAS 内部权限判断（Inhibit、Immunity 检查 Source 的 Tag）。
3. **`MakeOutgoingSpec(GEClass, Level, Context)`**：把 GE 类**实例化成一个 Spec**（规格对象）。三个参数：GE 类、**等级**（GE 内 ByLevel 曲线的查表键）、上下文。返回 `FGameplayEffectSpecHandle`（TSharedPtr 包装）。
4. **`ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC)`**：把 Spec 应用于目标 ASC。`Spec.Data` 是 `TSharedPtr<FGameplayEffectSpec>`，`*Data.Get()` 取裸引用。**参数是目标 ASC 而不是 Actor**——GAS 的应用单位就是 ASC。

```cpp
void ADuraCharacterBase::AddCharacterAbilities()
{
    UDuraAbilitySystemComponent* DuraASC = CastChecked<UDuraAbilitySystemComponent>(AbilitiesSystemComponent);
    if (!HasAuthority()) return;

    DuraASC->AddCharacterAbilities(StartupAbilities);
    DuraASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);  
}
```

- **`if (!HasAuthority()) return;`**：**授予技能只能在服务器**（Skill 的 Spec/Grant 内部状态只权威端正确）。防御在前、逻辑在后——安全检查写函数第一行的习惯。
- `CastChecked`：确定就该是子类，转换失败即崩溃（开发期暴露配置错误）。
- 主动技能与被动技能分两个数组、两个入口：被动技能授予后要**立即 TryActivate**（光环常驻），入口不同见第 6 篇 ASC。
- `StartupAbilities`/`StartupPassiveAbilities` 是 `EditDefaultsOnly` 数组——玩家蓝图里配 GA_FireBolt 等，**"这个角色天生会什么"是角色资产的数据**。

---

# 四、DuraPlayerState —— 玩家数据中枢 ★★★★★

## 4.1 构造函数：混合复制模式

```cpp
ADuraPlayerState::ADuraPlayerState()
{
    AbilitiesSystemComponent = CreateDefaultSubobject<UDuraAbilitySystemComponent>("AbilitiesSystemComponent");
    AbilitiesSystemComponent->SetIsReplicated(true);
    AbilitiesSystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UDuraAttributeSet>("AttributeSet");

    SetNetUpdateFrequency(100.0f);
}
```

- **ASC 复制三模式（★★★★★ 面试必考）**：

| 模式 | GE 复制方式 | 适用 | 开销 |
|---|---|---|---|
| `Full` | 每个 GE 全量复制到所有端 | 单人 | 最高 |
| `Mixed` | GE 只复制给**拥有者客户端**；Tag/Cue 复制给所有人 | **玩家**（本项目 PlayerState 用它） | 中 |
| `Minimal` | GE 不复制，只复制 Tag；Cue 走最小路径 | **AI 敌人**（本项目 Enemy 用它） | 最低 |

- 为什么玩家用 Mixed？自己必须知道身上的完整 GE（冷却/Debuff 剩余时间要显示 UI）；其他玩家只需要知道 Tag（比如"他眩晕了"）。
- **关键坑（课程原话级考点）**：Mixed 模式要求 **ASC 的 OwnerActor 的 NetUpdateFrequency 尽量高**——PlayerState 默认 0.1（100ms 才同步一次），冷却条会明显跳动，所以 `SetNetUpdateFrequency(100.0f)` 提到 100Hz。这是 GAS 玩家手感优化的经典操作。
- AttributeSet 直接 CreateDefaultSubobject 在 PlayerState 里：属性集合与 ASC 同宿主，生命周期一致。

## 4.2 经验/等级/点数系统：Add 与 Set 的双子星

```cpp
void ADuraPlayerState::AddToXP(int32 InXP)
{
    XP += InXP;
    OnXPChangedDelegate.Broadcast(XP);
}

void ADuraPlayerState::SetXP(int32 InXP)
{
    XP = InXP;
    OnXPChangedDelegate.Broadcast(XP);
}
```

- `Add` 用于运行时增量（击杀敌人）；`Set` 用于**读档恢复**（直接设定值）。两者都广播委托——**UI 不需要关心是哪种变化**。
- 网络细节：XP/Level 是 `ReplicatedUsing` 的 int32，服务器修改 → 自动复制 → 客户端 `OnRep_XP` 也广播同一个委托。**于是"服务器广播"与"客户端 OnRep 广播"形成双轨，两端 UI 都能刷新**。这与 DuraCharacter 的 InitAbilityActorInfo 双轨（下一节）是同一设计哲学。
- `OnLevelChanged(int32, bool)` 的第二参 `bLevelUp`：`AddToLevel` 传 true（真升级，要播特效/奖励），`SetLevel` 传 false（读档恢复，不播）——**同一位，不同语义**。

## 4.3 委托清单

```cpp
FOnPlayerStatChanged OnXPChangedDelegate;          // (int32 新值)
FOnLevelChanged OnLevelChangedDelegate;            // (int32 新等级, bool 是否升级)
FOnPlayerStatChanged OnAttributePointsChangedDelegate;
FOnPlayerStatChanged OnSpellPointsChangedDelegate;
```

订阅者：`DuraOverlayWidgetController`（XP 条）、`AttributeMenuWidgetController`（点数显示）、`SpellMenuWidgetController`（技能点显示）、`MVVM_LoadSlot`（存档快照）等（第 8/9 篇）。

---

# 五、DuraCharacter —— 玩家角色：双轨初始化的教科书 ★★★★★

## 5.1 构造函数：俯视角游戏的运动学配置

```cpp
ADuraCharacter::ADuraCharacter()
{
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->SetUsingAbsoluteRotation(true);   // 相机臂不随角色旋转
    CameraBoom->bDoCollisionTest = false;          // 相机不做遮挡检测

    TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>("TopDownCameraComponent");
    TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    TopDownCameraComponent->bUsePawnControlRotation = false;

    LevelUpNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("LevelUpNiagaraComponent");
    LevelUpNiagaraComponent->SetupAttachment(GetRootComponent());
    LevelUpNiagaraComponent->bAutoActivate = false;  // 升级特效手动触发

    GetCharacterMovement()->bConstrainToPlane = true;     // 锁定运动平面
    GetCharacterMovement()->bSnapToPlaneAtStart = true;   // 开局吸附到平面

    GetCharacterMovement()->bOrientRotationToMovement = true;   // 面朝移动方向
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);  // 每秒最多转 500 度

    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    bUseControllerRotationYaw = true;

    CharacterClass = ECharacterClass::Elementalist;  // 玩家默认职业：元素使
}
```

- **`SetUsingAbsoluteRotation(true)`**：SpringArm 不跟随角色 Yaw。俯视角固定机位的必要设置——否则角色一转身相机跟着甩。
- **`bDoCollisionTest=false`**：关闭"相机被墙卡住"的探测（探测每帧开销 + 俯视角通常不需要）。
- **平面约束三件套**（`bConstrainToPlane`/`bSnapToPlaneAtStart` + 默认 XY 平面）：强制角色只在水平面移动，杜绝浮空/入地误差——俯视角 RPG 标配。
- **转向策略选择（重要知识点）**：`bOrientRotationToMovement=true`（角色面朝**移动方向**，平滑旋转，上限 500°/s）+ `bUseControllerRotationYaw=true`（同时角色 Yaw 跟随控制器）。两者组合的语义：移动时自然转向移动方向；同时 Controller 的 Yaw（本游戏 Controller Yaw 基本不变）兜底。**如果只留 Controller 跟随**，角色会瞬移式转向；**如果只留 OrientToMovement**，攻击时想强制面向目标就要靠蒙太奇或 SetActorRotation。这是"角色朝向策略"的经典三选一（第三种：AoT—— RotationRate 更高完全跟随移动）。

## 5.2 InitAbilityActorInfo 双轨初始化（本篇最核心函数）

### 5.2.1 为什么需要"双轨"（★★★★★ 面试必考）

玩家 ASC 在 PlayerState 上。**不同机器上，"拿到 PlayerState"的时机不同**：

- **服务器**：Pawn 生成 → `PossessedBy(Controller)` 被调用 → 此时 `GetPlayerState()` 已可用 → 在这里初始化。
- **客户端**：Pawn 是网络复制来的，`PossessedBy` **不会**在客户端对玩家 Pawn 调用（Possession 是服务器行为）；但 PlayerState 复制到位时会触发 Pawn 的 `OnRep_PlayerState()` → 在这里初始化。

**两条轨道都要通向同一个函数 `InitAbilityActorInfo()`**，谁先到位谁先跑。

### 5.2.2 服务器轨道

```cpp
void ADuraCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    //服务器端, 初始化ASC
    InitAbilityActorInfo();
    LoadProgress();

    ADuraGameModeBase* DuraGameMode = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this));  
    if(DuraGameMode)
    {
        DuraGameMode->LoadWorldState(GetWorld());
    }
}
```

- 初始化 ASC → 恢复玩家进度（属性+技能）→ 恢复**世界状态**（哪些箱子被开了、哪些检查点激活了）。世界状态恢复放在这里因为只有服务器有存档职责；LoadWorldState 内部有 DoesSaveGameExist 守卫，首次游戏无事发生。
- **注意 InitAbilityActorInfo 在 LoadProgress 之前**：AddCharacterAbilities 需要 ASC 已绑定 ActorInfo，顺序不能颠倒。

### 5.2.3 客户端轨道

```cpp
void ADuraCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if(!HasAuthority())
    {
        //客户端, 初始化ASC
        InitAbilityActorInfo();
    }
}
```

- `if(!HasAuthority())`：保险丝。万一服务器也触发了 OnRep（理论上 RepNotify 在服务器本地修改也会触发），不重复初始化。
- **重复调用防护**（真实世界 bug 点）：OnRep_PlayerState 可能触发多次（Pawn 复制顺序、重新 Possess）。本项目在 InitAbilityActorInfo 内做了 RemoveAll 再注册（见下），并在 LoadProgress/GameMode 侧靠幂等设计兜底。

### 5.2.4 汇合点：InitAbilityActorInfo 全解

```cpp
void ADuraCharacter::InitAbilityActorInfo()
{
    ADuraPlayerState* playerState = GetPlayerState<ADuraPlayerState>();
    check(playerState);

    AbilitiesSystemComponent = playerState->GetAbilitySystemComponent();
    AttributeSet = playerState->GetAttributeSet();
    check(AbilitiesSystemComponent);

    // OnRep_PlayerState 可能多次触发（重新 Possess 等），先解绑再注册避免回调堆积
    AbilitiesSystemComponent->RegisterGameplayTagEvent(FDuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
    AbilitiesSystemComponent->RegisterGameplayTagEvent(FDuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &ADuraCharacter::StunTagChanged);

    OnASCRegistered.Broadcast(AbilitiesSystemComponent);

    AbilitiesSystemComponent->InitAbilityActorInfo(playerState, this);

    CastChecked<UDuraAbilitySystemComponent>(AbilitiesSystemComponent)->AbilityActorInfoSet();

    if (ADuraPlayerController* PlayerController = Cast<ADuraPlayerController>(GetController()))
    {
        if (ADuraHUD* HUD = Cast<ADuraHUD>(PlayerController->GetHUD()))
        {
            HUD->InitOverlay(PlayerController, playerState, AbilitiesSystemComponent, AttributeSet);
        }
    }
}
```

逐行精讲：

- **`GetPlayerState<ADuraPlayerState>()`**：模板版 Getter，内部 Cast。`check` 保证一定有 PlayerState（玩家 Pawn 必有）。
- **借用而不是拥有**：`AbilitiesSystemComponent = playerState->Get...` —— 基类的成员指针指向 PlayerState 里的组件。**Pawn 只是"引用"ASC，所有权在 PlayerState**。此后基类所有 `GetAbilitySystemComponent()` 都通。
- **Tag 事件的 RemoveAll + Add 模式**：`RegisterGameplayTagEvent` 返回委托引用，`.RemoveAll(this)` 移除**本对象**的全部旧绑定再重新加——防重复订阅的经典写法（对比 `AddUnique`）。
- **`OnASCRegistered.Broadcast(...)`**：对外广播"我的 ASC 就绪了"。订阅者：`DuraOverlayWidgetController` 等要等 ASC 才能注册属性回调；`MagicCircle` 等系统。**事件顺序**：先广播"ASC 注册"，再真正 `InitAbilityActorInfo`——订阅者拿到的是已就位但未激活 ActorInfo 的 ASC，二者职责不同。
- **`ASC->InitAbilityActorInfo(OwnerActor, AvatarActor)`**（GAS 核心 API）：
  - 参数1 `OwnerActor = playerState`：**ASC 的拥有者**（组件挂谁身上）。
  - 参数2 `AvatarActor = this`：**化身**（能力作用在谁身上、动画蒙太奇播谁、骨骼插座取谁）。
  - 内部构建 `FGameplayAbilityActorInfo`：缓存 Owner/Avatar/MovementComponent/SkeletalMesh 等，供技能运行期快速访问。
  - **玩家这里 Owner=PlayerState、Avatar=Character**；敌人后面是 Owner=Avatar=自身。**为什么 ActorInfo 里要区分两者**：属性数据的宿主（Owner）与表现/移动的宿主（Avatar）分离，正是"ASC 在 PlayerState"模式的根基。
- **`AbilityActorInfoSet()`**（DuraAbilitySystemComponent 的自定义函数，第 6 篇详讲）：注册"玩家死亡/眩晕时给输入加 BlockTag"等 ASC 级别监听。
- **`HUD->InitOverlay(...)`**：把 UI 初始化所需的四大件（PC/PS/ASC/AS）一次性传给 HUD，HUD 再创建 Overlay 控件并构造 WidgetController（第 8 篇）。**UI 初始化搭在 ASC 初始化的顺风车上**——因为 UI 依赖 ASC，ASC 就绪即 UI 就绪。
- 最后被注释掉的 `//InitializeDefaultAttributes();`：属性初始化挪去了 `LoadProgress()`（要么首次应用初始化 GE，要么从存档恢复）——**保留注释是教学残留**，实际由 LoadProgress 决策。

### 5.2.5 双轨时序图（背下来）

```
服务器                              客户端
──────                              ──────
Spawn Pawn                          收到复制的 Pawn（尚无 PlayerState 引用?）
  │                                   │
PossessedBy(PC)                      PlayerState 复制到位
  │                                   │
InitAbilityActorInfo() ◄───────► OnRep_PlayerState() → InitAbilityActorInfo()
  │                                   │
LoadProgress()                     （无存档职责，靠复制同步数据）
  │
LoadWorldState()
```

- **面试考点**：*"客户端玩家 Pawn 为什么不走 PossessedBy？"* —— Possession 是服务器概念；客户端靠 PlayerState 的 RepNotify 补偿初始化。
- **进阶追问**：*"如果客户端 OnRep_PlayerState 早于 ASC 复制到位怎么办？"* —— ASC 是 PlayerState 的 subobject 随 PlayerState 一起复制，天然原子；但 **ASC 内部 granted abilities 的复制**依赖 Mixed 模式的通道，这是 AbilityActorInfoSet 与 MinLevel 检查存在的意义。

## 5.3 LoadProgress —— 首次游戏 vs 读档二选一

```cpp
void ADuraCharacter::LoadProgress()
{
    ADuraGameModeBase* DuraGameMode = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this));  
    if(DuraGameMode)
    {
        ULoadScreenSaveGame* SaveData = DuraGameMode->RetrieveInGameSaveData();
        if(!SaveData) return;

        if(SaveData->bFirstTimeLoadIn)
        {
            InitializeDefaultAttributes();
            AddCharacterAbilities();
        }
        else
        {
            if(UDuraAbilitySystemComponent* DuraASC = Cast<UDuraAbilitySystemComponent>(AbilitiesSystemComponent))
            {
                DuraASC->AddCharacterAbilitiesFromSaveData(SaveData);
            }
            if(ADuraPlayerState* DuraPlayerState = Cast<ADuraPlayerState>(GetPlayerState()))
            {
                DuraPlayerState->SetLevel(SaveData->PlayerLevel);
                DuraPlayerState->SetXP(SaveData->XP);
                DuraPlayerState->SetAttributePoints(SaveData->AttributePoints);
                DuraPlayerState->SetSpellPoints(SaveData->SpellPoints);
            }
            UDuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(this, AbilitiesSystemComponent, SaveData);
        }
    }
}
```

- **`RetrieveInGameSaveData()`**（GameMode 提供，见第 7 节）：按 GameInstance 记录的槽位名取出存档，**不存在就新建**（首次游玩）。`bFirstTimeLoadIn` 是" virgin 槽"标志。
- **首次分支**：标准初始化（GE 三连 + 天生技能）。
- **读档分支**：
  - `AddCharacterAbilitiesFromSaveData(SaveData)`：按存档的 FSavedAbility 列表逐个授予技能并恢复等级/槽位/状态（第 6 篇 ASC 详讲）。
  - PlayerState 四件 Set：等级/XP/点数直接恢复（Set 系列广播委托，UI 一次性刷到位）。
  - `InitializeDefaultAttributesFromSaveData`（Library 静态函数）：**按存档里的四维主属性生成初始化 GE 参数**再应用——保证 MaxHealth/MaxMana 由主属性重算出来，而不是存一份血量（存血量的话升级后加成会错）。
- **为什么属性不直接存 MaxHealth**：属性是派生网络（主属性 → MMC → 次要 → 生命），存根（主属性）就能重建整棵树——**存增量不存派生**，与数据库范式化同理。

## 5.4 SaveProgress —— 把玩家状态写进存档

```cpp
void ADuraCharacter::SaveProgress_Implementation(const FName& CheckPointTag)
{
    if(!HasAuthority()) return;   // 存档数据只存在于服务器，客户端调用直接忽略
    ...
}
```

- `SaveProgress` 是 **IPlayerInterface** 的函数（玩家接口，第 11 篇）：CheckPoint（检查点）被激活时调用 `IPlayerInterface::Execute_SaveProgress(OverlappingActor, CheckPointTag)`。
- 属性快照：`UDuraAttributeSet::GetStrengthAttribute().GetNumericValue(GetAttributeSet())`——**属性系统的静态 Getter + GetNumericValue 组合拳**：`GetStrengthAttribute()` 返回 `FGameplayAttribute`（属性的反射描述符），`GetNumericValue(AttributeSet)` 取当前数值。这个组合不依赖字符串（类型安全），是 GAS 读属性的标准姿势。
- 技能快照（**ForEachAbility 遍历模式**）：

```cpp
FForEachAbility SaveAbilityDelegate;
SaveAbilityDelegate.BindLambda([this, DuraASC, &SaveData](const FGameplayAbilitySpec& AbilitySpec)
    {
        FGameplayTag AbilityTag = DuraASC->GetAbilityTagFromSpec(AbilitySpec);
        FDuraAbilityInfo AbilityInfo = UDuraAbilitySystemLibrary::GetAbilityInfo(this)->FindAbilityInfoForTag(AbilityTag);
        
        FSavedAbility SaveAbility;
        SaveAbility.GameplayAbilityClass = AbilityInfo.Ability;   // ← 注意：从 Info 取类，不从 Spec 取
        SaveAbility.AbilityLevel = AbilitySpec.Level;
        SaveAbility.AbilitySlot = DuraASC->GetSlotFromAbilityTag(AbilityTag);
        SaveAbility.AbilityStatus = DuraASC->GetStatusFromAbilityTag(AbilityTag);
        SaveAbility.AbilityTag = AbilityTag;
        SaveAbility.AbilityType = AbilityInfo.AbilityType;
        SaveData->SavedAbilities.AddUnique(SaveAbility);
    });
DuraASC->ForEachAbility(SaveAbilityDelegate);
```

- `FForEachAbility` 是 ASC 定义的非动态委托类型；`ForEachAbility` 内部遍历 `GetActivatableAbilities()`（已授予技能的 Spec 数组）逐个 Broadcast。
- **Lambda 捕获 `[this, DuraASC, &SaveData]`**：`&SaveData` 按引用捕获（同栈帧，安全），this/DuraASC 按值（防悬垂）——C++ 捕获语义的教学样本。
- **为什么 `GameplayAbilityClass` 从 AbilityInfo 查而不是直接从 Spec 拿**：Spec 里存的是 GA 类没问题，但**技能状态（Locked/UnLocked）的 Spec 可能是占位技能**；更重要的是存档里存"Tag→类"要与数据资产对齐，防止蓝图改类后旧存档的类指针失效——**存 Tag 与配置，运行时重建**。
- `SaveData->SavedAbilities.AddUnique(...)`：FSavedAbility 有 `==`（按 Tag 比较），重复保存去重。
- 最后 `SaveInGameProgressData(SaveData)` 落盘。

## 5.5 Die 与升级特效

```cpp
void ADuraCharacter::Die(const FVector& DeathImpulse)
{
    Super::Die(DeathImpulse);   // 基类：武器掉落 + MulticastHandleDeath（物理/溶解/委托）

    FTimerDelegate DeathTimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if(ADuraGameModeBase* DuraGM = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
        {
            DuraGM->PlayerDied(this);
        }
    });
    GetWorldTimerManager().SetTimer(DeathTimer, DeathTimerDelegate, DeathTime, false);
    TopDownCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}
```

- **玩家死亡 ≠ 销毁**：敌人 5 秒后 SetLifeSpan 销毁；玩家要**重生**。所以 `DeathTime`（默认 5 秒）后调 `GameMode->PlayerDied(this)` → GameMode `OpenLevel` 回检查点（重开地图走完整诞生链路）。
- **`CreateWeakLambda(this, ...)`**：弱对象 Lambda——Timer 期间若 Pawn 被销毁（极端情况），弱引用失效不触发，**杜绝悬垂 this 崩溃**。与裸 `[this]` 的区别是生命周期安全，与 `AddUObject` 等价但适合 Lambda。**Timer 回调安全三选一**：WeakLambda / AddUObject / AddWeakLambda——面试常问"定时器回调里对象死了怎么办"。
- **`TopDownCameraComponent->DetachFromComponent(KeepWorldTransform)`**：死亡瞬间相机从角色上**脱离**，定住看尸体——电影感设计。相机臂本来挂角色根上，角色溶解后相机没跟着掉（KeepWorld 保持当前位置）。

```cpp
void ADuraCharacter::LevelUp_Implementation()
{
    MulticastLevelUpParticles();
}

void ADuraCharacter::MulticastLevelUpParticles_Implementation() const
{
    const FVector CameraLocation = TopDownCameraComponent->GetComponentLocation();
    const FVector NiagaraSystemLocation = LevelUpNiagaraComponent->GetComponentLocation();
    const FRotator ToCameraRotation = (CameraLocation - NiagaraSystemLocation).Rotation();

    LevelUpNiagaraComponent->SetWorldRotation(ToCameraRotation);
    LevelUpNiagaraComponent->Activate(true);
}
```

- 升级特效（光环升起）让 Niagara **面朝相机**：`目标方向 = 相机位置 - 特效位置` 再 `.Rotation()`。俯视角下光环 billboard 化，玩家永远看得到。
- `NetMulticast`：全端播放。调用方是 `LevelUp_Implementation`——它由 `DuraPlayerState` 升级链（OnLevelChanged）经 PlayerInterface 触发（服务器确定升级 → 全端特效）。

## 5.6 DuraCharacter 里的一堆"翻译函数"（PlayerInterface 实现）

`AddToXP/GetXP/FindLevelForXP/GetAttributePointsReward/GetSpellPointsReward/AddToPlayerLevel/AddToAttributePoints/AddToSpellPoints/GetAttributePoints/GetSpellPoints/ShowMagicCircle/HideMagicCircle/SaveProgress` —— 全是"**接到调用 → CastChecked 到 PlayerState → 转发**"模式。

- **为什么 Pawn 不直接被调用，要隔一层接口？** 因为调用方（ExecCalc 给 XP、CheckPoint 触发保存、ArcaneShards 消耗点数）手里只有 Actor（CombatInterface 视角）。接口让"奖励发放"对**玩家与敌人**统一：敌人不给 XP，玩家给——调用方只管"发奖"，玩家类负责翻译成 PlayerState 操作。
- `CastChecked` vs `GetPlayerState<ADuraPlayerState>()`：本项目混用，语义都是"我确定是玩家上下文"。
- **ShowMagicCircle**：`PlayerController->ShowMagicCircle(DecalMaterial)` + 藏鼠标。奥术碎片技能施法时地面出现魔法阵（贴花）替代光标——**玩家角色只是中转，真正实现在 PlayerController**（第 3 篇）。

---

# 六、DuraEnemy —— 敌人：自持 ASC 的另一条路 ★★★★☆

## 6.1 构造函数

```cpp
ADuraEnemy::ADuraEnemy()
{
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    ...
    AbilitiesSystemComponent = CreateDefaultSubobject<UDuraAbilitySystemComponent>("AbilitiesSystemComponent");
    AbilitiesSystemComponent->SetIsReplicated(true);
    AbilitiesSystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UDuraAttributeSet>("AttributeSet");

    HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
    HealthBar->SetupAttachment(GetRootComponent());

    BaseWalkSpeed = 250.f;

    GetMesh()->SetCustomDepthStencilValue(HIGHLIGHT_COLOR_RED);
    GetMesh()->MarkRenderStateDirty();  
    Weapon->SetCustomDepthStencilValue(HIGHLIGHT_COLOR_RED);
    Weapon->MarkRenderStateDirty();
}
```

- **`ECC_Visibility → Block`**：Visibility 通道是鼠标点击检测（LineTrace）用的通道；敌人 Mesh 挡住它意味着"点得到"。玩家 Mesh 不设 Block（玩家不走点击选中）。对比学习：**通道按"谁需要被谁检测"配置**。
- **`Minimal` 复制模式**（前文表格）：敌人数量多，GE 全量复制太贵；客户端只需要 Tag（眩晕/灼烧）+ 血条数值（血量走属性复制，非 GE）。
- **`SetCustomDepthStencilValue(HIGHLIGHT_COLOR_RED)`**：构造期就给 Mesh/Weapon 盖"红色描边"戳（250 号）。`MarkRenderStateDirty()` 强制刷新渲染状态——改渲染属性后必须调用，否则不生效（**渲染组件的属性修改不会自动 dirty**，经典坑）。
- 默认描边值红色，玩家是蓝色（在 BP_DuraCharacter 或 DuraCharacter 里可对照，玩家实现不走 HighlightInterface 而是走固定蓝）。

## 6.2 BeginPlay：敌人的一站式初始化

```cpp
void ADuraEnemy::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
    InitAbilityActorInfo();

    if (HasAuthority())
    {
        UDuraAbilitySystemLibrary::GiveStartupAbilities(this, AbilitiesSystemComponent, CharacterClass);
    }

    if (UDuraUserWidget* DuraUserWidget = Cast<UDuraUserWidget>(HealthBar->GetUserWidgetObject()))
    {
        DuraUserWidget->SetWidgetController(this);
    }

    UDuraAttributeSet* DuraAS = Cast<UDuraAttributeSet>(AttributeSet);
    if (DuraAS)
    {
        AbilitiesSystemComponent->GetGameplayAttributeValueChangeDelegate(DuraAS->GetHealthAttribute()).AddLambda(
            [this](const FOnAttributeChangeData& Data) { OnHealthChanged.Broadcast(Data.NewValue); });
        ...MaxHealth 同理...

        AbilitiesSystemComponent->RegisterGameplayTagEvent(FDuraGameplayTags::Get().Effect_HitReact, 
            EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADuraEnemy::HitReactTagChanged);

        OnHealthChanged.Broadcast(DuraAS->GetHealth());
        OnMaxHealthChanged.Broadcast(DuraAS->GetMaxHealth());
    }
}
```

- **`GiveStartupAbilities(this, ASC, CharacterClass)`**（Library 静态函数）：按职业（Warrior/Ranger/Elementalist）从 `DA_CharacterClassInfo` 的 `StartupAbilities` 里批量授予——**敌人技能按职业配置**，与玩家蓝图硬配不同（数据驱动差异，第 7 篇）。
- **血条绑定**：`HealthBar->GetUserWidgetObject()` 拿 WidgetComponent 里嵌的 UMG 实例，`SetWidgetController(this)`——**敌人自己就是 WidgetController**（实现血条所需委托：OnHealthChanged 等，见第 8 篇 MVC）。
- **属性变化委托**：`GetGameplayAttributeValueChangeDelegate(Attribute)` 是 GAS 属性监听的官方 API，返回可加回调的委托。Lambda 把引擎事件翻译成自己的 `BlueprintAssignable` 委托 OnHealthChanged——**引擎事件 → 项目蓝图事件的适配器模式**。
- **手动首播两次 Broadcast**：注册委托后立刻广播当前值——**UI 初始化时刷新一次初始血量**（否则血条初始为 0 直到第一次掉血）。这是"注册后立即同步快照"的通用模式（HUD 初始化同理）。
- `OnRep_Stunned/OnRep_Burned` 在敌人端没有覆写（用基类空实现）——敌人端眩晕表现靠 StunTagChanged + Debuff 组件即可，不需要输入屏蔽（敌人没有鼠标输入）。

## 6.3 InitAbilityActorInfo（敌人版）与 PossessedBy

```cpp
void ADuraEnemy::InitAbilityActorInfo()
{
    check(AbilitiesSystemComponent);
    AbilitiesSystemComponent->InitAbilityActorInfo(this, this);   // Owner = Avatar = 自己
    Cast<UDuraAbilitySystemComponent>(AbilitiesSystemComponent)->AbilityActorInfoSet();
    AbilitiesSystemComponent->RegisterGameplayTagEvent(FDuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &ADuraEnemy::StunTagChanged);
    if (HasAuthority())
    {
        InitializeDefaultAttributes();
    }
    OnASCRegistered.Broadcast(AbilitySystemComponent);
}
```

- **`InitAbilityActorInfo(this, this)`**：敌人 Owner 与 Avatar 都是自身——与玩家的（PlayerState, Character）形成鲜明对照，**这就是两种 ASC 宿主方案的代码分叉点**。
- 属性初始化包在 `HasAuthority()` 里：属性值只需服务器权威，客户端收属性复制（AttributeSet 的 RepNotify，第 4 篇）。
- **谁调用了这个函数？** BeginPlay 里直接调（敌人出生即初始化，无需等 Possess）。PossessedBy 是第二条保险（AI Controller 附身时再跑一遍 AI 启动）：

```cpp
void ADuraEnemy::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (!HasAuthority()) return;

    DuraAIController = Cast<ADuraAIController>(NewController);
    if(DuraAIController && BehaviorTree && DuraAIController->GetBlackboardComponent())
    {
        DuraAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
        DuraAIController->RunBehaviorTree(BehaviorTree);
        DuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
        DuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);
    }
}
```

- **AI 启动三步**：`InitializeBlackboard(黑板资产)` 初始化黑板键 → `RunBehaviorTree(行为树)` 开始决策 → 写入初始黑板值。
- **黑板键 RangedAttacker**：`CharacterClass != Warrior` 即远程怪——**用职业决定 AI 行为分支**（近战贴脸 vs 远程拉扯），数据驱动 AI 的最小实现。
- `!HasAuthority()` 先行：AI 只在服务器跑，客户端的 PossessedBy（对 AI Pawn 客户端也会触发？不会，AI Pawn 不复制 Possession 给非拥有客户端——此判断是防御式冗余）。
- **BeginPlay 与 PossessedBy 的初始化分工**（★★★★☆ 常混淆点）：GAS/属性/血条在 BeginPlay（与控制者无关）；AI 行为树在 PossessedBy（依赖 Controller）。**SpawnVolume 生成的敌人**是先 Spawn 再 SpawnDefaultController，两个时机都成立。

## 6.4 Die（敌人版）

```cpp
void ADuraEnemy::Die(const FVector& DeathImpulse)
{
    SetLifeSpan(LifeSpan);          // 5 秒后销毁（含尸体）
    if(DuraAIController && DuraAIController->GetBlackboardComponent())
    {
        DuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);  // 行为树停手
    }
    SpawnLoot();                    // BlueprintImplementableEvent：蓝图按 LootTiers 掉宝
    Super::Die(DeathImpulse);       // 基类死亡表现
}
```

- **死亡前三件事的顺序语义**：先定时销毁（尸体只留 5 秒）→ 通知 AI 停止（黑板 Dead=true，行为树切到无操作分支）→ 掉落战利品 → 基类广播表现。
- `SpawnLoot()` 是 BlueprintImplementableEvent：掉落物生成在蓝图（BP_DuraEnemy）里做，读 `DA_LootTiers`——**掉率数据在数据资产，生成逻辑在蓝图，生命周期在 C++**。

## 6.5 HitReactTagChanged

```cpp
void ADuraEnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    bHitReacting = NewCount > 0;
    GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.0f : BaseWalkSpeed;
    if (DuraAIController && DuraAIController->GetBlackboardComponent())
    {
        DuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
    }
}
```

- 与眩晕几乎同构（移速归零），**多一步：写黑板**。受击时 AI 从"追击/攻击"切到"硬直"分支——**同一个 Tag 事件，人类玩家消费表现为输入屏蔽，AI 消费表现为黑板布尔**。
- `Effect_HitReact` Tag 由命中时应用的 HitReact GE（带 `Abilities.HitReact` GrantedTag）授予，ExecCalc 里 Apply。

---

# 七、DuraGameModeBase —— 大管家 ★★★★☆

## 7.1 它管什么

- **三份全局数据资产**：`CharacterClassInfo`（职业→初始化 GE+技能表）、`AbilityInfo`（技能→描述/等级曲线）、`LootTiers`（掉落表）。**为什么放 GameMode**：全局唯一、服务器职责、蓝图子类 BP_DuraGameMode 里配好实例。任何系统 `Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this))` 就能拿。
- **存档系统**：槽位 CRUD、局内存档（Retrieve/SaveInGameProgress）、**世界状态存档**（SaveWorldState/LoadWorldState）、跨地图转移（TravelToMap）。
- **出生点选择**：ChoosePlayerStart 覆写。
- **死亡重生**：PlayerDied。

## 7.2 存档四函数精讲

### GetOrCreateSaveSlotData

```cpp
ULoadScreenSaveGame* ADuraGameModeBase::GetOrCreateSaveSlotData(const FString& SlotName, int32 SlotIndex) const
{
    USaveGame* SaveGameObject = nullptr;
    if(UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
        SaveGameObject = UGameplayStatics::LoadGameFromSlot(SlotName, SlotIndex);
    else
        SaveGameObject = UGameplayStatics::CreateSaveGameObject(LoadScreenSaveGameClass);
    return Cast<ULoadScreenSaveGame>(SaveGameObject);
}
```

- UE 存档 API 三件套：`DoesSaveGameExist` 检查、`LoadGameFromSlot` 读（反序列化出 USaveGame 对象）、`SaveGameToSlot` 写、`CreateSaveGameObject` 按类造空对象。
- **"不存在就新建"语义**：首次游玩没有存档，Retrieve 也要返回一个可写的空对象——调用方（LoadProgress）拿 bFirstTimeLoadIn 标志分流。
- `SlotName + SlotIndex` 二元组定位一个存档（同名不同 index 是不同槽）——注意 `LoadSlot->GetLoadSlotName()`（玩家起的名）与 SlotIndex 的组合。

### SaveSlotData（主菜单建/覆盖槽位）

把 **MVVM_LoadSlot**（UI 侧 ViewModel，第 9 篇）的数据搬到 USaveGame 对象落盘。先 `DeleteSlot` 再写——**覆盖写 = 删+建**，因为 SaveGameToSlot 对同名槽是覆盖，显式删除保证状态干净（也清理被删槽的中间状态）。

### SaveWorldState / LoadWorldState（★★★★★ 通用 Actor 序列化范式）

```cpp
void ADuraGameModeBase::SaveWorldState(UWorld* World, const FString& DestinationMapAssetName)
{
    FString WorldName = World->GetMapName();
    WorldName.RemoveFromStart(World->StreamingLevelsPrefix);   // 去掉流送关卡前缀 "UEDPIE..." 等
    ...
    for (FActorIterator It(World); It; ++It)
    {
        AActor* Actor = *It;
        if(!IsValid(Actor) || !Actor->Implements<USaveInterface>()) continue;

        FSaveActor SavedActor;
        SavedActor.ActorName = Actor->GetFName();
        SavedActor.Transform = Actor->GetTransform();

        FMemoryWriter MemoryWriter(SavedActor.Bytes);
        FObjectAndNameAsStringProxyArchive Archive(MemoryWriter, true);
        Archive.ArIsSaveGame = true;
        Actor->Serialize(Archive);

        SavedMap.SavedActors.AddUnique(SavedActor);
    }
    ...
    UGameplayStatics::SaveGameToSlot(SaveData, ...);
}
```

逐行拆解（这是 **UE 通用存档系统的标准模板，面试常考**）：

- **`WorldName.RemoveFromStart(World->StreamingLevelsPrefix)`**：PIE 下地图名带 `UEDPIE_0_` 前缀、流送子关卡带前缀——**先清洗名字再当 Key**，否则 PIE 存的档在 Standalone 读不出来。
- **`Actor->Implements<USaveInterface>()`**：只序列化"自愿参与存档"的 Actor（宝箱/检查点/药草）。**白名单模式**，避免全场景爆炸。
- **`FMemoryWriter MemoryWriter(SavedActor.Bytes)`**：把序列化输出写进**内存字节数组**（TSortedMap→TArray<uint8>）。
- **`FObjectAndNameAsStringProxyArchive Archive(MemoryWriter, true)`**：代理档案。第二参 `bInLoadIfFindFails=false`→这里传 true？注意参数是 `bInLoadIfFindFails`——为 true 时加载找不到对象就置空而不是崩溃。此代理的作用：**把 UObject 引用序列化为"对象路径名字符串"**（USaveGame 二进制里不能存裸指针）。
- **`Archive.ArIsSaveGame = true`**：**只序列化标记了 `SaveGame` specifier 的 UPROPERTY**！`UPROPERTY(SaveGame) int32 bIsActivated;` 的变量才进字节流。这是**属性级选择性序列化**——同一个 Actor 一百个属性，只存三五个状态位。
- **`Actor->Serialize(Archive)`**：手动触发 Actor 的序列化管线（走反射按 ArIsSaveGame 过滤）。**同一份代码，读档时换 FMemoryReader 就反向恢复**——对称美。
- **LoadWorldState 的对称操作**：遍历 Actor → 按名字匹配 SavedActor → `ShouldLoadTransform` 接口询问是否恢复位置 → `SetActorTransform` → 换 `FMemoryReader` 再 Serialize（字节流反灌回属性）→ `Execute_LoadActor(Actor)` 通知 Actor"你被恢复了"（做后续逻辑，如宝箱保持打开状态）。
- **FSaveMap/FSaveActor 结构**（在 LoadScreenSaveGame.h，第 9 篇）：存档按 **地图→Actor→字节** 三级组织，`GetSavedMapWithMapName(WorldName)` 取当前地图条目，`SaveData->HasMap` 判新图。
- **`AddUnique` + 替换循环**：`SavedActors.AddUnique(SavedActor)`（FSaveActor 按 ActorName 定义 ==）+ 遍历 `SavedMaps` 用新数据**整体替换**同名字图条目——先清空再填充再替换，保证幂等（多次保存不叠加垃圾）。

### TravelToMap / GetMapNameFromMapAssetName / PlayerDied / ChoosePlayerStart

- **`TravelToMap`**：`Maps` 是 `TMap<FString 显示名, TSoftObjectPtr<UWorld>>`；`OpenLevelBySoftObjectPtr` 软引用跳图（不强制加载地图对象进内存）。**安全修复痕迹**：Map 里找不到时打 Log 警告而不是 FindChecked 崩溃（git 提交的"断言降级"精神）。
- **`ChoosePlayerStart_Implementation`**：覆写 GameMode 的出生点选择——按 **GameInstance->PlayerStartTag** 匹配关卡里 PlayerStart 的 `PlayerStartTag`；匹配不到就用第一个。**链路**：玩家踩 CheckPoint → SaveProgress 存 CheckPointTag → GameInstance->PlayerStartTag → 下次 ChoosePlayerStart 选对出生点 → **复活在检查点旁**。跨地图跳转时 LoadSlot 的 PlayerStartTag 一路传递。
- **`PlayerDied`**：读局内存档的 MapAssetName → `OpenLevel` 重开当前图（走 ChoosePlayerStart 选检查点）。**死亡重生 = 重开地图**，简单粗暴但闭环完整。

## 7.3 BeginPlay 注册默认地图

```cpp
void ADuraGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    Maps.Add(DefaultMapName, DefaultMap);
}
```
- 编辑器只配 `DefaultMapName/DefaultMap` 一对 + 可选 Maps 表；BeginPlay 时把默认图塞进注册表——**代码兜底配置**，策划少配一项也不至于查无此图。

---

# 八、DuraGameInstance —— 跨地图的"会话背包" ★★★☆☆

```cpp
UCLASS()
class DURA_API UDuraGameInstance : public UGameInstance
{
public:
    UPROPERTY() FName PlayerStartTag = FName();
    UPROPERTY() FString LoadSlotName = FString();
    UPROPERTY() int32 LoadSlotIndex = 0;
};
```

- **生命周期**：GameInstance 从引擎启动活到退出，**跨越所有地图切换**（GameMode/关卡每张图重建）。所以"当前玩的是哪个存档槽、出生在哪"这种跨图信息放这里。
- 数据流：主菜单选槽 → GameInstance.LoadSlotName/Index 被赋值 → 跳图 → 新地图的 GameMode 用这对值读写存档 → CheckPoint 更新 PlayerStartTag → 死亡重生用它选出生点。
- **为什么不用静态变量**：GameInstance 是引擎管理的单例（有生命周期、蓝图可访问、PIE 多实例安全），静态变量是反模式。
- 面试考点：*GameInstance / GameMode / GameState / PlayerState 生命周期与职责区分*（经典必考，务必自己能画表）。

---

# 九、本章全链路总图

```
                     ┌────────────────────────────┐
                     │ UDuraGameInstance（跨图）     │
                     │ LoadSlotName / PlayerStartTag│
                     └──────────┬─────────────────┘
                                │ 槽位信息
      ┌─────────────────────────▼──────────────────────────┐
      │ ADuraGameModeBase（服务器大管家）                       │
      │ CharacterClassInfo / AbilityInfo / LootTiers 数据资产 │
      │ 存档 CRUD / 世界状态 / ChoosePlayerStart / PlayerDied │
      └──────┬─────────────────────────────┬───────────────┘
             │ 玩家轨道                      │ 敌人轨道
┌────────────▼────────────┐   ┌────────────▼─────────────┐
│ ADuraCharacter（Pawn）    │   │ ADuraEnemy（Pawn=ASC宿主） │
│  PossessedBy ──┐          │   │  BeginPlay:               │
│  OnRep_PlayerState ─┤     │   │   InitAbilityActorInfo    │
│        ▼       │(双轨)    │   │   (this,this) + 属性      │
│  InitAbilityActorInfo()  │   │   GiveStartupAbilities    │
│   借用 PlayerState 的 ASC │   │   血条绑定/委托注册          │
│   HUD->InitOverlay       │   │  PossessedBy: 行为树启动    │
│  LoadProgress(存/读档)    │   │  Die: LifeSpan+掉落+黑板   │
└────────────┬────────────┘   └────────────┬─────────────┘
             │      共同基类 ADuraCharacterBase              │
             │  Die→MulticastHandleDeath(物理/溶解/委托)      │
             │  StunTagChanged(移速) / TakeDamage(桥)        │
             │  ApplyAttributeInitEffectToSelf(GE四步曲)     │
             └──────────────────────────────────────────────┘
```

# 十、动手实验建议

1. **把敌人 ASC 复制模式从 Minimal 改成 Mixed**，PIE 双窗观察网络流量（`net.PktLag=200` + 控制台 stat net），体会模式差异。
2. 在 `OnRep_PlayerState` 里去掉 `RemoveAll(this)`，让 InitAbilityActorInfo 跑两次（例如在 PossessedBy 也手动调），观察眩晕回调重复执行的日志爆炸——理解防重订阅。
3. 给一个宝箱 Actor 实现 SaveInterface（UPROPERTY(SaveGame) bool），用 GameMode 的 SaveWorldState/LoadWorldState 存读，观察字节流长度变化。

---

*下一篇：`03-输入分发链路.md` —— 鼠标点击如何变成技能释放：InputConfig/EnhancedInputComponent/PlayerController 的委托接力。*
