# 12 · 投射物与法术 Actor 链路 —— 碰撞命中、网络销毁表现与地面点集

> **逻辑链路一句话概括**：
> `技能（第 6 篇）SpawnActorDeferred 生成投射物并注入 FDamageEffectParams` → `弹丸靠 Sphere 重叠检测飞行命中` → `IsValidOverlap 三道过滤（来源有效/不打自己/不打友军）` → `服务器补全击退/死亡冲量方向 → ApplyDamageEffect（第 5 篇管线）→ Destroy` → **Destroy 的网络复制在每台机器触发 Destroyed → 各端本地播命中特效** → `FireBall 子类改成"穿透伤+归途爆炸"，爆炸用非复制 GameplayCue 省带宽` → `PointCollection 为地面技能提供"贴地取点"服务`。

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/Actor/DuraProjectile.h` / `.cpp` | 投射物基类（碰撞/命中/网络销毁） | ★★★★★ |
| 2 | `Source/Dura/Public/Actor/DuraFireBall.h` / `.cpp` | 火球子类（穿透+GameplayCue 爆炸） | ★★★★☆ |
| 3 | `Source/Dura/Public/Actor/PointCollection.h` / `.cpp` | 地面点集（ArcaneShards 碎片落点） | ★★★☆☆ |

---

# 一、ADuraProjectile —— 投射物基类 ★★★★★

## 1.1 构造函数：碰撞与运动学

```cpp
ADuraProjectile::ADuraProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;                    // 弹丸是网络复制 Actor

    Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);       // 只查询不物理
    Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);           // 先全部忽略
    Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);  // 动态物体
    Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);   // 静态世界
    Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);          // 角色
    Sphere->SetCollisionObjectType(ECC_Projectile);                  // 自己是"投射物"通道

    SetRootComponent(Sphere);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
    ProjectileMovement->InitialSpeed = 500.0f;
    ProjectileMovement->MaxSpeed = 550.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;    // 无重力（直线弹道）
}
```

- **弹丸碰撞的三条规则**：
  1. **QueryOnly**：不需要物理反弹，只要"重叠检测"。
  2. **白名单 Overlap**（世界静态/动态/Pawn）：只对这三类起反应；**其他一切忽略**（相机、拾取物、其他弹丸互相穿过）。
  3. **`ObjectType=ECC_Projectile`**（第 1 篇的宏，GameTraceChannel1）：把自己归类到"投射物"对象通道——**反向被忽略的钥匙**：角色 Mesh 对 `ECC_Projectile` 设 Overlap（第 2 篇）、敌人武器/相机对它 Ignore。**ObjectType 是"我是谁"，Response 是"我对谁有反应"，双向都要配**。
- **`ProjectileGravityScale = 0`**：零重力直线飞行（法术弹）。想抛物线弹道就调大。
- **`bReplicates = true`（构造期）**：弹丸服务器生成、客户端复制显示。**网络角色**：服务器权威判定命中，客户端看弹道表现。
- **`bCanEverTick = false`**：弹丸的运动由 MovementComponent（内部 Tick）驱动，Actor 本身不需要 Tick——**Tick 职责让渡给组件**。

## 1.2 BeginPlay 与飞行音效

```cpp
void ADuraProjectile::BeginPlay()
{
    Super::BeginPlay();
    SetLifeSpan(LifeSpan);                 // 15 秒自毁（兜底：没打中也要回收）
    SetReplicateMovement(true);            // 移动复制（客户端看到弹道）
    Sphere->OnComponentBeginOverlap.AddDynamic(this, &ADuraProjectile::OnSphereOverlap);
    LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());  // 飞行循环音
}
```

- **`SetReplicateMovement(true)`**：**必须显式开**——bReplicates 只复制"存在"，**移动复制单独开关**。忘开的结果：客户端弹丸原地不动直到命中瞬移。这是弹丸网络同步的第二个必配项（面试点：bReplicates 与 SetReplicateMovement 的分工）。
- **`SpawnSoundAttached(音效, 根组件)`**：飞行呼啸声**挂在弹丸上**跟随移动（区别于 SpawnSoundAtLocation 定点播放）。返回 UAudioComponent 存下来——命中/销毁时要**手动停**（循环音不会自己停）。

## 1.3 OnSphereOverlap —— 命中决策（★★★★★）

```cpp
void ADuraProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, 
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if(!IsValidOverlap(OtherActor)) return;

    if (!bHit) OnHit();    // 本地立即播命中特效（不等服务器）

    if (HasAuthority())
    {
        if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
        {
            const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
            DamageEffectParams.DeathImpulse = DeathImpulse;      // 补全死亡冲量方向

            const bool bKnockback = FMath::RandRange(1, 100) <= DamageEffectParams.KnockbackChance;
            if(bKnockback)
            {
                FRotator Rotation = GetActorRotation();
                Rotation.Pitch = 45.f;                            // 击退固定 45° 仰角

                const FVector KnockbackDirection = Rotation.Vector();
                const FVector KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackMagnitude;
                DamageEffectParams.KnockbackForce = KnockbackForce;
            }     
            DamageEffectParams.TargetAbilitySystemComponent = TargetASC;   // 最后一块拼图：目标
            UDuraAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);  // 第 5 篇入口
        }
        Destroy();      // 服务器销毁 → 复制销毁事件 → 各客户端 Destroyed
    }
    else
    {
        bHit = true;    // 客户端：只标记，防止自己的 Destroyed 再播
    }
}
```

**这段是"客户端预测表现 + 服务器权威结算"的标准范本**，逐点拆：

- **`if (!bHit) OnHit()`**：**命中表现先于结算**——本地立刻播爆炸特效/音效（OnHit），不等服务器往返（网络延迟下表现先行，预测思想）。bHit 防重入（多组件同时重叠回调）。
- **服务器分支**：
  - **死亡冲量方向补全**：`ForwardVector × Magnitude`——弹丸飞行方向就是"打死你的方向"，尸体朝弹道方向飞。回顾第 6 篇：技能生成弹丸时 Params 里只有 Magnitude（方向未知），**方向在命中瞬间才知道**——增量填充的最后一站。
  - **击退掷骰**：`KnockbackChance` 概率触发；触发时方向=弹道方向+45° 仰角（把人**抛起**）。
  - **TargetASC 补全 → ApplyDamageEffect**：Params 的字段全部就位（发起时填的数值 + 命中时补的方向与目标），进入伤害管线（ExecCalc→AttributeSet）。
  - **`Destroy()`**：服务器销毁。**Destroy 的复制**：销毁事件复制到客户端 → 客户端调用 `Destroyed()`。
- **客户端分支**：只 `bHit = true`——**为什么不 Destroy**：销毁由服务器复制驱动（服务器 Destroy → 客户端 Destroyed → 引擎销毁本地代理）；客户端自己 Destroy 会破坏复制一致性。
- **IsValidOverlap（三道门）**：

```cpp
bool ADuraProjectile::IsValidOverlap(AActor* OtherActor)
{
    if(!DamageEffectParams.SourceAbilitySystemComponent) return false;   // 无来源（未注入参数的弹）不伤人

    AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
    if (SourceAvatarActor == OtherActor) return false;                   // 不打自己
    
    if(!UDuraAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor)) return false;  // 不打友军（第 5 篇）

    return true;
}
```

- **自我排除的必要性**：弹丸从手部 Socket 出生，出生点与自身角色重叠——不过滤的话火球一出膛就炸自己脸上。
- **IsNotFriend 用 ActorTag**（"Player"/"Enemy"）——弹丸判断阵营的最轻量路径（第 5 篇详述）。

## 1.4 Destroyed 与命中表现的全端一致（★★★★★ 网络设计精华）

```cpp
void ADuraProjectile::Destroyed()
{
    if( LoopingSoundComponent ) 
    { 
        LoopingSoundComponent->Stop();
        LoopingSoundComponent->DestroyComponent();
    }

    if (!bHit && !HasAuthority()) OnHit();   // ★ 关键行

    Super::Destroyed();
}
```

- **`if (!bHit && !HasAuthority()) OnHit()`**：**命中时服务器 OnHit 播了、服务器 Destroy → 客户端收到销毁 → 客户端 Destroyed 里发现"我还没播过"→ 本地补播 OnHit**。
- **覆盖的第三种情况**：弹丸**超时自毁**（LifeSpan 15 秒）或打在**无 ASC 的物体**上（IsValidOverlap 挡下）——服务器 Destroy 但从未进过 Overlap 伤害分支 → **客户端从未收到 bHit** → Destroyed 兜底让**所有端都能看到"弹丸消失时的爆裂特效"**。
- **各端的 bHit 时序**：命中目标时客户端提前在 Overlap 里置 bHit（预测播了）→ 复制销毁到达时 `!bHit` 为 false 不重复播；未命中情形 bHit=false → 补播。**一个标志位 + 一行兜底 = 三种网络场景全正确**。这是**面试讲"网络同步表现"的黄金例子**。
- **循环音的停止也放 Destroyed**：无论哪条路径销毁（命中/超时/服务器提前销毁），飞行音必须停——**资源清理放在"必经之路"**。

```cpp
void ADuraProjectile::OnHit()
{
    bHit = true;
    UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation(), FRotator::ZeroRotator);
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, GetActorLocation());
    if( LoopingSoundComponent ) { LoopingSoundComponent->Stop(); LoopingSoundComponent->DestroyComponent(); }
}
```
- 命中表现三件套：定点爆音 + 命中 Niagara + 停飞行音。**没有特效网络同步**——每端各自本地生成（粒子不需要权威）。

---

# 二、ADuraFireBall —— 火球子类（★★★★☆）

## 2.1 与基类的三点差异

```cpp
class DURA_API ADuraFireBall : public ADuraProjectile
{
public:
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> ReturnToActor;       // 归宿（施法者）
    UPROPERTY(BlueprintReadWrite) FDamageEffectParams ExplosionDamageParams;  // 爆炸用的第二套参数
    UFUNCTION(BlueprintImplementableEvent) void StartOutgoingTimeline();  // 出膛动画（蓝图时间轴）
protected:
    virtual void OnSphereOverlap(...) override;   // 穿透：命中不销毁
    virtual void OnHit() override;                // 爆炸改用 GameplayCue
};
```

- **FireBlast 的弹**（第 6 篇生成 12 发）：飞出去 → **穿过敌人时各打一发直伤** → 时间轴把它**调头带回施法者** → 归途爆炸（ExplosionDamageParams，径向伤害）。
- **OnSphereOverlap 覆写（穿透语义）**：

```cpp
void ADuraFireBall::OnSphereOverlap(...)
{
    if(!IsValidOverlap(OtherActor)) return;

    if (HasAuthority())
    {
        if (UAbilitySystemComponent* TargetASC = ...)
        {
            const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
            DamageEffectParams.DeathImpulse = DeathImpulse;    
            DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
            UDuraAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
        }
    }
}
```

- **与基类的两处删改**：**删掉击退掷骰**（穿透伤不给击退——一群敌人被推来推去太乱）与**删掉 Destroy**（穿过继续飞）。**覆写=复制父类逻辑+删改**，可读性 OK；更优是父类拆出"命中结算"虚函数钩子——工程权衡。
- **客户端分支整个删掉**：穿透弹不做客户端提前 OnHit（爆炸在归途，提前播反而错）。
- **`StartOutgoingTimeline`（BeginPlay 里调）**：出膛的螺旋加速动画在蓝图时间轴——**动画表现外置蓝图**的又一例。
- **归途与爆炸**：蓝图时间轴控制弹丸转向/回归；归途中（或到达 ReturnToActor 时）蓝图调 ExplosionDamageParams 相关逻辑（配合第 5 篇径向伤害——爆炸中心/内外半径已在 Params 里）。

## 2.2 OnHit 与非复制 GameplayCue（★★★★☆）

```cpp
void ADuraFireBall::OnHit()
{
    if(GetOwner())
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = GetActorLocation();

        //节省带宽
        UGameplayCueManager::ExecuteGameplayCue_NonReplicated(
            GetOwner(),
            FDuraGameplayTags::Get().GameplayCue_FireBlast,
            CueParams
        );
    }
    if( LoopingSoundComponent ) { ...停音... } 
    bHit = true;
}
```

- **GameplayCue 快速课**：GAS 的"表现事件系统"——Tag 触发的粒子/音效/震动，两种执行方式：
  - **Replicated（`ExecuteGameplayCue`）**：服务器调 → 自动复制全端 → 每端播放。**权威单点触发**。
  - **Non-Replicated（`ExecuteGameplayCue_NonReplicated`）**：**只在调用机本地播**，不复制。
- **为什么这里用 Non-Replicated（注释"节省带宽"）**：FireBall 每端**都会**经历 OnHit（服务器命中时播 + 客户端 Destroyed 兜底播，1.4 节机制）——**如果用 Replicated Cue，服务器触发一次 → 复制包发 N 份 → 每端本来就要本地 OnHit 一次又各自播一份 = 每端播两次**。所以本地各播各的（Non-Replicated），**零网络开销、零重复**。**表现复制的去重思路：与其复制一次再端端播放，不如端端本地触发**。
- **`CueParams.Location`**：Cue 的位置参数（Cue 蓝图读它决定粒子在哪）。**GetOwner 判空**：Cue 挂 Owner（施法者）执行——Owner 销毁时静默跳过。
- **对比基类 OnHit**：火弹（FireBolt）的命中特效是直接 `SpawnSystemAtLocation`；火球（FireBlast）走 Cue——**同一件事的两种工程化程度**（Cue 带来统一管理/参数化，直接生成更简单直给）。

---

# 三、APointCollection —— 地面点集（★★★☆☆）

## 3.1 结构：10 个"预留锚点"

```cpp
UPROPERTY(BlueprintReadOnly, VisibleAnywhere) TArray<USceneComponent*> ImmutablePts;
UPROPERTY(BlueprintReadOnly, VisibleAnywhere) TObjectPtr<USceneComponent> Pt_0;   // 根
// Pt_1 ~ Pt_9 同款（构造里逐个 CreateDefaultSubobject + Add + SetupAttachment(Pt_0)）
```

- **10 个空 SceneComponent**（Pt_0 为根）：**"摆放点货架"**——编辑器里把整组点摆到目标区域（ArcaneShards 的 BP_PointCollection），运行时逐点贴地。**为什么硬编码 10 个而不是运行时 Create**：编辑器需要**可视化摆放**（运行时生成的点没法在编辑器里看/调）。
- **`ImmutablePts` 命名**：点的**数量/父子关系**不可变（构造定死），**位置**运行时可调（贴地）——名字表达"结构不可变"。

## 3.2 GetGroundPoints —— 贴地取点算法

```cpp
TArray<USceneComponent*> APointCollection::GetGroundPoints(const FVector& GroundLocation, 
    int32 NumPoints, float YawOverride /*= 0.f*/)
{
    checkf(ImmutablePts.Num() >= NumPoints, TEXT("Attempted to access ImmutablePts out of bounds."));

    TArray<USceneComponent*> ArrayCopy;

    // 所有探测点共用同一中心，生存者列表只需查询一次；结果同时作为射线检测的忽略列表
    TArray<AActor*> IgnoreActors;
    UDuraAbilitySystemLibrary::GetLivePlayersWithinRadius(this, IgnoreActors, TArray<AActor*>(), 1500.f, GetActorLocation());

    for (USceneComponent* Pt : ImmutablePts)
    {
        if(ArrayCopy.Num() >= NumPoints) return ArrayCopy;

        if(Pt != Pt_0)
        {
            FVector ToPoint = Pt->GetComponentLocation() - Pt_0->GetComponentLocation();
            ToPoint = ToPoint.RotateAngleAxis(YawOverride, FVector::UpVector);   // 整组绕中心旋转
            Pt->SetWorldLocation(Pt_0->GetComponentLocation() + ToPoint);
        }

        const FVector RaisedLocation = FVector(..., Z + 500.f);
        const FVector LoweredLocation = FVector(..., Z - 500.f);

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActors(IgnoreActors);
        FHitResult HitResult;
        GetWorld()->LineTraceSingleByProfile(HitResult, RaisedLocation, LoweredLocation, FName("BlockAll"), QueryParams);

        // 未命中任何阻挡面时 ImpactPoint/ImpactNormal 无效（会落到 Z=0 和垃圾法线），保持点位不动
        if(HitResult.bBlockingHit)
        {
            const FVector AdjustedLocation = FVector(..., HitResult.ImpactPoint.Z);
            Pt->SetWorldLocation(AdjustedLocation);
            Pt->SetWorldRotation(UKismetMathLibrary::MakeRotFromZ(HitResult.ImpactNormal));
        }

        ArrayCopy.Add(Pt);
    }
    return ArrayCopy;
}
```

- **参数**：`GroundLocation`（中心点——鼠标施法点）、`NumPoints`（要几个点，按技能等级）、`YawOverride`（整组朝向）。
- **忽略列表一鱼两吃**：`GetLivePlayersWithinRadius(1500)`（第 5 篇）拿"中心附近的活人"——**既是忽略者**（射线不打到玩家身上）**也是语义上的"别把碎片放在人头上"**。**一次查询两用**的注释点明。
- **三点贴地算法**：每点从"抬升 500"到"下探 500"打竖直射线 → 命中则 Z 吸附到 ImpactPoint.Z、朝向对齐地面法线（`MakeRotFromZ(法线)`：让 Z 轴=法线——**贴在斜坡上的碎片法线朝向**）。
- **`LineTraceSingleByProfile(命中配置名="BlockAll")`**：**按 Profile 而不是通道**检测——Profile 是碰撞配置的"预设套餐"（Project Settings 里定义），比逐通道设置简洁。
- **防御注释（项目改造点）**：未命中（悬空区域）时 **保持点位不动** 而不是用无效 ImpactPoint（Z=0 会把碎片吸到地底）。**对"检测失败"的默认行为是保留旧值**——与"清缓存"（第 3 篇）相反的另一种正确策略（那里的旧值会误导，这里的旧值无害）。
- **消费场景**：ArcaneShards 蓝图取 N 个点 → 每个点放一枚碎片（延迟爆炸径向伤害）。
- **返回值**：**组件指针数组**（不是位置数组）——蓝图继续读每个点的位置/朝向/自定义信息。

---

# 四、本章全链路总图

```
技能 Spawn（第 6 篇，服务器）
  └─ SpawnActorDeferred → 注入 DamageEffectParams（数值已填/方向待命中）→ FinishSpawning
