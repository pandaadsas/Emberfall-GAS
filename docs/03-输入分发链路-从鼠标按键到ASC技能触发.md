# 03 · 输入分发链路 —— 从鼠标按键到 ASC 技能触发

> **逻辑链路一句话概括**：
> `增强输入系统捕获原始按键（IMC_DuraContext 映射到 InputAction）` → `PlayerController 的 SetupInputComponent 把每个 InputAction 绑定到三个事件（Started/Completed/Triggered）` → `触发时取出该按键的 InputTag，先查 ASC 身上有没有 Player_Block_* 屏蔽 Tag` → `通过则转发给 ASC->AbilityInputTagPressed/Released/Held(InputTag)` → `ASC 找到绑定了同款 InputTag 的技能并 TryActivate（第 6 篇）`。
> **同一套按键在 LMB 上还有第二重身份：点击移动（Diablo 式操作）**——按下/按住/松开的时间与目标类型决定"移动"还是"施法"。

---

## 📋 本篇必读文件清单（按阅读顺序）

| 顺序 | 文件路径 | 作用 | 重要性 |
|---|---|---|---|
| 1 | `Source/Dura/Public/Input/DuraInputConfig.h` / `.cpp` | InputAction ↔ InputTag 的数据资产映射表 | ★★★★☆ |
| 2 | `Source/Dura/Public/Input/DuraEnhancedInputComponent.h` | 模板化的一键三绑工具 | ★★★☆☆ |
| 3 | `Source/Dura/Public/Player/DuraPlayerController.h` / `.cpp` | 输入总枢纽：绑定、分发、鼠标寻迹、点击移动、自动寻路、伤害数字、魔法阵 | ★★★★★ |

配套资产（蓝图，供对照）：`Content/Blueprints/Input/DA_DuraInputConfig.uasset`（InputConfig 数据资产）、`IMC_DuraContext.uasset`（输入映射上下文）。

---

# 一、铺垫：Enhanced Input 三件套（引擎层知识）

| 概念 | 是什么 | 本项目对应 |
|---|---|---|
| **InputAction（IA）** | 一个"语义按键"：IA_Move（2D 轴）、IA_LMB（数字按键）……只关心"发生了什么"，不关心"哪个键" | 编辑器资产 IA_LMB、IA_RMB、IA_Move、IA_Shift 等 |
| **InputMappingContext（IMC）** | 映射表：物理键 → InputAction（可带修饰器 Modifier 与触发器 Trigger） | `IMC_DuraContext`：鼠标左键→IA_LMB，1 键→IA_1…… |
| **TriggerEvent** | 输入生命周期：`Started`（刚按下）、`Triggered`（持续按住期间每帧）、`Completed`（松开） | Held 用 Triggered，Pressed 用 Started，Released 用 Completed |

- **为什么用 Enhanced Input 而不是老 Action/Axis**：类型化值（FVector2D）、运行时换映射（换手柄/改键）、按玩家分 Context。库洛的开放世界项目必然是 Enhanced Input，面试要熟。
- **本项目的独特设计**：IA 本身不带逻辑，**IA 的"身份"由 InputTag 表达**。绑定回调时把 Tag 一起捕获进 Lambda，触发时带着 Tag 问 ASC"有没有技能认领这个 Tag"。输入系统完全不认识技能系统。

---

# 二、UDuraInputConfig —— 键位到 Tag 的"翻译字典" ★★★★☆

## 2.1 数据结构

```cpp
USTRUCT(BlueprintType)
struct FDuraInputAction
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    const class UInputAction* InputAction = nullptr;

    UPROPERTY(EditDefaultsOnly)
    FGameplayTag InputTag = FGameplayTag();
};

UCLASS()
class UDuraInputConfig : public UDataAsset
{
public:
    const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FDuraInputAction> AbilityInputActions;
};
```

- **`UDataAsset`**：纯数据资产类（UObject 子类，可存盘、可被引用）。选择 DataAsset 而非"硬编码数组"的原因：**键位表是策划数据**——新增 5 号键技能 = 在 DA_DuraInputConfig 里加一行（IA + InputTag.5），C++ 零修改。
- **`const class UInputAction*`**：UPROPERTY 里的 const 裸指针——只读引用（防运行时改指向），`class` 前置声明解决头文件循环。UHT 支持这种写法且编辑器显示资产选择器。
- **`InputTag` 用 `==` 比较**：`FGameplayTag::operator==` 比较的是 Tag 节点（等价于 MatchesTagExact），不是字符串。
- **`bLogNotFound = false` 默认参数**：查找失败默认静默（有些查询本来就允许失败，如探路），调用方确定"必须有"时传 true 报错。**默认参数在接口函数里的语义是"我预见两种调用场景"**。

