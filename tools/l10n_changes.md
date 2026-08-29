# 蓝图/资产汉化清单（盘点结果与翻译方案）

> 盘点方式：UE 无头编辑器 ObjectIterator 全量扫描 1025 个资产。
> 结果：36 个 WBP、3 个 DataTable、178 个蓝图图表；无绑定文本、无图表字面量、数据资产无英文文本。

## 一、WBP 静态文本（32 条，去重后）

| WBP | 控件 | 原文 | 译为 |
|---|---|---|---|
| WBP_Title | TextBlock_77 | DURA | DURA（游戏名保留） |
| WBP_Title | TextBlock_146 | MASTER OF THE ELEMENTS | 元素之主 |
| WBP_Button | Text_Title | X | X（关闭符号保留） |
| WBP_AreYouSure | TextBlock_69 | Are You Sure? | 确定要这么做吗？ |
| WBP_AreYouSure | TextBlock_Message | Deleting a slot is permanent. You will lose all progress. | 删除存档槽是永久的，该进度将全部丢失。 |
| WBP_AttributePointsRow | Text_AttributeName | Attribute Points | 属性点数 |
| WBP_TextValueRow | Text_AttributeName | Attribute | 属性 |
| WBP_AttributesMenu | Text_Attributes | Attributes | 属性 |
| WBP_AttributesMenu | Text_Attributes_1 | Primary Attributes | 主属性 |
| WBP_AttributesMenu | Text_Attributes_2 | Secondary Attributes | 次要属性 |
| WBP_LoadSlot_Taken | Text_Map | Map: | 地图： |
| WBP_LoadSlot_Taken | Text_Level | Level: | 等级： |
| WBP_LoadSlot_Taken | Text_PlayerName | Player Name | 玩家名称 |
| WBP_LoadSlot_Vacant | TextBlock_42 | NEW GAME | 新游戏 |
| WBP_EffectMessage | Text_Message | Pick up a Health Potion | 拾取了生命药剂（占位文本，运行时来自下表） |
| WBP_HealthManaSpells | TextBlock_0 | Offsensive（原拼写错误） | 攻击技能 |
| WBP_HealthManaSpells | Text_LMB | LMB | 鼠标左键 |
| WBP_HealthManaSpells | Text_RMB | RMB | 鼠标右键 |
| WBP_HealthManaSpells | TextBlock | Passive | 被动技能 |
| WBP_LevelUpMessage | TextBlock | Level （尾随空格） | 等级  |
| WBP_LevelUpMessage | TextBlock_254 | YOU HAVE REACHED | 你已达到 |
| WBP_LevelUpMessage | Text_Level | X | X（数字占位保留） |
| WBP_EquippedSpellRow | TextBlock_220 | Offensive | 攻击技能 |
| WBP_EquippedSpellRow | TextBlock_321 | Passive | 被动技能 |
| WBP_EquippedSpellRow | TextBlock_LMB | LMB | 鼠标左键 |
| WBP_EquippedSpellRow | TextBlock_RMB | RMB | 鼠标右键 |
| WBP_SpellMenu | Text_Attributes | ABILITIES | 技能 |
| WBP_SpellMenu | Text_Attributes_1 | PASSIVE ABILITIES | 被动技能 |
| WBP_SpellMenu | Text_Attributes_2 | OFFENSIVE ABILITIES | 攻击技能 |
| WBP_SpellMenu | Text_Attributes_3 | EQUIPPED ABILITIES | 已装备技能 |
| WBP_SpellMenu | TextBlock_154 | Spell Points | 技能点数 |
| WBP_SpellMenu | TextBlock_231 | Description | 描述 |

## 二、DataTable（运行时消息表）

`DT_MessageWidgetData` 共 4 行需要翻译（表本身无法用 Python 直接写行，采用 CSV 重建导入或留给你手动改）：

| 行名 | 原文 | 译为 |
|---|---|---|
| Message.HealthPotion | Pick up a Health Potion | 拾取了生命药剂 |
| Message.ManaPotion | Pick up a Mana Potion | 拾取了法力药剂 |
| Message.HealthCrystal | Pick up a Health Crystal | 拾取了生命水晶 |
| Message.ManaCrystal | Pick up a Mana Crystal | 拾取了法力水晶 |

另两张表无文本：`DT_PrimaryInitialValues`（纯数值）、`DT_RichTextStyle`（富文本样式：颜色/字号，无用户可见文字）。

## 三、中文字体（必须做，否则全部显示方块）

项目 26 个字体资产全是拉丁字体。方案：导入 `C:\Windows\Fonts\simhei.ttf`（黑体，含全部简体字形）生成 FontFace，并通过 Font 资产的 CompositeFont 追加中文回退字体——英文界面保持原字体外观，中文字符自动回退到黑体。

## 四、顺带修复的两个资产问题（盘点过程中发现）

1. **GA_Electrocute**：存在悬空引脚 `Out Addditional Targets`（对应此前 C++ 侧拼写重命名 `OutAddditionalTargets→OutAdditionalTargets`，蓝图引脚引用还挂在旧名上）→ 将蓝图内的引脚引用改名到新参数，连接保持完好。
2. **GA_Electrocute / GA_FireBolt**：`ActivationBlockedTags` 里配置了不存在的标签 `Debuff.Lightning`（加载时引擎报 Warning）→ 移除该无效标签（眩晕时的输入屏蔽由 C++ 动态添加 Player_Block_* 标签实现，功能不受影响）。

## 五、执行与恢复

- 每个资产：改文本 → 内存读回验证 → 保存 → git 提交
- 恢复：`git restore --source 汉化前快照 -- Content/`（单个资产换具体路径）
- 修改后你需要打开编辑器做一次视觉验收（字体效果、布局）
