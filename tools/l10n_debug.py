# -*- coding: utf-8 -*-
# 只读：转储读档槽位三个控件的全部子对象（找 SELECT SLOT / Enter Name 的文本控件）
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

TARGETS = [
    "/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadSlot_Taken",
    "/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadSlot_Vacant",
    "/Game/Blueprints/UI/MainMenu/LoadMenu/WBP_LoadSlot_EnterName",
]

def txt_of(v):
    try:
        if type(v).__name__ == "Text":
            return str(v)
    except Exception:
        pass
    return None

for t in TARGETS:
    unreal.EditorAssetLibrary.load_asset(t)
    L("==== %s" % t)
    for obj in unreal.ObjectIterator():
        try:
            if obj.get_package().get_name() != t:
                continue
            cls = obj.get_class().get_name()
        except Exception:
            continue
        # 只关心可视控件类
        if not (cls.endswith("_C") or cls in ("TextBlock", "Button", "EditableTextBox",
                                              "EditableText", "RichTextBlock", "Image",
                                              "Border", "Overlay", "VerticalBox", "HorizontalBox",
                                              "SizeBox", "CanvasPanel", "NamedSlot", "WidgetSwitcher")):
            continue
        name = "?"
        try:
            name = obj.get_name()
        except Exception:
            pass
        # 跳过 CDO
        if name.startswith("Default__"):
            continue
        txt = ""
        try:
            txt = txt_of(obj.get_editor_property("text")) or ""
        except Exception:
            pass
        bind = ""
        for bn in ("text_delegate",):
            try:
                b = obj.get_editor_property(bn)
                if b is not None:
                    bind = " [BIND:%s]" % type(b).__name__
            except Exception:
                pass
        L("  %-28s | %-28s | [%s]%s" % (cls, name, txt, bind))
L("[DUMP-DONE]")
OUT.close()