## 2.2 查找函数

```cpp
const UInputAction* UDuraInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
    for (const FDuraInputAction& Action : AbilityInputActions)
    {
        if (Action.InputAction && Action.InputTag == InputTag)
        {
            return Action.InputAction;
        }
    }
    if (bLogNotFound)
    {
        UE_LOG(LogDura, Error, TEXT("Can't find AbilityInputAction for InputTag [%s], on InputConfig [%s]"), ...);
    }
    return nullptr;
}
```

- 线性遍历 8 项——量小不值得建 TMap；`Action.InputAction &&` 先判空（配置了 Tag 没配 IA 的脏行跳过）。
- **日志的三个占位信息**：Tag 名 + 配置资产名（GetNameSafe(this) 安全版 GetPathName，空安全）——报错信息能直接定位到"哪个资产的哪一行没配"，**报错信息的可用性是工程素养**。
- **谁消费它**：PlayerController 的 SetupInputComponent（正向：IA→回调）与 SpellMenuWidgetController（反向：Tag→IA，给技能图标找按键提示）。一张表双向服务。

## 2.3 对比拓展：Lyra 的做法

Lyra（Epic 官方示例）用同样的 "InputConfig + Tag" 模式但绑定时用模板函数把成员函数指针传进去。本项目第 3 节的 `BindAbilityActions` 就是这个思路的简化版——**学会这个模式，看 Lyra 源码就没有门槛**。

---

# 三、UDuraEnhancedInputComponent —— 一键三绑的模板工具 ★★★☆☆

## 3.1 为什么它存在

每个技能按键都要绑 **三个事件**（Pressed/Released/Held），写三遍 `BindAction` 太啰嗦。这个子类提供模板函数一次绑齐：

```cpp
template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
void UDuraEnhancedInputComponent::BindAbilityActions(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
    BindActionValueLambda(Action, ETriggerEvent::Started,   [Object, PressedFunc](const FInputActionValue&) { (Object->*PressedFunc)(); });
    BindActionValueLambda(Action, ETriggerEvent::Completed, [Object, ReleasedFunc](const FInputActionValue&) { (Object->*ReleasedFunc)(); });
    BindActionValueLambda(Action, ETriggerEvent::Triggered, [Object, HeldFunc](const FInputActionValue&)    { (Object->*HeldFunc)(); });
}
```

- **逐个参数**：`Action`（要绑的 IA）；`TriggerEvent` 参数**实际上没用到**（函数内部写死了 Started/Completed/Triggered 三个事件——参数冗余，这是代码可挑剔点）；`Object`（回调宿主）；三个函数指针（成员函数指针类型，如 `&ADuraPlayerController::AbilityInputTagPressed`）。
- **`(Object->*PressedFunc)()`**：**成员函数指针调用语法**——`->*` 运算符。C++ 里成员函数指针不是普通指针，必须带对象调用。面试语言考点。
- **模板而不是 std::function**：编译期内联展开，零运行时开销；UE 引擎源码大量用这种"模板+成员指针"绑定风格（如 `BindAction` 本体）。
- **本项目实际没调用它**：PlayerController 里直接用了 `BindActionValueLambda`（因为需要捕获每个 Tag 到 Lambda，而不是绑到固定成员函数）。**工具在库里、按需取用**——但它展示了"如何封装重复绑定"，面试讲封装时是好例子。
- `BindActionValueLambda(Action, Event, Lambda)`：EnhancedInputComponent 的官方 API，给 IA 的某事件绑一个 Lambda（捕获 Tag 的唯一途径）。

---

# 四、DuraPlayerController —— 输入总枢纽 ★★★★★

## 4.1 构造函数与 TargetingStatus

```cpp
ADuraPlayerController::ADuraPlayerController()
{
    bReplicates = true;
    Spline = CreateDefaultSubobject<USplineComponent>("Spline");
}
```

- **`bReplicates = true`**：PlayerController 只复制给**拥有者客户端**（引擎规则：PlayerController 不广播给别人）。Replication 打开是为了 `ShowDamageNumber` 这个 Client RPC。
- **`USplineComponent`**：样条组件，存"点击移动"的路径点。挂 PC 而不是 Pawn：PC 在换 Pawn 后仍在（跨重生存活），且路径是"玩家意图"不是"身体属性"。

