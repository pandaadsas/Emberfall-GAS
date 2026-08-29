# -*- coding: utf-8 -*-
# Phase A2：修标签 + 探查蓝图编辑 API + 全量编译诊断
import unreal, io

OUT = io.open(__file__.replace("l10n_debug.py", "debug_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")

# ---------- 1. 无效标签修复 ----------
for path in ("/Game/Blueprints/AbilitySystem/Dura/Abilities/Lightning/GA_Electrocute",
             "/Game/Blueprints/AbilitySystem/Dura/Abilities/Fire/FireBolt/GA_FireBolt"):
    bp = unreal.load_asset(path)
    gen = bp.generated_class()
    cdo = unreal.get_default_object(gen)
    tags = cdo.get_editor_property("activation_blocked_tags")
    s = tags.export_text()
    L("TAGS %s before: %s" % (path.split("/")[-1], s))
    if "Debuff.Lightning" in s:
        ok = tags.import_text(s.replace("Debuff.Lightning", "Debuff.Stun"))
        cdo.set_editor_property("activation_blocked_tags", tags)
        after = cdo.get_editor_property("activation_blocked_tags").export_text()
        L("  import=%s after: %s" % (ok, after))
        saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        L("  saved: %s" % saved)
    else:
        L("  no invalid tag")

# ---------- 2. 蓝图编辑 API 探查 ----------
if hasattr(unreal, "BlueprintEditorLibrary"):
    fns = [n for n in dir(unreal.BlueprintEditorLibrary) if not n.startswith("_")]
    L("BlueprintEditorLibrary: %s" % ", ".join(fns))
else:
    L("no BlueprintEditorLibrary")

# ---------- 3. 全量编译诊断（只编译不保存） ----------
ASSETS = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
bp_list = []
for p in ASSETS:
    p = str(p)
    try:
        aid = unreal.EditorAssetLibrary.find_asset_data(p)
        cls = str(aid.asset_class_path.asset_name)
    except Exception:
        continue
    if cls in ("Blueprint", "WidgetBlueprint"):
        bp_list.append(p)
L("blueprints to compile: %d" % len(bp_list))

ok_count = 0
fail = []
for p in bp_list:
    try:
        bp = unreal.load_asset(p)
        r = unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if r:
            ok_count += 1
        else:
            fail.append(p)
    except Exception as e:
        fail.append(p + " EXC:" + repr(e)[:80])
L("compile ok=%d fail=%d" % (ok_count, len(fail)))
for f in fail:
    L("COMPILE-FAIL %s" % f)
L("[DBG-DONE]")
OUT.close()
