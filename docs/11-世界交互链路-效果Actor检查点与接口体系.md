# 11 · 世界交互链路 —— 效果 Actor、检查点、地图入口与接口体系

> **逻辑链路一句话概括**：
> `DuraEffectActor（药水/篝火）把三种 GE 策略（瞬时/持续/无限）按重叠事件应用到碰到的角色，并管理"无限 GE 的回收"` → `CheckPoint 继承 APlayerStart：被踩到 = 存世界状态 + 存玩家进度 + 点亮自己，死亡重生时 GameMode 按 PlayerStartTag 选中它` → `MapEntrance 继承 CheckPoint：存档目标图信息后跳图` → **所有世界物件的"可选中/可交互/可存档"由四个接口（Highlight/Player/Enemy/Save）声明**。
> 本篇是"接口设计"的集中展示——**四个接口如何把一票 Actor 组织成可扩展的世界系统**。

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/Interaction/SaveInterface.h` | 存档接口（2 函数） | ★★★★☆ |
| 2 | `Source/Dura/Public/Interaction/HighlightInterface.h` | 高亮接口（3 函数） | ★★★★☆ |
| 3 | `Source/Dura/Public/Interaction/PlayerInterface.h` / `EnemyInterface.h` | 玩家/敌人接口（回顾+归类） | ★★★☆☆ |
| 4 | `Source/Dura/Public/Actor/DuraEffectActor.h` / `.cpp` | 效果 Actor（GE 三策略+移动动画） | ★★★★★ |
| 5 | `Source/Dura/Public/CheckPoint/CheckPoint.h` / `.cpp` | 检查点（双接口+存档触发） | ★★★★★ |
| 6 | `Source/Dura/Public/CheckPoint/MapEntrance.h` / `.cpp` | 地图入口（继承复用） | ★★★★☆ |
| 7 | `Source/Dura/Public/Actor/MagicCircle.h` / `.cpp` | 魔法阵贴花 | ★★★☆☆ |

---

# 一、接口体系总览（先建立地图再进场景）★★★★☆

## 1.1 四接口的职责划分

| 接口 | 函数 | 回答的问题 | 实现者 |
|---|---|---|---|
| **ICombatInterface**（第 2 篇） | 17 个 | "你作为战斗单位的基本盘？" | 玩家/敌人（CharacterBase） |
| **IPlayerInterface** | 13 个 | "你的玩家特权操作？" | 仅 DuraCharacter |
| **IEnemyInterface** | 2 个 | "你的战斗目标是谁？" | 仅 DuraEnemy |
| **IHighlightInterface** | 3 个 | "光标悬停/点击移动时你的表现？" | DuraEnemy / CheckPoint / MapEntrance |
| **ISaveInterface** | 2 个 | "世界状态存读档时你的参与方式？" | CheckPoint / MapEntrance / SpawnVolume 等 |

- **`IPlayerInterface` 函数清单**（第 2/4 篇的消费者在此汇合）：XP/等级/点数的读写（FindLevelForXP/GetXP/AddToXP/AddToPlayerLevel/GetAttributePointsReward/GetSpellPointsReward/AddToAttributePoints/AddToSpellPoints/GetAttributePoints/GetSpellPoints/LevelUp）+ **ShowMagicCircle/HideMagicCircle**（地面瞄准）+ **SaveProgress**（检查点存档）。
- **为什么 XP 加成在"玩家接口"而不在 CombatInterface**：Interface 的设计准则是**最小惊讶**——查一个 Actor"是不是玩家"后，调用方只想知道"能调哪些玩家专属操作"。**接口按"角色身份"切**而不是按"功能域"切，避免 God-Interface。
- **接口声明形态统一**：全部 `UFUNCTION(BlueprintNativeEvent)`（蓝图可覆盖+C++ 默认实现），消费侧统一 `Execute_Xxx(Actor, ...)`。

## 1.2 一个 Actor 可以实现多个接口（正交能力）

CheckPoint 的继承声明就是标本：

```cpp
class DURA_API ACheckPoint : public APlayerStart, public ISaveInterface, public IHighlightInterface
```

**"能力是插件、身份是骨架"**——ATargetPoint/PlayerStart 定身份，ISave/IHighlight/IPlayer 逐个叠能力。面试讲"UE Interface 的组合优于继承"用这个例子。

---

# 二、DuraEffectActor —— 效果 Actor（★★★★★）

## 2.1 移动动画：奖励品的"呼吸感"

```cpp
void ADuraEffectActor::ItemMovement(float DeltaTime)
{
    if(bRotates)
    {
        const FRotator DeltaRotation(0.f, DeltaTime * RotationRate, 0.f);
        CalculatedRotation = UKismetMathLibrary::ComposeRotators(CalculatedRotation, DeltaRotation);    
    }

    if(bSinusoidalMovement)
    {
        const float Sine = SineAmplitude * FMath::Sin(RunningTime * SinePeriodConstant);
        CalculatedLocation = InitialLocation + FVector(0.f, 0.f, Sine);
    }
}
```

- **旋转**：每帧累加 Yaw（`ComposeRotators` 组合旋转——比直接加 Angle 好，处理了万向节细节）。**为什么存 CalculatedRotation 而不直接 SetActorRotation**：**蓝图的根运动/粒子常要用"计算值"**（BlueprintReadWrite 的用途）——实际 SetActorLocation/Rotation 在蓝图子类里做（WBP/Pickup 蓝图 Tick 里把 Calculated 应用到组件）——**C++ 算、蓝图演**的分工。
- **正弦浮动**：`Amplitude × Sin(t × 周期常数)` 沿 Z 浮动——**药水悬浮感**。`RunningTime` 周期性归零（`2π/常数`）防浮点大数精度损失（长跑游戏 sin(1e7) 会抖）。
- **`PrimaryActorTick.bCanEverTick = false` 但 Tick 被覆写**：**默认关 Tick**——不动的拾取物（篝火）不耗 Tick；要动的蓝图子类调 `StartSinusoidalMovement/StartRotation`（BlueprintCallable）时再开。**Tick 按需开启**是性能基本功。

## 2.2 三种 GE 策略（本类核心设计）

```cpp
UPROPERTY(EditAnywhere, ...) TSubclassOf<UGameplayEffect> InstantGameplayEffectClass;
UPROPERTY(EditAnywhere, ...) EEffectApplicationPolicy InstantEffectApplicationPolicy = DoNotApply;
UPROPERTY(EditAnywhere, ...) TSubclassOf<UGameplayEffect> DurationGameplayEffectClass;
UPROPERTY(EditAnywhere, ...) EEffectApplicationPolicy DurationEffectApplicationPolicy = DoNotApply;
UPROPERTY(EditAnywhere, ...) TSubclassOf<UGameplayEffect> InfiniteGameplayEffectClass;
UPROPERTY(EditAnywhere, ...) EEffectApplicationPolicy InfiniteEffectApplicationPolicy = DoNotApply;
UPROPERTY(EditAnywhere, ...) EEffectRemovePolicy InfiniteEffectRemovePolicy = RemoveOnEndOverlap;
```

- **GE 三种 DurationPolicy 对应三种拾取场景**：
  - **Instant（瞬时）**：回血瓶——碰到立刻 +50 血，完事。
  - **Duration（持续）**：碰到的"力量祭坛"——+10 力量 30 秒。
  - **Infinite（无限）**：水潭——站进去每秒扣血，**离开就停**。
- **ApplicationPolicy（何时应用）**：`ApplyOnOverlap / ApplyOnEndOverlap / DoNotApply`——**应用时机也是配置**（有的效果进圈给，有的出圈才给——"陷阱出口毒"之类）。
- **RemovePolicy（何时移除）**：只对 Infinite 有意义（Instant 用不着移除、Duration 自然过期）——`RemoveOnEndOverlap / DoNotRemove`。
- **一个 Actor 可以同时配三种**（都生效时逐个应用）——**策略组合爆炸被枚举+三组字段化解**，蓝图子类只填想要的那组。

## 2.3 ApplyEffectToTarget —— 应用与登记

```cpp
void ADuraEffectActor::ApplyEffectToTarget(AActor* Target, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
    if (!bApplyEffectsToEnemies && Target->ActorHasTag(FName("Enemy")))
    {
        return;      // 敌人过滤（默认只对玩家生效）
    }

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (ASC == nullptr) return;      // 没有 ASC 的 Actor（箱子等）免疫

    check(GameplayEffectClass);

    FGameplayEffectContextHandle EffectContextHandle = ASC->MakeEffectContext();
    EffectContextHandle.AddSourceObject(this);      // 溯源：是"药水"给的效果

    const FGameplayEffectSpecHandle EffectSpecHandle = ASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
    const FActiveGameplayEffectHandle ActiveEffectHandler = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data);

    const bool bIsInfinite = EffectSpecHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::Infinite;
    if (bIsInfinite && InfiniteEffectRemovePolicy == EEffectRemovePolicy::RemoveOnEndOverlap)
    {
        ActiveHandlers.Add(ActiveEffectHandler, ASC);   // 登记，等离开时回收
    }
    
    if (bDestroyOnEffectApplication && !bIsInfinite)
    {
        Destroy();
    }
}
```

- **四道门**：敌人过滤（ActorTag）→ ASC 判空 → GE 类 check → 应用。**注意门 1 的双重身份**：`bApplyEffectsToEnemies=false`（默认）时敌人踩血瓶不回血——**掉落物被敌人"抢走"是 bug**，这就是为什么默认排除。
- **`ActorLevel`**：GE 按此等级查曲线——**拾取物自己有等级**（10 级区域的血瓶回 100 血）。
- **`ApplyGameplayEffectSpecToSelf` 的返回值**：**FActiveGameplayEffectHandle**——已生效效果的"句柄"（凭它可查询/移除该 GE）。**只有这里才拿得到句柄**，所以 Infinite 的回收必须在此登记。
- **`EffectSpecHandle.Data->Def->DurationPolicy`**：从 Spec 读出 GE 定义的时长策略——**运行时判断"这是不是无限 GE"**（同一个函数服务三种类型，靠策略自省分流）。
- **`bDestroyOnEffectApplication && !bIsInfinite`**：应用即销毁的拾取物**不能**是 Infinite（销毁了就没人负责移除了——**自毁与回收的互斥**，逻辑自洽检查）。

## 2.4 ActiveHandlers —— 无限 GE 的回收账本

```cpp
TMap<FActiveGameplayEffectHandle, UAbilitySystemComponent*> ActiveHandlers;

void ADuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
    ...三种策略的 EndOverlap 应用...
    if (InfiniteEffectRemovePolicy == EEffectRemovePolicy::RemoveOnEndOverlap)
    {
        UAbilitySystemComponent* ASC = ...;
        if (!IsValid(ASC)) return;

        TArray<FActiveGameplayEffectHandle> Handlers;
        for (const auto& pair : ActiveHandlers)
        {
            if (pair.Value == ASC)      // 找到"这个角色"登记的所有效果
            {
                ASC->RemoveActiveGameplayEffect(pair.Key, 1);   // 移除（StacksToRemove=1）
                Handlers.Add(pair.Key);
            }
        }
        for (const auto& handler : Handlers)
        {
            ActiveHandlers.FindAndRemoveChecked(handler);   // 账本销账
        }
    }
}
```

- **为什么要账本**：玩家 A 站进水潭拿了"每秒扣血"，玩家 B 也站进去了——**两个角色各有一条 Infinite GE**。离开时**只移除自己的**：账本以"句柄→ASC"记录归属，EndOverlap 时按离开者的 ASC 摘除它的条目。
- **两段式遍历（★ 教科书细节）**：**先收集再删除**——直接在遍历 TMap 时 Remove 会失效迭代器（容器修改未定义行为）。第一循环收集 Handlers、第二循环销账。**"不可边遍历边删"**是容器操作铁律。
- **`RemoveActiveGameplayEffect(Handle, StacksToRemove=1)`**：第二参是要移除的层数。
- **`FindAndRemoveChecked`**：找到并移除（找不到断言）——账本里一定有（刚收集的），断言成立。
- **通俗解释**：水潭是"健身房"，进门发一条"运动中"效果（Infinite GE），出门时健身房按**登记表**（ActiveHandlers）把你发的那条收走——不能误收别人的。

---

# 三、CheckPoint —— 检查点（★★★★★）

## 3.1 继承 APlayerStart 的深意

```cpp
class DURA_API ACheckPoint : public APlayerStart, public ISaveInterface, public IHighlightInterface
```

- **APlayerStart**：引擎的"出生点"Actor，自带 **`PlayerStartTag`**（FName）——第 2 篇 GameMode::ChoosePlayerStart 按这个 Tag 选出生点。
- **检查点=出生点**的设计：激活检查点 = `SaveProgress(自己的 PlayerStartTag)` 把 Tag 写进存档 → 死亡重生时 GameMode 选中**这个**检查点——**"激活处"与"复活处"天然是同一个 Actor**，不需要额外的位置同步。
- **构造函数**：

```cpp
ACheckPoint::ACheckPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CheckPointMesh = CreateDefaultSubobject<UStaticMeshComponent>("CheckPointMesh");
    CheckPointMesh->SetupAttachment(GetRootComponent());
    CheckPointMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CheckPointMesh->SetCollisionResponseToAllChannels(ECR_Block);

    Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Sphere->SetupAttachment(CheckPointMesh);

    MoveToComponent = CreateDefaultSubobject<USceneComponent>("MoveToComponent");
    MoveToComponent->SetupAttachment(GetRootComponent());

    CheckPointMesh->SetCustomDepthStencilValue(CustomDepthStencilOverride);   // 默认黄褐（HIGHLIGHT_COLOR_TAN）
    CheckPointMesh->MarkRenderStateDirty();
}
```

- **`(const FObjectInitializer&)` 构造签名**：**为什么要带 ObjectInitializer 的构造函数**——APlayerStart 的胶囊/根组件初始化需要（基类要求显式传递时）。MapEntrance 里用它额外调 `Sphere->SetupAttachment(MoveToComponent)` 调整挂接（见后）。
- **双碰撞体**：Mesh=Block（实体，玩家要站上去/撞到）；Sphere=QueryOnly+只对 Pawn Overlap（**触发器**）。**"实体+触发器"分离**是世界物件的通用布局（对比 SpawnVolume：只有触发器）。
- **`CustomDepthStencilOverride`**：描边颜色可覆写（默认 252=黄褐），**蓝图子类改颜色**（终局检查点变色之类）。
- **MoveToComponent**：**点击移动的目的地微调点**——第 3 篇 `SetMoveToLocation_Implementation` 里 `OutDestination = MoveToComponent->GetComponentLocation()`：玩家点检查点，角色走到 MoveToComponent 处（检查点**前方**）而不是撞进触发球中心。**空组件当"交互站位锚"**。

## 3.2 触发即存档（与世界状态+玩家进度的双保存）

```cpp
void ACheckPoint::OnSphereOverlap(...)
{
    if(OtherActor->Implements<UPlayerInterface>())
    {
        bReached = true;

        //保存世界状态
        if(ADuraGameModeBase* DuraGM = Cast<ADuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
        {
            const UWorld* World = GetWorld();
            FString MapName = World->GetMapName();
            MapName.RemoveFromStart(World->StreamingLevelsPrefix);

            DuraGM->SaveWorldState(GetWorld(), MapName);
        }

        //保存玩家状态
        IPlayerInterface::Execute_SaveProgress(OtherActor, PlayerStartTag);

        HandleGlowEffects();
    }
}
```

- **触发链三连**：
  1. **SaveWorldState(World, MapName)**：把当前地图所有 SaveInterface Actor（宝箱/其他检查点/刷怪区）的状态写档（第二参传当前图名——第 2 篇签名里它是"目的地"，这里当"当前图名"用，语义两栖：**"存档时的地图归属"**）。
  2. **Execute_SaveProgress(玩家, PlayerStartTag)**：玩家进度（第 2 篇：等级/XP/点数/四维/技能快照）。
  3. **HandleGlowEffects**：点亮。
- **`bReached` 是 `SaveGame` 属性**：`UPROPERTY(BlueprintReadWrite, SaveGame)`——重进地图时 LoadActor 恢复"已激活"状态。
- **MapName 的清洗**：`RemoveFromStart(StreamingLevelsPrefix)`——PIE 前缀剥离（第 2 篇存档同款细节，检查点独立又做了一遍——**两处调用方各自清洗**，保证无论谁发起存档键都是干净的）。

```cpp
void ACheckPoint::LoadActor_Implementation()
{
    if(bReached)
    {
        HandleGlowEffects();
    }
}

void ACheckPoint::HandleGlowEffects()
{
    Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 已激活不再触发
    UMaterialInstanceDynamic* DynamicMaterialInstance = UMaterialInstanceDynamic::Create(CheckPointMesh->GetMaterial(0), this);
    CheckPointMesh->SetMaterial(0, DynamicMaterialInstance);
    CheckpointReached(DynamicMaterialInstance);   // 蓝图事件：动态材质推进发光动画
}
```

- **LoadActor（读档回调）**：已激活的检查点**重新发光**——玩家读档看到"我到过哪"的视觉记忆。**读档=状态恢复+表现恢复**的典型。
- **HandleGlowEffects**：克隆当前材质为 MID（`GetMaterial(0)` 取再 `Create`）→ 蓝图事件里用 MID 的标量参数推进发光（BlueprintImplementableEvent）。**克隆当前材质而不是配新材质**——不同检查点可以有不同底色，发光效果通用。
- **`bCallOverlapCallback`（BeginPlay 里决定是否绑重叠）**：**蓝图开关**——MapEntrance 之类子类可能有别的触发策略；也允许关卡设计师"摆一个永久不触发的展示检查点"。

## 3.3 高亮接口实现（受击检查点不高亮）

```cpp
void ACheckPoint::HighlightActor_Implementation()
{
    if(!bReached)
    {
        CheckPointMesh->SetRenderCustomDepth(true);   // 已激活的不高亮
    }
}
void ACheckPoint::UnHighlightActor_Implementation()
{
    CheckPointMesh->SetRenderCustomDepth(false);
}
```
- **状态感知的高亮**：未激活才发亮提示"可以交互"；激活过的不再吸引注意力。**高亮不是纯视觉函数，可以带业务判断**。

---

# 四、MapEntrance —— 地图入口（★★★★☆）

```cpp
class DURA_API AMapEntrance : public ACheckPoint
{
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UWorld> DestinationMap;

    UPROPERTY(EditAnywhere)
    FName DestinationPlayerStartTag;
};

void AMapEntrance::OnSphereOverlap(...)
{
    if(OtherActor->Implements<UPlayerInterface>())
    {
        bReached = true;

        if(ADuraGameModeBase* DuraGM = ...)
        {
            DuraGM->SaveWorldState(GetWorld(), DestinationMap.ToSoftObjectPath().GetAssetName());
        }

        IPlayerInterface::Execute_SaveProgress(OtherActor, DestinationPlayerStartTag);

        UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationMap);
    }
}
```

- **继承复用的教科书**：MapEntrance 覆写三个点——
  1. **SaveWorldState 的第二参传"目的地地图名"**：离开时存档的 MapAssetName=目的地图（读档/死亡重生会直接进目的地图，第 2 篇 PlayerDied 的 `OpenLevel(SaveGame->MapAssetName)` 就读它）。**同一个参数两种语义**（CheckPoint 传"当前图"、MapEntrance 传"目标图"）——签名通用、调用方赋予语义。
  2. **SaveProgress 传 `DestinationPlayerStartTag`**：玩家将出生在**目的地图**的这个 Tag 的 PlayerStart 上——**跨图出生点**。
  3. 最后 `OpenLevelBySoftObjectPtr` 跳图。
- **`LoadActor_Implementation` 覆写为空**（注释"地图入口在存档加载时无需任何操作"）：bReached 对入口没意义（进了就跳走了）——**覆写掉父类的发光逻辑**，继承者可以"取消"父类行为。
- **构造函数调 `Sphere->SetupAttachment(MoveToComponent)`**：入口的触发球挂在"站位锚"上（玩家走近锚点才触发）而不是 Mesh——**子类微调组件拓扑**，这就是 ObjectInitializer 构造存在的意义。
- **`HighlightActor_Implementation` 覆写为空**：入口**不高亮**（跳图是自动触发的，不需要吸引点击）——又一处行为覆写。
- **面试点**：*"MapEntrance 如何复用 CheckPoint 且改变哪些行为？"* —— 三个覆写（存档目的地/出生点/高亮取消/LoadActor 取消/球挂点）+ 继承全部触发与存档机制。

---

# 五、SaveInterface —— 存档协议（★★★★☆）

```cpp
UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
bool ShouldLoadTransform();      // 读档时要不要恢复位置

UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
void LoadActor();                // 读档后的"自我恢复"回调
```

- **两个函数构成最小存档协议**：`ShouldLoadTransform`（位置要不要存——宝箱要、敌人尸体不要）、`LoadActor`（字节流恢复后的行为钩子——SpawnVolume 自毁、CheckPoint 发光）。
- **与 GameMode 的关系**（第 2 篇）：SaveWorldState 用 `Actor->Implements<USaveInterface>()` 筛选 + `Actor->Serialize(Archive)`（反射收 SaveGame 属性）+ `Execute_LoadActor`。**接口+反射+字节流的组合拳**在 GameMode，**本接口只声明协议**——协议与机制分离。
- **新增可存档 Actor 的三步**：实现 ISaveInterface（两个函数）→ 把需要存的状态标 `UPROPERTY(SaveGame)` → 完事。**零存档系统改动**——这就是协议设计的收益。

---

# 六、MagicCircle —— 魔法阵（★★★☆☆）

```cpp
UCLASS()
class AMagicCircle : public AActor
{
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UDecalComponent> MagicCircleDecay;  
};

void AMagicCircle::SetMaterial(int32 Index, UMaterialInterface* DecalMaterial)
{
    MagicCircleDecay->SetMaterial(Index, DecalMaterial);
}
```

- **贴花组件（UDecalComponent）**：**投射到地面几何上的材质**（不占物体、贴着地形起伏）——魔法阵的标准实现。
- **单函数包装**：`SetMaterial` 只是转发（为什么不 public DecalComponent？**封装**——蓝图调 Actor 级 API 而不是摸组件内部；类型是 DecalComponent 的细节被隐藏，未来换成 Niagara 地面标记也不用改调用方）。
- **消费链回顾（第 3 篇）**：PlayerController 生成单例 MagicCircle → 每帧 `SetActorLocation(hitResult.ImpactPoint)` → 技能（ArcaneShards 蓝图）`ShowMagicCircle(紫色贴花)` 注入材质。**位置归 PC 管、材质归技能管、渲染归贴花管**——三方协作的最小 Actor。
- 成员名 `MagicCircleDecay` 是 `Decal` 的拼写笔误（Decay=衰减）——读代码别被带偏。

---

# 七、本章交互全景图

```
光标系统（第 3 篇 MouseTrace）
   ├─ 悬停 → IHighlightInterface::HighlightActor（敌人红描边 / 未激活检查点黄描边）
   └─ 点击 → IHighlightInterface::SetMoveToLocation（检查点的站位锚修正寻路终点）