```cpp
enum class ETargetingStatus : uint8
{
    TargetingEnemy,
    TargetingNotEnemy,
    NotTargeting
};
```

- 三态枚举（强类型 enum class + 显式底层类型 uint8）：**当前鼠标语义**——点着敌人（战斗模式）、点着地面（移动模式）、没在点击。它是 LMB 分支决策的状态机变量。

## 4.2 BeginPlay —— 装上键位映射与光标模式

```cpp
void ADuraPlayerController::BeginPlay()
{
    Super::BeginPlay();
    check(DuraContext);

    UEnhancedInputLocalPlayerSubsystem* SubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
    if (SubSystem)
    {
        SubSystem->AddMappingContext(DuraContext, 0);
    }

    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;

    FInputModeGameAndUI Inputmode;
    Inputmode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Inputmode.SetHideCursorDuringCapture(false);
    SetInputMode(Inputmode);
}
```

- **AddMappingContext(Context, Priority=0)**：把 IMC 注册进**本地玩家的 EnhancedInput 子系统**。第二参是优先级（大的覆盖小的同名映射）。**为什么在 PC 的 BeginPlay**：LocalPlayer 在 PC 创建后才存在，且 PC 是"每个本地玩家一份"的正确挂点。
- **`check(DuraContext)`**：蓝图忘了配 IMC 直接崩在第一帧——配置错误要炸得早、炸得响。
- **`if (SubSystem)`**：子系统在极端情况（非本地回放）可能为空，判空防崩——与上一行的 check 形成对比：**"资产配置错误"用 check，"引擎状态可能合法缺失"用判空**。
- **InputMode 三选一**：`GameOnly`（FPS）/ `UIOnly`（纯菜单）/ `GameAndUI`（鼠标可见、游戏也响应——俯视角点击游戏的标准选择）。`DoNotLock` 不锁鼠标到视口（窗口化时鼠标能出窗口），`SetHideCursorDuringCapture(false)` 点击时不藏光标（鼠标是主要交互工具，藏了就瞎了）。

## 4.3 SetupInputComponent —— 全部绑定发生在这里

```cpp
void ADuraPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    check(InputConfig);
    UDuraEnhancedInputComponent* DurainputComponent = CastChecked<UDuraEnhancedInputComponent>(InputComponent);
    check(MoveAction);
    check(ShiftAction);

    DurainputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, Move));
    DurainputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, ShiftPressed));
    DurainputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, GET_FUNCTION_NAME_CHECKED(ADuraPlayerController, ShiftReleased));

    for (const FDuraInputAction& Action : InputConfig->AbilityInputActions)
    {
        if (Action.InputAction && Action.InputTag.IsValid())
        {
            FGameplayTag Tag = Action.InputTag;
            DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Started,   
                [this, Tag](const FInputActionValue& ActionValue) { AbilityInputTagPressed(Tag); });
            DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Completed, 
                [this, Tag](const FInputActionValue& ActionValue) { AbilityInputTagReleased(Tag); });
            DurainputComponent->BindActionValueLambda(Action.InputAction, ETriggerEvent::Triggered, 
                [this, Tag](const FInputActionValue& ActionValue) { AbilityInputTagHeld(Tag); });
        }
    }
}
```

- **`SetupInputComponent` vs BeginPlay**：输入绑定必须在 `SetupInputComponent`（引擎在 InputComponent 就绪后调用；BeginPlay 时 InputComponent 可能未就绪）。**顺序**：引擎构造 InputComponent → SetupInputComponent → BeginPlay。
- **`CastChecked<UDuraEnhancedInputComponent>(InputComponent)`**：**APawn/PlayerController 的 InputComponent 类型由项目设置决定**——`DefaultEngine.ini` 的 `+ClassMappings=(ClassName="EnhancedPlayerInput",...)` 与 Pawn 的 `PlayerInputClass`。**为什么必须 Cast 到子类**：要用子类的 `BindActionValueLambda`。背后机制：EnhancedInput 项目模板在 ini 里把默认 InputComponent 类换成 `UEnhancedInputComponent`，这里再换成我们的子类。
- **`GET_FUNCTION_NAME_CHECKED(类, 函数)`**：编译期检查函数存在并生成 FName 的宏——比手写 `FName("Move")` 多了拼写检查（改名编译报错）。
- **Lambda 捕获 Tag 的关键细节**：`FGameplayTag Tag = Action.InputTag;` **先拷贝到局部变量再捕获**！如果直接 `[this, Action.InputTag]` 捕获（C++14/17 捕获成员的语法糖实际捕获 this），循环变量生命周期结束后 Lambda 里访问的是**悬垂的循环变量引用**——这是 C++ 循环绑 Lambda 的经典死亡陷阱（UB）。**先拷贝再按值捕获是唯一正确写法**。★面试高分点。
- **为什么 Move 用成员函数绑定而技能用 Lambda**：Move 只有一个（直接 `BindAction` 即可）；技能按键是循环出的 N 个（必须 Lambda 捕获各自 Tag）。
- **Shift 绑定两个事件**：Shift 按下/松开只是翻一个 bool 标志（`bShiftKeyDown`），供 LMB 分支判断"按住 Shift 时左键 = 强制施法而非移动"。

