
# -*- coding: utf-8 -*-
import unreal, io, os
OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n"); OUT.flush()
ASSET = "/Game/Blueprints/AbilitySystem/Data/DA_AbilityInfo"
unreal.EditorAssetLibrary.load_asset(ASSET)
asset = unreal.load_asset(ASSET)
for n in dir(asset):
    if n.startswith("_"): continue
    try: v = asset.get_editor_property(n)
    except Exception: continue
    if isinstance(v, list) or type(v).__name__ == "Array":
        L("array %s len=%d" % (n, len(v)))
        for i, el in enumerate(v):
            fields = {}
            for f in dir(el):
                if f.startswith("_"): continue
                try: fv = getattr(el, f)
                except Exception: continue
                tn = type(fv).__name__
                if tn in ("int","float","bool"): fields[f] = fv
                elif tn == "Name": fields[f] = str(fv)
                elif tn == "Text": fields[f] = str(fv)
            L("  [%d] %s" % (i, fields))
L("[DONE]")
OUT.close()