玩家实体碰撞（Pawn Overlap 触发器族）
   ├─ DuraEffectActor ── GE 三策略（Instant/Duration/Infinite × Overlap/EndOverlap）
   │      └─ ActiveHandlers 账本：Infinite GE 按归属回收（两段式遍历防迭代失效）
   ├─ CheckPoint（继承 APlayerStart）
   │      └─ 触发 → SaveWorldState(当前图) + SaveProgress(PlayerStartTag) + 点亮
   │             （bReached 存档 → 读档 LoadActor 恢复发光）
   └─ MapEntrance（继承 CheckPoint）
          └─ 触发 → SaveWorldState(目标图) + SaveProgress(目标图出生Tag) + OpenLevel

存档协议（第 2 篇机制 + 本篇协议）
   SaveInterface（ShouldLoadTransform/LoadActor）+ UPROPERTY(SaveGame) 字段
   ↔ GameMode::SaveWorldState/LoadWorldState（字节流反射序列化）
```

**面试考点**：
1. **GE 的三种 DurationPolicy 在拾取物场景的配置组合**（Instant/Duration/Infinite × Apply/Remove 时机）
2. **Infinite GE 的生命周期管理为什么要账本**（多角色归属 + 句柄移除）
3. **检查点为什么继承 APlayerStart**（激活处=复活处的天然统一）
4. **新增可存档世界物件的最小步骤**（接口实现+SaveGame 属性）

# 八、动手实验建议

1. 配一个"毒沼泽"：Infinite GE（每秒扣血）+ RemoveOnEndOverlap，双人 PIE（两个玩家）分别进出，观察 ActiveHandlers 只收走自己的效果（日志打印句柄数）。
2. 把 CheckPoint 的 `SaveWorldState` 第二参换成别的图名，死亡重生观察重生到错误地图——理解 MapAssetName 在存档里的"当前所在地"语义。
3. 新建一个宝箱 Actor：实现 ISaveInterface（UPROPERTY(SaveGame) bool bOpened）+ IHighlightInterface，开箱后 SaveWorldState/LoadWorldState 验证状态恢复——亲手跑通"新增可存档物件三步"。

---

*下一篇：`12-投射物与法术Actor链路.md` —— DuraProjectile 的碰撞/重叠/伤害注入与 FireBall 的往返爆炸。*
