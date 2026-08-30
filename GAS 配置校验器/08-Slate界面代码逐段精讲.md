# 08 - Slate 界面代码逐段精讲

> 「原理精讲」第 3 篇。把 `SGasValidatorPanel.h/.cpp` 和 `DuraEditor.cpp` 的界面代码**按段拆开讲**。读完后你应该能自己用 Slate 搭一个带表格的工具面板。

---

## 1. Slate 的世界观：控件树 + 声明式嵌套

Slate 程序 = 一棵控件树。你用 `SNew(控件类型).属性(值)[ 子控件 ]` 的语法把树"写"出来：

```cpp
SNew(SVerticalBox)                          // 根：垂直布局
+ SVerticalBox::Slot().AutoHeight()         // 一个槽位，高度按内容
[
    SNew(SButton).Text(...)                 // 槽位里放一个按钮
]
+ SVerticalBox::Slot().FillHeight(1.f)      // 另一个槽位，占满剩余高度
[
    SNew(SListView<...>)...                 // 槽位里放列表
]
```

读 Slate 代码的口诀：**`SNew` 开头是造控件，`.xxx()` 是设置属性，`[ ]` 里是孩子，`+ Slot()` 是往容器里放**。

对照：UMG 里你在 Designer 拖一个 VerticalBox 再拖两个子控件；Slate 里同样的动作变成写代码。**Slate 没有 Designer，但表达力更强，且这是编辑器唯一可用的 UI 框架。**

## 2. 面板骨架：Construct() 四段式

`SGasValidatorPanel::Construct()` 就是那张界面图的代码版：

### 第一段：初始化数据

```cpp
FilterOptions = {
    MakeShared<FString>(TEXT("全部")),
    MakeShared<FString>(TEXT("错误+警告")),
    MakeShared<FString>(TEXT("仅错误"))
};
SelectedFilter = FilterOptions[0];
```

SComboBox 的选项类型要求是"可空指针"，所以用 `TSharedPtr<FString>` 包一层字符串——这也是 05 篇里编译错误的根源（`SComboBox<FString>` 直接编译不过）。

### 第二段：订阅子系统

```cpp
if (GEditor)
{
    if (UGasValidatorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGasValidatorSubsystem>())
    {
        CachedSubsystem = Subsystem;
        ValidationDelegateHandle = Subsystem->OnValidationCompleted.AddRaw(
            this, &SGasValidatorPanel::HandleValidationCompleted);

        // 已有结果就直接显示（面板关了重开的场景）
        if (Subsystem->GetLastResult().ScannedAssetCount > 0)
        {
            HandleValidationCompleted(Subsystem->GetLastResult());
        }
    }
}
```

三个细节：
- `AddRaw(this, ...)`：裸指针订阅。**为什么敢用 raw？** 因为析构函数里精确退订（`Remove(ValidationDelegateHandle)`）。订阅/退订必须成对，否则子系统广播时面板可能已经销毁 → 崩溃。
- `TWeakObjectPtr<UGasValidatorSubsystem> CachedSubsystem`：弱引用持有子系统，用前 `Get()` 判空。子系统由 GEditor 管理生命周期，面板绝不拥有它。
- 第二个 if：**打开面板时若已有历史结果，立即回放**。这就是为什么面板关了再开，上次扫描结果还在。

### 第三段：搭工具行 + 统计行

```cpp
SNew(SHorizontalBox)
+ SHorizontalBox::Slot().AutoWidth()
[
    SNew(SButton)
    .Text(FText::FromString(TEXT("开始扫描")))
    .OnClicked(this, &SGasValidatorPanel::OnScanClicked)   // 点击回调
]
+ SHorizontalBox::Slot().FillWidth(1.f)
[
    SNew(SSpacer)          // 弹簧：把右边的控件推到最右
]
+ SHorizontalBox::Slot().AutoWidth()
[
    SAssignNew(FilterCombo, SComboBox<TSharedPtr<FString>>)   // SAssignNew = 造的同时留个指针
    ...
]
```

`SAssignNew(变量, 类型)`：造控件的同时把共享指针存进成员变量，以后能主动操作它（比如刷新下拉框）。统计行同理：

