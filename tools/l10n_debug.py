# -*- coding: utf-8 -*-
# 只读：读档/主菜单/主界面控件树名单（给用户自己汉化与找按钮用）
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

TARGETS = [
    "/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadScreen",
    "/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadScreenWidget_Base",
    "/Game/Blueprints/UI/MainMenu/WBP_MainMenu",
    "/Game/Blueprints/UI/Button/WBP_Button",
    "/Game/Blueprints/UI/Overlay/WBP_Overlay",
    "/Game/Blueprints/UI/Overlay/SubWidget/WBP_HealthManaSpells",
]

def txt_of(v):
    try:
        if type(v).__name__ == "Text":
            return str(v)
    except Exception:
        pass
    return None

for t in TARGETS:
    try:
        unreal.EditorAssetLibrary.load_asset(t)
    except Exception:
        pass
    pkg = t
    L("==== %s" % t)
    for obj in unreal.ObjectIterator():
        try:
            if obj.get_package().get_name() != pkg:
                continue
            cls = obj.get_class().get_name()
            interesting = (cls in ("TextBlock", "Button", "EditableTextBox", "RichTextBlock", "CheckBox")
                           or cls.startswith("WBP_"))
            if not interesting:
                continue
            txt = ""
            try:
                txt = txt_of(obj.get_editor_property("text")) or ""
            except Exception:
                pass
            L("  %s | %s | [%s]" % (cls, obj.get_name(), txt))
        except Exception:
            continue
L("[DUMP-DONE]")
OUT.close()