## 4.4 PlayerTick —— 每帧三件事

```cpp
void ADuraPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    MouseTrace();            // ① 光标寻迹：高亮敌人 + 缓存命中信息
    AutoRun();               // ② 自动寻路推进
    UpdateMagicCircleLocation();  // ③ 魔法阵跟随光标
}
```

- PC 的 Tick 独立于 Pawn 的 Tick（PC 永远存在、每帧必跑）。**为什么寻迹放 PC 不放 Pawn**：光标是"玩家视角"概念，与身体无关；且敌人也要被高亮（光标点谁谁亮，不必是玩家视野内的判断）。

## 4.5 MouseTrace —— 光标寻迹与高亮（★★★★☆）

```cpp
void ADuraPlayerController::MouseTrace()
{
    // 眩晕等状态通过 Player_Block_CursorTrace 屏蔽鼠标寻迹；同时清空过期缓存
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_CursorTrace))
    {
        UnHighlightActor(lastActor);
        lastActor = nullptr;
        thisActor = nullptr;
        hitResult = FHitResult();
        return;
    }

    FHitResult CursorHit;
    GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);
    if (!CursorHit.bBlockingHit)
    {
        UnHighlightActor(lastActor);
        lastActor = nullptr;
        thisActor = nullptr;
        return;
    }
    hitResult = CursorHit;
    thisActor = CursorHit.GetActor();
    
    if (IsValid(thisActor) && thisActor->Implements<UHighlightInterface>())
    {
        HighlightActor(thisActor);
    }
    else
    {
        UnHighlightActor(thisActor);
    }
    
    if (lastActor != thisActor)
    {
        UnHighlightActor(lastActor);
        lastActor = thisActor;
    }
}
```

逐块精讲：

- **`GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)`**：
  - 从光标屏幕位置反投影一条射线做 Trace。参数1 通道（Visibility——敌人 Mesh 特意对该通道设 Block，见第 2 篇）；参数2 `bTraceComplex=false`（用简化碰撞体而不是逐三角形，性能优先，检测"这个敌人"不需要精确到像素）；参数3 输出命中结果。
  - 返回 bool，这里忽略返回值改查 `CursorHit.bBlockingHit`（有 blocking 命中才有效）。
- **屏蔽 Tag 消费点**：眩晕时（技能给 ASC 加了 `Player.Block.CursorTrace` Tag）整条寻迹短路，并**清理缓存**（旧高亮取消、lastActor/thisActor 置空、hitResult 重置）——不清理的话施法结束后会残留旧高亮/旧目标。**屏蔽一个系统时要把它的输出状态也清干净**，防"幽灵状态"。
- **高亮状态机（双 Actor 缓存）**：`lastActor`（上一帧命中者）与 `thisActor`（本帧命中者）。逻辑：
  - 本帧命中者实现高亮接口 → 点亮它；
  - 不实现（点到了地板/空气以外的普通物体）→ 显式取消它（防御：确保状态归零）；
  - **lastActor != thisActor**（光标移开了）→ 熄灭旧的、更新缓存。
  - **为什么需要 lastActor 缓存**：高亮是"点亮/熄灭"成对操作，光标离开时**没人会再通知你熄灭旧目标**——必须自己记住上次点亮了谁。漏写 lastActor 逻辑 = 满屏常亮描边（新手常见 bug）。
- **`Implements<UHighlightInterface>()`**：注意模板参数是 `U` 开头的 **UHighlightInterface**（反射类）而不是 `I` 开头的接口类——`Implements<>` 查的是 UClass 的接口实现表。
- **性能考量**：每帧一次 LineTrace 很便宜；每帧调 SetRenderCustomDepth 切换有轻微开销，但引擎内部有状态判断（相同值不 dirty）。

## 4.6 Move —— WASD 移动

