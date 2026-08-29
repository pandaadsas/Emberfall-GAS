# -*- coding: utf-8 -*-
# 只读：转储 DA_AttributeInfo 全部字段内容
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

ASSET = "/Game/Blueprints/AbilitySystem/Data/DA_AttributeInfo"
unreal.EditorAssetLibrary.load_asset(ASSET)
asset = unreal.load_asset(ASSET)
L("class: %s" % asset.get_class().get_name())

def txt_of(v):
    try:
        if type(v).__name__ == "Text":
            return str(v)
    except Exception:
        pass
    return None

for n in dir(asset):
    if n.startswith("_"):
        continue
    try:
        v = asset.get_editor_property(n)
    except Exception:
        continue
    L("prop %s : %s" % (n, type(v).__name__))
    if isinstance(v, list) or type(v).__name__ == "Array":
        L("  array len=%d" % len(v))
        for i, el in enumerate(v):
            fields = [f for f in dir(el) if not f.startswith("_")]
            parts = []
            for f in fields:
                try:
                    fv = getattr(el, f)
                except Exception:
                    continue
                t = txt_of(fv)
                if t is not None:
                    parts.append("%s=[%s]" % (f, t))
                else:
                    tn = type(fv).__name__
                    if tn in ("Name",):
                        parts.append("%s=%s" % (f, str(fv)))
                    elif tn in ("int", "float", "bool"):
                        parts.append("%s=%s" % (f, fv))
            L("  [%d] %s" % (i, " | ".join(parts)))

L("[DUMP-DONE]")
OUT.close()
