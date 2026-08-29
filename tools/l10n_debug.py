# -*- coding: utf-8 -*-
# 只读：查 WBP_WideButton 蓝图结构——暴露变量与内部文本绑定
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_out.txt"), "w", encoding="utf-8")
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

# 找到 WBP_WideButton 资产
candidates = unreal.EditorAssetLibrary.list_assets("/Game/Blueprints/UI", recursive=True, include_folder=False)
target = None
for p in candidates:
    if "WideButton" in str(p) and str(p).endswith("WBP_WideButton"):
        target = str(p)
        break
if target is None:
    for p in candidates:
        if "WideButton" in str(p):
            target = str(p)
            break
L("asset: %s" % target)

if target:
    bp = unreal.load_asset(target)
    gen = bp.generated_class()
    cdo = unreal.get_default_object(gen)
    # 生成类上所有可编辑属性（找暴露的 Text 变量）
    L("CDO props containing 'text':")
    for n in dir(cdo):
        if "text" in n.lower() and not n.startswith("_"):
            try:
                v = cdo.get_editor_property(n)
                L("  %s = [%s] (%s)" % (n, txt_of(v) if txt_of(v) is not None else v, type(v).__name__))
            except Exception as e:
                L("  %s (read err %s)" % (n, repr(e)[:60]))
    # 内部 TextBlock 的绑定情况
    for obj in unreal.ObjectIterator():
        try:
            if obj.get_package().get_name() != target.split(".")[0]:
                continue
            if obj.get_class().get_name() != "TextBlock":
                continue
            nm = obj.get_name()
            if nm.startswith("Default__"):
                continue
            txt = txt_of(obj.get_editor_property("text")) or ""
            bind = "无绑定(可编辑静态值)"
            try:
                b = obj.get_editor_property("text_delegate")
                if b is not None:
                    bind = "已绑定 type=%s" % type(b).__name__
                    try:
                        fn = b.get_editor_property("function_name") if hasattr(b, "get_editor_property") else None
                        if fn:
                            bind += " fn=%s" % fn
                    except Exception:
                        pass
            except Exception:
                pass
            L("inner TextBlock %s = [%s] %s" % (nm, txt, bind))
        except Exception:
            continue
L("[DUMP-DONE]")
OUT.close()