```cpp
void ADuraPlayerController::Move(const FInputActionValue& InputValue)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
        return;

    if(bAutoRunning)
    {
        bAutoRunning = false;
        TargetingStatus = ETargetingStatus::NotTargeting;
    }

    FVector2d AxisValue = InputValue.Get<FVector2d>();

    FRotator CtlRotation = GetControlRotation();
    CtlRotation.Pitch = 0.0f;
    CtlRotation.Roll = 0.0f;

    FVector ForwardVector = FRotationMatrix(CtlRotation).GetUnitAxis(EAxis::X);
    FVector RightVector = FRotationMatrix(CtlRotation).GetUnitAxis(EAxis::Y);

    if (APawn* ControlledPawn = GetPawn<APawn>())
    {
        ControlledPawn->AddMovementInput(ForwardVector, AxisValue.Y);
        ControlledPawn->AddMovementInput(RightVector, AxisValue.X);
    }
}
```

- **屏蔽检查**：眩晕/施法锁定时（ASC 有 BlockTag）WASD 无效——**移动也是技能管辖权的一部分**。
- **打断自动寻路**：玩家手动按方向键 = 取消寻路意图（bAutoRunning=false），否则两套移动逻辑打架。
- **`InputValue.Get<FVector2D>()`**：增强输入的类型化值。IA_Move 配置为 Axis2D（W=+Y，S=-Y，D=+X，A=-X）——**方向语义编码在值里**。
- **控制旋转取基向量**：`GetControlRotation()` 是相机朝向（俯视角下固定朝下看）；**把 Pitch/Roll 归零**保证基向量是水平的（俯视角相机 Pitch 约 -60°，不归零的话 W 会带垂直分量）；`FRotationMatrix(Rot).GetUnitAxis(EAxis::X/Y)` 取旋转后的前/右单位向量——**不用 GetForwardVector()**（那是 Rotator 的便捷函数，等价，这里用矩阵写法更显式）。
- **`AddMovementInput(方向, 缩放)`**：把"输入意图"喂给**角色的** MovementComponent（CharacterMovement 汇总所有输入帧末统一算速度——输入与移动解耦，敌人 AI 也用同一套）。
- **为什么归零用 Pitch/Roll 而 Yaw 保留**：俯视角下相机 Yaw 固定朝北，"W=屏幕上方"依赖这个 Yaw。

## 4.7 LMB 三兄弟：Pressed / Held / Released（★★★★★ 本篇核心）

这三个函数是"**点击移动 + 指向施法**"的双模式决策树。先给决策表：

| 事件 | 指向敌人 或 按住Shift | 指向地面（短按/长按） |
|---|---|---|
| **Started（Pressed）** | 转发 Tag 给 ASC（施法） | 缓存目的地；若点中敌人标记 TargetingEnemy；**仍然转发给 ASC** |
| **Triggered（Held，每帧）** | 转发 Tag 给 ASC（持续施法） | **累计 FollowTime** + **朝 CachedDestination 方向 AddMovementInput**（拖着角色走） |
| **Completed（Released）** | 转发 Tag 给 ASC | 短按（FollowTime≤0.5s）→ **寻路 + Spline + 自动跑**；重置 FollowTime/TargetingStatus |

### 4.7.1 AbilityInputTagPressed

```cpp
void ADuraPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
        return;

    if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
    {
        if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
        return;
    }

    if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
    {
        if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
    }
    else
    {
        const APawn* ControlledPawn = GetPawn();
        if (FollowTime <= ShortPressThreshold && ControlledPawn)
        {
            if (hitResult.bBlockingHit)  CachedDestination = hitResult.ImpactPoint;

            if (IsValid(thisActor) && thisActor->Implements<UEnemyInterface>())
            {
                TargetingStatus = ETargetingStatus::TargetingEnemy;
                bAutoRunning = false;
            }
        }

        if(GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
    }
}
```