弹丸飞行（全端，移动复制）
  ├─ BeginPlay：LifeSpan 15s / 循环音挂载 / Overlap 绑定
  └─ MovementComponent 直线推进（无重力）
命中判定（Overlap 回调）
  ├─ IsValidOverlap：来源有效？非自己？非友军？（ActorTag 判断）
  ├─ 本地：OnHit（爆炸特效/停音）+ bHit=true
  └─ 服务器：补死亡冲量（弹道方向）/击退掷骰（45°仰角）/TargetASC → ApplyDamageEffect（第 5 篇）
            → Destroy
销毁同步（★★★）
  └─ 服务器 Destroy → 复制 → 各客户端 Destroyed
       ├─ 停飞行音（必经之路）
       └─ !bHit && 客户端 → 补播 OnHit（超时/无效命中场景的全端表现一致）
火球特化（FireBlast）
  ├─ 穿透：Overlap 只结算不销毁（无击退）
  ├─ 蓝图时间轴：出膛动画 + 归途回收
  └─ 爆炸：ExecuteGameplayCue_NonReplicated（各端本地播，省带宽防重复）
地面技能辅助
  └─ PointCollection::GetGroundPoints：活人忽略列表（一次查询两用）+ 竖直射线贴地 + 法线对齐
```

**面试考点**：
1. **弹丸的网络同步三件套**（bReplicates + SetReplicateMovement + 服务器权威判定/客户端预测表现）
2. **Destroyed 的兜底播放在三种场景下如何工作**（命中/无效命中/超时）
3. **Overlap 与 Block 的选择**（穿透需求→Overlap；阻挡需求→Block；弹丸 QueryOnly+白名单）
4. **GameplayCue Replicated 与 Non-Replicated 的选择依据**

# 五、动手实验建议

1. PIE 双窗（ListenServer+Client）+ `net.PktLag=300`：把 `if (!bHit && !HasAuthority()) OnHit()` 注释掉，向空气发射火弹并等超时销毁——观察客户端没有爆裂特效；恢复后对比。亲手验证 Destroyed 兜底。
2. 给 DuraProjectile 加 `ProjectileMovement->ProjectileGravityScale = 1.0`，观察抛物线弹道与重力对手感的影响。
3. 用 ArcaneShards 在斜坡施法，观察碎片法线对齐；再站到探测区域内（IgnoreActors 生效）确认碎片不打到玩家脚底。

---

*下一篇：`00-学习路线总览.md` 与 `13-面试考点速查手册.md`（收尾篇）。*
