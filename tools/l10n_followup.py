# -*- coding: utf-8 -*-
# 跟进 v3：fill_data_table_from_csv_string 直接改消息表（保留全部列引用）
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "followup_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

try:
    dt = unreal.load_asset("/Game/Blueprints/UI/Data/DT_MessageWidgetData")

    # 原表导出的全部列与引用原样保留，仅 Message 的源字符串换成中文（NSLOCTEXT key 不变）
    ROWS = [
        ('Message.HealthPotion',  'D8AFDC1D4EAC4E02576C4DA7FA63E362', '拾取了生命药剂',
         "/Script/Engine.Texture2D'/Game/Assets/UI/Pickups/T_Potion_Red.T_Potion_Red'"),
        ('Message.ManaPotion',    '2DA102184E297BBE2D01C6ACD50C342B', '拾取了法力药剂',
         "/Script/Engine.Texture2D'/Game/Assets/UI/Pickups/T_Potion_Blue.T_Potion_Blue'"),
        ('Message.HealthCrystal', 'AA9CB0C14AB14FBAFCDEA19A9E36F183', '拾取了生命水晶',
         "/Script/Engine.Texture2D'/Game/Assets/UI/Pickups/T_HealthCrystal.T_HealthCrystal'"),
        ('Message.ManaCrystal',   '038ACB4945B4964834F52D9B70B29226', '拾取了法力水晶',
         "/Script/Engine.Texture2D'/Game/Assets/UI/Pickups/T_ManaCrystal.T_ManaCrystal'"),
    ]
    WIDGET = "/Script/UMG.WidgetBlueprintGeneratedClass'/Game/Blueprints/UI/Overlay/SubWidget/WBP_EffectMessage.WBP_EffectMessage_C'"
    lines = ['---,MessageTag,Message,MessageWidget,Image']
    for tag, key, zh, img in ROWS:
        message = 'NSLOCTEXT("[8DA41B181D8CE8CAEBD4166A7F6FBCC1]", "%s", "%s")' % (key, zh)
        def esc(s):
            return '"' + s.replace('"', '""') + '"'
        lines.append(','.join([tag, esc('(TagName="%s")' % tag), esc(message), esc(WIDGET), esc(img)]))
    csv_string = '\r\n'.join(lines) + '\r\n'
    L("csv to fill:\n%s" % csv_string)

    result = None
    try:
        result = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(dt, csv_string)
    except Exception as e:
        L("variant1 err " + repr(e)[:120])
        try:
            result = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(dt, csv_string, True)
        except Exception as e2:
            L("variant2 err " + repr(e2)[:120])
    L("fill result: %s" % result)

    saved = unreal.EditorAssetLibrary.save_loaded_asset(dt, only_if_is_dirty=False)
    L("saved: %s" % saved)

    # 验证
    csv_back = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(dt)
    L("CSV AFTER:\n%s" % csv_back)
    L("HAS_CHINESE: %s" % ('拾取' in csv_back))

except Exception as e:
    L("FATAL " + repr(e)[:300])

L("[FOLLOWUP-DONE]")
OUT.close()
unreal.SystemLibrary.quit_editor()
