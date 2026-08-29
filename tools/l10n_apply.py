# -*- coding: utf-8 -*-
# 总执行（完整编辑器）：B2 字体回退链 + C WBP 文本汉化 + D 消息表重建
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "apply_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

SIMHEI_REF = "/Script/Engine.FontFace'/Game/Assets/Fonts/SimHei/SimHei.SimHei'"

def has_english(s):
    if not s:
        return False
    s = str(s)
    return any('a' <= c.lower() <= 'z' for c in s) and not any('\u4e00' <= c <= '\u9fff' for c in s)

def txt_of(v):
    try:
        if type(v).__name__ == "Text":
            return str(v)
    except Exception:
        pass
    return None

try:
    # ============ B2. Font 资产追加 CJK 回退 ============
    FONTS = [
        "/Game/Assets/Fonts/Amarante/Amarante-Regular_Font",
        "/Game/Assets/Fonts/David_Libre/DavidLibre-Bold_Font",
        "/Game/Assets/Fonts/David_Libre/DavidLibre-Medium_Font",
        "/Game/Assets/Fonts/David_Libre/DavidLibre-Regular_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-Black_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-Bold_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-ExtraBold_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-Light_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-Medium_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-Regular_Font",
        "/Game/Assets/Fonts/Inkut/InknutAntiqua-SemiBold_Font",
        "/Game/Assets/Fonts/Nanum_Brush/NanumBrushScript-Regular_Font",
        "/Game/Assets/Fonts/Pirata_One/PirataOne-Regular_Font",
    ]
    OLD = "FallbackTypeface=(Typeface=(Fonts=),ScalingFactor=1.000000)"
    NEW = ('FallbackTypeface=(Typeface=(Fonts=((Name="CJK Fallback",Font=(FontFaceAsset="%s")))),ScalingFactor=1.000000)' % SIMHEI_REF)
    for fp in FONTS:
        try:
            font = unreal.load_asset(fp)
            cf = font.get_editor_property("composite_font")
            s = cf.export_text()
            if "SimHei" in s:
                L("FONT %s already patched" % fp.split("/")[-1])
                continue
            if OLD not in s:
                L("FONT %s UNEXPECTED FORMAT: %s" % (fp.split("/")[-1], s))
                continue
            ok = cf.import_text(s.replace(OLD, NEW))
            font.set_editor_property("composite_font", cf)
            after = font.get_editor_property("composite_font").export_text()
            saved = unreal.EditorAssetLibrary.save_loaded_asset(font, only_if_is_dirty=False)
            L("FONT %s import=%s saved=%s simhei_in=%s" % (fp.split("/")[-1], ok, saved, ("SimHei" in after)))
        except Exception as e:
            L("FONT ERR %s %s" % (fp, repr(e)[:150]))

    # ============ C. WBP 静态文本汉化 ============
    # 映射: 包名 -> {控件名 -> 新文本}
    CHANGES = {
        "WBP_Title": {"TextBlock_146": "元素之主"},
        "WBP_AreYouSure": {"TextBlock_69": "确定要这么做吗？",
                           "TextBlock_Message": "删除存档槽是永久的，该进度将全部丢失。"},
        "WBP_AttributePointsRow": {"Text_AttributeName": "属性点数"},
        "WBP_TextValueRow": {"Text_AttributeName": "属性"},
        "WBP_AttributesMenu": {"Text_Attributes": "属性",
                               "Text_Attributes_1": "主属性",
                               "Text_Attributes_2": "次要属性"},
        "WBP_LoadSlot_Taken": {"Text_Map": "地图：", "Text_Level": "等级：", "Text_PlayerName": "玩家名称"},
        "WBP_LoadSlot_Vacant": {"TextBlock_42": "新游戏"},
        "WBP_EffectMessage": {"Text_Message": "拾取了生命药剂"},
        "WBP_HealthManaSpells": {"TextBlock_0": "攻击技能", "Text_LMB": "鼠标左键",
                                 "Text_RMB": "鼠标右键", "TextBlock": "被动技能"},
        "WBP_LevelUpMessage": {"TextBlock": "等级 ", "TextBlock_254": "你已达到"},
        "WBP_EquippedSpellRow": {"TextBlock_220": "攻击技能", "TextBlock_321": "被动技能",
                                 "TextBlock_LMB": "鼠标左键", "TextBlock_RMB": "鼠标右键"},
        "WBP_SpellMenu": {"Text_Attributes": "技能", "Text_Attributes_1": "被动技能",
                          "Text_Attributes_2": "攻击技能", "Text_Attributes_3": "已装备技能",
                          "TextBlock_154": "技能点数", "TextBlock_231": "描述"},
    }
    pkg_map = {}
    for p in unreal.EditorAssetLibrary.list_assets("/Game/Blueprints/UI", recursive=True, include_folder=False):
        p = str(p)
        try:
            aid = unreal.EditorAssetLibrary.find_asset_data(p)
            if str(aid.asset_class_path.asset_name) != "WidgetBlueprint":
                continue
        except Exception:
            continue
        base = p.split("/")[-1].split(".")[0]
        if base in CHANGES:
            pkg_map[p.split(".")[0]] = (base, CHANGES[base])
    L("WBP packages to change: %d" % len(pkg_map))

    changed = {}
    for obj in unreal.ObjectIterator():
        try:
            pkg = obj.get_package().get_name()
        except Exception:
            continue
        if pkg not in pkg_map:
            continue
        base, mapping = pkg_map[pkg]
        try:
            if obj.get_class().get_name() != "TextBlock":
                continue
            wname = obj.get_name()
        except Exception:
            continue
        if wname not in mapping:
            continue
        old = txt_of(obj.get_editor_property("text"))
        new = mapping[wname]
        obj.set_editor_property("text", unreal.Text(new))
        back = txt_of(obj.get_editor_property("text"))
        key = (base, wname)
        changed[key] = (old, new, back)
        L("TEXT %s | %s | [%s] -> [%s] readback=[%s]" % (base, wname, old, new, back))

    L("text changes applied: %d (expect %d unique)" % (len(changed), sum(len(v) for v in CHANGES.values())))

    # 保存所有改过的 WBP
    for pkg in pkg_map:
        bp = unreal.load_asset(pkg)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        L("SAVE %s -> %s" % (pkg.split("/")[-1], saved))

    # ============ D. 消息表重建（CSV 导入替换） ============
    L("UIWidgetRow fields: %s" % ", ".join(n for n in dir(unreal.UIWidgetRow) if not n.startswith("_")))
    try:
        import io as _io
        csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "message_table.csv")
        with _io.open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
            f.write("Name,MessageTag,Message\r\n")
            f.write("Message.HealthPotion,Message.HealthPotion,\"拾取了生命药剂\"\r\n")
            f.write("Message.ManaPotion,Message.ManaPotion,\"拾取了法力药剂\"\r\n")
            f.write("Message.HealthCrystal,Message.HealthCrystal,\"拾取了生命水晶\"\r\n")
            f.write("Message.ManaCrystal,Message.ManaCrystal,\"拾取了法力水晶\"\r\n")
        L("csv written")

        factory = unreal.DataTableFactory()
        fns = [n for n in dir(factory) if not n.startswith("_")]
        L("DataTableFactory props: %s" % ", ".join(fns))
        # 行结构设置
        try:
            factory.set_editor_property("struct", unreal.UIWidgetRow.static_struct())
        except Exception as e:
            L("factory struct prop err: " + repr(e)[:100])
            try:
                factory.set_editor_property("table_row_struct", unreal.UIWidgetRow.static_struct())
            except Exception as e2:
                L("factory table_row_struct err: " + repr(e2)[:100])

        task = unreal.AssetImportTask()
        task.filename = csv_path
        task.destination_path = "/Game/Blueprints/UI/Data"
        task.destination_name = "DT_MessageWidgetData"
        task.automated = True
        task.save = True
        task.replace_existing = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        dt = unreal.load_asset("/Game/Blueprints/UI/Data/DT_MessageWidgetData")
        if dt is not None:
            rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
            L("DT reimport rows: %s" % [str(r) for r in rows])
            saved = unreal.EditorAssetLibrary.save_loaded_asset(dt, only_if_is_dirty=False)
            L("DT saved: %s" % saved)
        else:
            L("DT load failed after import")
    except Exception as e:
        L("DT ERR " + repr(e)[:200])

    # ============ 验证：重读几个关键资产 ============
    try:
        wbp = unreal.load_asset("/Game/Blueprints/UI/Overlay/SubWidget/WBP_HealthManaSpells")
        tree = unreal.load_object(None, "/Game/Blueprints/UI/Overlay/SubWidget/WBP_HealthManaSpells.WBP_HealthManaSpells:WidgetTree")
        cnt = 0
        for obj in unreal.ObjectIterator():
            try:
                if obj.get_package().get_name() != "/Game/Blueprints/UI/Overlay/SubWidget/WBP_HealthManaSpells":
                    continue
                if obj.get_class().get_name() != "TextBlock":
                    continue
                L("VERIFY HealthManaSpells %s = [%s]" % (obj.get_name(), txt_of(obj.get_editor_property("text"))))
                cnt += 1
            except Exception:
                continue
    except Exception as e:
        L("VERIFY ERR " + repr(e)[:120])

except Exception as e:
    L("FATAL " + repr(e)[:300])

L("[APPLY-DONE]")
OUT.close()
unreal.SystemLibrary.quit_editor()
