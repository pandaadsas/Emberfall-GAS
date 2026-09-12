# Emberfall｜余烬陨落

> 基于 **Unreal Engine 5.8 · Gameplay Ability System (GAS)** 的暗黑奇幻第三人称 ARPG
> 全中文注释 · 全中文学习文档 · 个人作品集项目

## 项目简介

Emberfall（余烬陨落）是一个以 Epic 官方 **Gameplay Ability System** 为核心骨架的动作 RPG 项目。项目完整跑通了从 **输入 → 技能激活 → GameplayEffect 结算 → 属性更新 → UI 刷新** 的 GAS 全链路数据流，并针对 UE 5.8 做了全量适配与稳定性加固。

## 核心系统

### 技能系统（GAS）
- **主动技能**：火弹术（FireBolt）、火爆术（FireBlast）、奥术碎片（ArcaneShards）、奥术光束（BeamSpell）、近战攻击（MeleeAttack）、召唤术（Summon）等
- **被动技能**：电击连锁（Electrocute）等，基于 Niagara 组件的被动光环表现
- **冷却 / 消耗**：异步任务监听（`UWaitCooldownChange`、`TargetDataUnderMouse` 自定义 AbilityTask）
- **减益系统**：灼烧、眩晕等 Debuff，配 `DebuffNiagaraComponent` 表现层
- **技能解锁与升级**：技能菜单按角色等级解锁，含未解锁文案展示

### 属性与伤害管线
- `AttributeSet`：4 主属性 + 10 次属性 + 4 系抗性，Tag 驱动的派生属性映射
- **MMC 派生计算**（MaxHealth / MaxMana 随主属性成长）
- `ExecCalc_Damage`：护甲 / 护甲穿透、格挡、暴击、抗性完整结算公式
- 伤害飘字、受击反馈、经验奖励（IncomingXP 事件流）

### 角色与世界
- 玩家 / 敌人双轨初始化（`CharacterClassInfo` 数据驱动，战士 / 游侠 / 元素使三种职业成长曲线）
- 行为树 AI：索敌服务（寻找最近玩家）、攻击任务、`DuraAIController`
- 刷怪器（EnemySpawnPoint / SpawnVolume）、关卡传送（MapEntrance）、检查点（CheckPoint）
- 高亮描边（HighlightInterface）、魔法阵（MagicCircle）、拾取掉落（LootTiers）

### UI 架构（MVC / MVVM）
- 属性菜单 / 技能菜单 / 主界面 Overlay 全套 **WidgetController** 架构（数据与视图解耦）
- 读档加载屏采用 **ModelViewViewModel** 模式 + FieldNotify 数据绑定
- 多槽位存档 / 读档（`LoadScreenSaveGame` + SaveInterface 分帧序列化）

### 编辑器工具（自研模块）
- **GAS 配置校验器（DuraEditor）**：一键扫描全部技能 / 效果 / 数据资产，自动发现配置错误（缺失技能类、无效标签、等级表断档等），详见 [GAS 配置校验器](GAS%20配置校验器/README.md)

## 技术亮点

- **UE 5.8 全量适配**：弃用 API 清理、TMap 语义适配、全面判空加固、断言降级
- **日志规范**：`LogTemp` 全量替换为项目日志通道 `LogDura`
- **本地化**：C++ 注释与游戏内文本全量汉化，蓝图文本汉化 + 中文字体回退
- **文档化**：14 篇全链路源码讲解，覆盖框架启动到投射物表现，并附面试考点速查手册

## 快速开始

1. 安装 **Unreal Engine 5.8**
2. 安装 **Visual Studio 2022**（所需组件清单见 `.vsconfig`）
3. 克隆本仓库，右键 `Dura.uproject` → **Generate Visual Studio project files**
4. 编译 `Dura` 模块（Development Editor）后启动项目即可

> 命名说明：代码中的类名 / 模块名统一以 `Dura` 为前缀，它是本项目的工程代号，保留前缀可以让 C++ 与 Content 资产的引用保持一致。

## 目录结构

```
Source/
  Dura/               游戏运行时模块（GAS / 角色 / AI / UI / 存档 / 世界交互）
  DuraEditor/         编辑器模块：GAS 配置校验器
Content/              蓝图 / 地图 / 特效 / 字体等游戏资产
Data/                 曲线表（升级经验、职业属性成长）
docs/                 14 篇全链路中文学习文档
GAS 配置校验器/        DuraEditor 工具的需求设计与使用说明
tools/                蓝图汉化与批量调试脚本
```

## 学习文档导航

| 文档 | 内容 |
| --- | --- |
| [01 全局框架启动链路](docs/01-全局框架启动链路-从模块加载到GameplayTag注册.md) | 从模块加载到 GameplayTag 注册 |
| [02 游戏模式与角色诞生链路](docs/02-游戏模式与角色诞生链路-ASC安装与玩家敌人双轨初始化.md) | ASC 安装与玩家敌人双轨初始化 |
| [03 输入分发链路](docs/03-输入分发链路-从鼠标按键到ASC技能触发.md) | 从鼠标按键到 ASC 技能触发 |
| [04 属性系统全链路](docs/04-属性系统全链路-AttributeSet属性流水线与派生计算.md) | AttributeSet 属性流水线与派生计算 |
| [05 伤害计算与 GE 流水线](docs/05-伤害计算与GE流水线-ExecCalc公式与Library工具箱.md) | ExecCalc 公式与 Library 工具箱 |
| [06 技能释放全链路](docs/06-技能释放全链路-从输入Tag到技能激活与异步任务.md) | 从输入 Tag 到技能激活与异步任务 |
| [07 数据驱动配置链路](docs/07-数据驱动配置链路-五大DataAsset与查表体系.md) | 五大 DataAsset 与查表体系 |
| [08 UI 与 WidgetController 链路](docs/08-UI与WidgetController链路-MVC模式下的数据流.md) | MVC 模式下的数据流 |
| [09 存档与加载屏 MVVM 链路](docs/09-存档与加载屏MVVM链路-SaveGame与FieldNotify数据绑定.md) | SaveGame 与 FieldNotify 数据绑定 |
| [10 敌人 AI 与战斗链路](docs/10-敌人AI与战斗链路-行为树驱动与索敌服务.md) | 行为树驱动与索敌服务 |
| [11 世界交互链路](docs/11-世界交互链路-效果Actor检查点与接口体系.md) | 效果 Actor、检查点与接口体系 |
| [12 投射物与法术 Actor 链路](docs/12-投射物与法术Actor链路-碰撞命中与网络销毁表现.md) | 碰撞命中与网络销毁表现 |
| [13 面试考点速查手册](docs/13-面试考点速查手册.md) | GAS 高频面试考点速查 |
| [00 学习路线总览](docs/00-学习路线总览.md) | 全部文档的阅读路线图 |

## 致谢

- 本项目基于 [GAS_Dura](https://github.com/hangge1/GAS_Dura)（MIT License，Copyright © 2024-2026 Zhihang Zhang）修改而来，在其基础上完成了 **UE 5.8 适配、稳定性修复、全量中文化、编辑器工具（GAS 配置校验器）与配套文档建设**。
- 感谢 Epic Games 的 Gameplay Ability System 与相关社区教程对 GAS 学习生态的贡献。

## License

本项目基于 MIT 协议开源，详见 [LICENSE](LICENSE)。第三方素材、课程资料与外部资源仍遵循其原始授权。