```cpp
SAssignNew(StatsText, STextBlock)
.Text(FText::FromString(TEXT("尚未扫描。点击「开始扫描」，检查技能配置。")))
```

### 第四段：结果表格（本面板最复杂的部分）

```cpp
SAssignNew(ListView, SListView<FGasValidationRowItemPtr>)
.ListItemsSource(&FilteredRows)                          // 数据源（数组的指针！）
.OnGenerateRow(this, &SGasValidatorPanel::OnGenerateRow) // 每行怎么造
.OnMouseButtonDoubleClick(this, &SGasValidatorPanel::OnRowDoubleClicked)
.SelectionMode(ESelectionMode::Single)
.HeaderRow(
    SNew(SHeaderRow)
    + SHeaderRow::Column(GAS_COL_Severity).FillWidth(0.8f)[SNew(STextBlock).Text(...)]
    + SHeaderRow::Column(GAS_COL_Category).FillWidth(1.1f)[...]
    ...)
```

**SListView 的三要素**：
1. **数据源**：`TArray<TSharedPtr<行数据>>` 的地址（注意是指针，数组换内容后要 `RequestListRefresh()`）；
2. **行生成器**：数据 → 行控件；
3. **交互回调**：双击/选择/右键菜单……

`FGasValidationRowItemPtr` = `TSharedPtr<FGasValidationRowItem>`。为什么包一层？SListView 要求元素是"可空指针类型"（和 SComboBox 同款要求），直接放结构体不行；共享指针还让"同一份问题数据被面板/日志/导出共享"变得自然。

## 3. 行控件：按列名发货

```cpp
class SGasValidatorRow : public SMultiColumnTableRow<FGasValidationRowItemPtr>
{
    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
    {
        Item = InArgs._Item;
        SMultiColumnTableRow<...>::Construct(FSuperRowType::FArguments(), OwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        // ListView 会为每个列名各调一次本函数
        if (ColumnString == GAS_COL_Severity)  return 严重度文字（带颜色）;
        if (ColumnString == GAS_COL_Category)  return 类别;
        if (ColumnString == GAS_COL_RuleId)    return 等宽字体规则名;
        if (ColumnString == GAS_COL_Asset)     return 资产名（tooltip 显示完整路径）;
        return 问题说明（自动折行）;
    }
};
```

机制：ListView 渲染一行时，对表头定义的**每个列名**调用一次 `GenerateWidgetForColumn`，你按列名返回不同控件——像 switch-case 分发货物的仓库管理员。

几个显示技巧：

```cpp
// 严重度配色（GetSeverityColor 是静态函数，行和面板共用）
Error   → FLinearColor(0.95f, 0.35f, 0.35f)   // 红
Warning → FLinearColor(0.95f, 0.80f, 0.35f)   // 黄
Info    → FLinearColor(0.60f, 0.60f, 0.60f)   // 灰

// 规则 ID 用等宽字体（代码感，也方便对齐）
SNew(STextBlock).Text(RuleText).Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))

// 长文案自动折行 + 悬停完整显示
SNew(STextBlock).Text(MessageText).AutoWrapText(true).ToolTipText(MessageText)
```

## 4. 扫描按钮：从点击到刷新

```cpp
FReply SGasValidatorPanel::OnScanClicked()
{
    {
        FScopedSlowTask ScanTask(1.f, FText::FromString(TEXT("正在扫描 GAS 配置资产…")));
        ScanTask.MakeDialog();                       // 弹进度条（给用户"生效了"的确认感）

        UGasValidatorSubsystem* Subsystem = ...;
        Subsystem->StartScan();                      // 扫描（内部会广播）
        ScanTask.EnterProgressFrame(1.f);
    }   // 作用域结束 = 进度条关闭

    return FReply::Handled();   // 告诉 Slate：事件我处理了，别往下传
}
```

**注意这里没有"刷新列表"的代码**——因为 `StartScan()` 内部广播时，`HandleValidationCompleted` 已经同步执行了刷新。事件驱动的代码流和直觉的"从上到下"不同：按钮回调只负责"发起"，结果处理集中在广播接收端。这样按钮、控制台命令、未来任何触发方式共享同一条处理路径。

