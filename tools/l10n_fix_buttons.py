# -*- coding: utf-8 -*-
# 修复：WBP_WideButton 私有绑定变量文本（Details 面板只读，走引擎属性接口写入）
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fix_buttons_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

def txt_of(v):
    try:
        if type(v).__name__ == "Text":
            return str(v)
    except Exception:
        pass
    return None

JOBS = [
    ("/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadSlot_Taken", "Button_SelectSlot", "选择槽位"),
    ("/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadSlot_EnterName", "Button_NewSlot", "新建槽位"),
]

try:
    for pkg, widget_name, new_text in JOBS:
        unreal.EditorAssetLibrary.load_asset(pkg)
        hit = 0
        for obj in unreal.ObjectIterator():
            try:
                if obj.get_package().get_name() != pkg:
                    continue
                if obj.get_class().get_name() != "WBP_WideButton_C" or obj.get_name() != widget_name:
                    continue
            except Exception:
                continue
            old = txt_of(obj.get_editor_property("text"))
            obj.set_editor_property("text", unreal.Text(new_text))
            back = txt_of(obj.get_editor_property("text"))
            hit += 1
            L("OK %s | %s | [%s] -> [%s] readback=[%s]" % (pkg.split("/")[-1], widget_name, old, new_text, back))
        if hit:
            bp = unreal.load_asset(pkg)
            saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
            L("SAVE %s -> %s" % (pkg.split("/")[-1], saved))
        else:
            L("WARN not found: %s in %s" % (widget_name, pkg))
except Exception as e:
    L("FATAL " + repr(e)[:300])

L("[FIX-DONE]")
OUT.close()