- **第一个 if（屏蔽）**：`Player.Block.InputPressed` 是技能执行期间给 ASC 加的 Loose Tag（比如按住 LMB 拖动瞄准时，不想让连按触发新技能）。
- **第二个 if（LMB 特判）**：只有 LMB 有"移动/施法"二义性；RMB/数字键直接无脑转发。`MatchesTagExact`：InputTag.LMB 精确匹配。
- **TargetingEnemy 分支**：上一帧已经点着敌人（连续点击同一敌人）或 Shift 强制 → 纯施法路径。
- **else 分支（首次点击地面附近）**：`FollowTime <= ShortPressThreshold` 保证"快速连点敌人"能进入目标标记（点太久的地面点击不该标记敌人）；缓存命中点、若点中的是敌人（**IEnemyInterface**——判定"这个 Actor 是敌人"，敌人实现、玩家不实现）则切换状态机。
- **注意最后一行仍然转发**：点地面**也**会触发技能（比如 FireBolt 向点击方向发射）——**移动与施法并行**，这是 Diablo 类游戏的操作精髓：点地面 = 边移动边朝那施法。
- **为什么 Pressed 里不开始移动、移动在 Held**：Held 每帧执行"朝 CachedDestination 挤一下"——按住才走，符合"按住拖动"手感。

### 4.7.2 AbilityInputTagHeld

```cpp
void ADuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputHeld))
        return;

    if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
    {
        if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
        return;
    }

    if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
    {
        if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
    }
    else
    {
        FollowTime += GetWorld()->GetDeltaSeconds();
        
        if (hitResult.bBlockingHit)  CachedDestination = hitResult.ImpactPoint;

        if (APawn* ControlledPawn = GetPawn())
        {
            const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
            ControlledPawn->AddMovementInput(WorldDirection);
        }
    }
}
```

- `FollowTime += DeltaSeconds`：**按住时长累计**——Released 里判断短按/长按的依据。
- **拖动移动**：每帧把"角色→光标落点"的方向喂给 MovementComponent，角色跟着鼠标走（光线射到哪走到哪），松手进寻路。

### 4.7.3 AbilityInputTagReleased（含寻路与自动跑）

```cpp
void ADuraPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
    if(GetASC() && GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputReleased))
        return;

    if (!InputTag.MatchesTagExact(FDuraGameplayTags::Get().InputTag_LMB))
    {
        if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
        return;
    }

    if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);   // LMB 的施法转发（无条件的）

    if (TargetingStatus != ETargetingStatus::TargetingEnemy && !bShiftKeyDown)
    {
        const APawn* ControlledPawn = GetPawn();
        if (FollowTime <= ShortPressThreshold && ControlledPawn)
        {
            if(IsValid(thisActor) && thisActor->Implements<UHighlightInterface>())
            {
                IHighlightInterface::Execute_SetMoveToLocation(thisActor, CachedDestination);
            }
            else if(GetASC() && !GetASC()->HasMatchingGameplayTag(FDuraGameplayTags::Get().Player_Block_InputPressed))
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ClickNiagaraSystem, CachedDestination);
            }

            if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ControlledPawn->GetActorLocation(), CachedDestination))
            {
                Spline->ClearSplinePoints();
                if (NavPath->PathPoints.Num() != 0)
                {
                    for (const FVector& point : NavPath->PathPoints)
                    {
                        Spline->AddSplinePoint(point, ESplineCoordinateSpace::World);
                    }
                    if (NavPath->PathPoints.Num() > 0)
                    {
                        CachedDestination = NavPath->PathPoints[NavPath->PathPoints.Num() - 1];
                        bAutoRunning = true;
                    }
                }
            }        
        }
        FollowTime = 0.0f;
        TargetingStatus = ETargetingStatus::NotTargeting;
    }
}
```

- **无条件转发在目标分支之前**：松开 LMB **总是**触发技能释放（点地面也施法），移动逻辑只在"非目标模式"追加。
- **SetMoveToLocation（接口微调目的地）**：点到宝箱/门口时，由该 Actor 自己修正"移动到哪"（比如宝箱前的交互位）。**多态注入寻路目标**——不同可交互物知道"该走到它的哪个方位"。
- **点击特效**：落地光圈 Niagara。`else if` 里检查 InputPressed 屏蔽 Tag——施法锁定期不播点击特效（避免特效与施法表现冲突）。
- **`FindPathToLocationSynchronously(this, 起点, 终点)`**：
  - **导航系统（NavMesh）同步寻路**：一次性算出路径数组。`Synchronously` 版本在调用线程直接算完（卡当前帧几毫秒）；异步版 `FindPathToLocationAsync` 走回调。点击寻路一次一条，同步版简单够用。
  - 第一个参数是 WorldContextObject（PC 自身）。
- **Spline 填充**：路径点逐个加进样条（世界坐标）。`CachedDestination = NavPath->PathPoints.Last()`——**目的地修正为路径最后一个点**（导航网格可能走不到光标精确位置，走得到的最远点是权威终点）。
- **`bAutoRunning = true`**：交给 AutoRun 每帧推进。**为什么有了 Held 拖动还要寻路**：拖动是"粗跟"（直线），寻路是"精跟"（绕障碍）。短按 = 需要绕路的长途 = 寻路；长按拖动 = 即时跟手 = 直线挤。