```cpp
void SGasValidatorPanel::HandleValidationCompleted(const FGasValidationResult& Result)
{
    LastResult = Result;                                   // 存原始结果（过滤用）
    FilteredRows.Reset();                                  // 重建显示数据
    for (const FGasValidationIssue& Issue : LastResult.Issues)
        FilteredRows.Add(FGasValidationRowItem::Make(Issue));
    UpdateStatsText();                                     // 刷新统计行
    ListView->RequestListRefresh();                        // 通知列表重绘
}
```

## 5. 过滤与搜索：只动显示，不动数据

```cpp
bool SGasValidatorPanel::PassesFilter(const FGasValidationIssue& Issue) const
{
    if (*SelectedFilter == TEXT("仅错误") && Issue.Severity != Error) return false;
    if (*SelectedFilter == TEXT("错误+警告") && Issue.Severity == Info) return false;
    if (!SearchText.IsEmpty())
    {
        const FString Haystack = 资产名 + 规则 + 说明 + 路径;
        if (!Haystack.Contains(SearchText, ESearchCase::IgnoreCase)) return false;
    }
    return true;
}

void SGasValidatorPanel::RebuildFilteredRows()
{
    FilteredRows.Reset();
    for (const FGasValidationIssue& Issue : LastResult.Issues)
        if (PassesFilter(Issue)) FilteredRows.Add(FGasValidationRowItem::Make(Issue));
    ListView->RequestListRefresh();
}
```

设计原则：**原始结果 `LastResult` 永远完整**，过滤只重建 `FilteredRows`。切换过滤器不重新扫描（0.4 秒的扫描也不该重复付），搜索是实时的（`OnSearchTextChanged` 每次输入都调 `RebuildFilteredRows`）。

## 6. 双击跳转：打开资产两件套

```cpp
void SGasValidatorPanel::OpenIssueAsset(const FGasValidationIssue& Issue) const
{
    // ① 用资产编辑器子系统打开（蓝图/数据资产各自有默认编辑器）
    if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        AES->OpenEditorForAsset(Issue.ObjectPath);      // ObjectPath 形如 .../GA_FireBolt.GA_FireBolt_C

    // ② 内容浏览器定位
    FString AssetObjectPath = Issue.ObjectPath;
    AssetObjectPath.RemoveFromEnd(TEXT("_C"));          // 类路径 → 资产路径
    TArray<FAssetData> AssetsToSync;
    AssetRegistry.GetAssetsByPackageName(FName(*FPackageName::ObjectPathToPackageName(AssetObjectPath)), AssetsToSync);
    if (AssetsToSync.Num() > 0)
        ContentBrowser.Get().SyncBrowserToAssets(AssetsToSync);
}
```

两个 API 各管一半：`OpenEditorForAsset` 打开编辑器窗口，`SyncBrowserToAssets` 让左下角的内容浏览器跳转并选中。中间的"去 `_C` 后缀 + 查 AssetRegistry"是把"蓝图类"还原成"蓝图资产"（内容浏览器里存在的是资产，不是类）。

## 7. 工具栏按钮与标签页（DuraEditor.cpp）

标签页 = 一个装着面板的窗口格子：

```cpp
TSharedRef<SDockTab> FGasValidatorEditorModule::SpawnValidatorTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)                 // Nomad：独立工具页，不属于任何资产/关卡
        .Label(FText::FromString(TEXT("GAS 配置校验器")))
        [
            SNew(SGasValidatorPanel)                 // 标签页的身体就是我们的面板
        ];
}
```

编辑器每次需要显示这个标签页都会调 `SpawnValidatorTab`（比如你关了再开）。Nomad 标签页可以拖出来变浮动窗口（实测里它就成了独立窗口）、也可以停靠回主框架——这些都是编辑器送的能力，一行代码没写。

---

→ 下一篇：[09-关键UE概念通俗解释.md](09-关键UE概念通俗解释.md)，把用到的引擎概念做成"名词卡片"。