### 4.7.4 AutoRun

```cpp
void ADuraPlayerController::AutoRun()
{
    if (bAutoRunning == false) return;

    if (APawn* ControlledPawn = GetPawn())
    {
        const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);
        const FVector Direction = Spline->FindDirectionClosestToWorldLocation(LocationOnSpline, ESplineCoordinateSpace::World);
        ControlledPawn->AddMovementInput(Direction);

        const float DistanceToDestination = (LocationOnSpline - CachedDestination).Length();
        if (DistanceToDestination <= AutoRunAcceptanceRadius)  
            bAutoRunning = false;
    }
}
```

- **样条跟随三步**：
  1. `FindLocationClosestToWorldLocation(角色位置)`：样条上**离角色最近的点**（角色可能偏离路径，取投影点）。
  2. `FindDirectionClosestToWorldLocation(投影点)`：样条在该点的**切线方向**（沿路径前进的方向）。
  3. `AddMovementInput(切线方向)`：朝切线挤——**每帧重算投影+方向**，角色自然沿曲线拐弯。
- **到达判定**：投影点与终点距离 ≤ `AutoRunAcceptanceRadius`（50cm）就停。**为什么用投影点而不是角色位置**：角色可能在路径旁边擦过终点，投影点更代表"路径进度"。
- **为什么用样条而不是每帧重寻路**：寻路贵，样条是"一次算好、每帧便宜地跟"；`AddMovementInput` 底层仍有 CharacterMovement 的避障兜底。
- **面试考点**：*"实现点击移动要哪些系统？"* —— 光标射线 → 导航寻路 → 样条跟随 → 到达判定 → 输入打断。本项目五件全齐，是完整教学样本。

## 4.8 GetASC —— 惰性缓存的单例查找

```cpp
UDuraAbilitySystemComponent* ADuraPlayerController::GetASC()
{
    if (DuraAbilitySystemComponent == nullptr)
    {
        DuraAbilitySystemComponent = 
            Cast<UDuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
    }
    return DuraAbilitySystemComponent;
}
```

- **`UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)`**：引擎便捷函数——内部对 Actor `Cast<IAbilitySystemInterface>` 再调 GetAbilitySystemComponent。**跨 pawn 身份**（玩家 ASC 在 PlayerState，敌人 ASC 在自身）这函数都能拿对——因为两种实现都返回"正确的那一个"。
- **惰性初始化 + 永不失效的隐患**：缓存后不再更新。玩家死亡 → Pawn 销毁 → 新 Pawn 拥有**同一个** PlayerState → 同一个 ASC——所以缓存仍然有效（ASC 生命周期长于 Pawn，这正是 ASC 放 PlayerState 的红利）。若 ASC 在 Pawn 上，这种缓存就是悬垂指针炸弹。
- 注意返回 nullptr 的可能（Pawn 未 Possess 时）——所有调用处都有 `GetASC() &&` 守卫。

## 4.9 ShowDamageNumber —— 伤害数字的 Client RPC

```cpp
UFUNCTION(Client, Reliable)
void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter, bool bBlockedHit, bool bCriticalHit);

void ADuraPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter, 
    bool bBlockedHit, bool bCriticalHit)
{
    if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
    {
        UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
        DamageText->RegisterComponent();
        DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        DamageText->SetDamageText(DamageAmount, bBlockedHit, bCriticalHit);
    }
}
```

- **`UFUNCTION(Client, Reliable)`**：**服务器调用、拥有者客户端执行**的 RPC。伤害数字是"本地 UI 表现"，别的客户端不需要（各自服务器会给他们发自己的）。Reliable：伤害不能丢。
- **实现四步**：
  1. **`NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass)`**：**运行时动态创建组件**（不是 CreateDefaultSubobject——那是构造函数专用）。Outer=TargetCharacter（组件归受击者所有）。类来自可配置的 `DamageTextComponentClass`（蓝图里换成自定义数字样式）。
  2. **`RegisterComponent()`**：动态组件必须注册才激活（走 BeginPlay/渲染注册）。
  3. **Attach 又立刻 Detach（KeepWorldTransform）**：先挂到目标根下再马上脱离——**这是"借用坐标系"的技巧**：Attach 的瞬间组件获得目标的**世界变换**，立刻 KeepWorld 脱离后，组件冻在"目标头顶出生点"那个世界位置。之后组件自己浮动上升（组件内部逻辑）。为什么不直接 SetWorldLocation？Attach+Detach 保证拿到的变换与目标骨骼/胶囊当前姿态完全一致，一行顶两行。
  4. `SetDamageText(...)`：传入数值与暴击/格挡标志（**这些标志从哪来？** 服务器 ExecCalc 写进 GE Context，PostGameplayEffectExecute 读出来再调这个 RPC——第 5 篇汇合）。
- **`IsLocalController()`**：双保险——RPC 只发拥有者，但防 PIE 分屏等边缘情况。
- **组件生命周期**：DamageTextComponent 播完动画自杀（第 8 篇）。

## 4.10 魔法阵（ShowMagicCircle / HideMagicCircle / UpdateMagicCircleLocation）

```cpp
void ADuraPlayerController::ShowMagicCircle(UMaterialInterface* DecalMaterial)
{
    if(!IsValid(MagicCircle))
    {
        MagicCircle = GetWorld()->SpawnActor<AMagicCircle>(MagicCircleClass);
        if(IsValid(MagicCircle) && DecalMaterial)
        {
            MagicCircle->SetMaterial(0, DecalMaterial);
        }
    }
}

void ADuraPlayerController::UpdateMagicCircleLocation()
{
    if(IsValid(MagicCircle))
    {
        MagicCircle->SetActorLocation(hitResult.ImpactPoint);
    }
}
```

- **单例式生成**：`if(!IsValid(MagicCircle))` 保证只有一个魔法阵（连点不改数量）。`MagicCircleClass` 空时的崩溃被 IsValid 守卫（配合注释"生成失败时跳过材质设置"）。
- **贴花换材质**：`SetMaterial(0, DecalMaterial)`——同一个 MagicCircle Actor，ArcaneShards 传入自己的紫色贴花。**资源注入式复用**：一个地面标记 Actor 服务所有需要地面瞄准的技能。
- **每帧跟随 `hitResult.ImpactPoint`**：MouseTrace 缓存的命中点就是魔法阵落点（技能用它做目标选择——TargetDataUnderMouse 拿的就是同一个点，第 6 篇汇合）。
- **消费方**：`ADuraCharacter::ShowMagicCircle_Implementation`（PlayerInterface）转调到这里 + `bShowMouseCursor = false`（魔法阵**替代**光标成为瞄准指针）。

---

# 五、本章全链路总图

```
物理按键（鼠标左键）
   │ IMC_DuraContext 映射
   ▼
IA_LMB ── EnhancedInput 子系统 ──► ETriggerEvent::Started/Triggered/Completed
   │                                │
   │                    UDuraEnhancedInputComponent::BindActionValueLambda
   │                    （SetupInputComponent 循环绑定，Tag 捕获进 Lambda）
   ▼                                ▼
AbilityInputTagPressed/Held/Released(InputTag)
   │
   ├─ ① ASC 有 Player_Block_* Tag？── 是 → 丢弃（眩晕/施法锁）
   ├─ ② 非 LMB？── 是 → 直接 ASC->AbilityInputTag*（纯技能键）
   └─ ③ LMB：TargetingEnemy 或 Shift？
          ├─ 是 → ASC->AbilityInputTag*（施法）
          └─ 否 → 移动模式：Held 拖动；Released 短按 → NavMesh 寻路 → Spline → AutoRun
                                          └→ 点中可交互物 → SetMoveToLocation 修正目的地

每帧独立循环：MouseTrace（高亮+缓存 hitResult）→ AutoRun → UpdateMagicCircleLocation
```

# 六、动手实验建议

1. 把 Lambda 捕获改成 `[this, Action.InputTag]`（C++17 下这捕获的是 this！），故意不拷贝局部变量——在多技能绑定下观察 Released 回调里 Tag 串位（运行不稳定或 UB），再改回来。体会"循环里捕获循环变量"。
2. 在 `MouseTrace` 里注释掉 `UnHighlightActor(lastActor)`，把光标从敌人快速移开——观察描边残留，理解 lastActor 缓存的必要性。
3. 把 `FindPathToLocationSynchronously` 换成异步版（`FindPathToLocationAsync`），重构 Released 逻辑为回调式，体会同步/异步寻路在"按下-响应"模型里的差异。

---

*下一篇：`04-属性系统全链路.md` —— AttributeSet 的属性声明宏、复制、PostGameplayEffectExecute、 Clamp 顺序与 MMC 派生。*
