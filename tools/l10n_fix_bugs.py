# -*- coding: utf-8 -*-
# Phase A：修复两个资产问题
# 1) GA_Electrocute 悬空引脚 OutAddditionalTargets -> OutAdditionalTargets（保持连线）
# 2) GA_Electrocute / GA_FireBolt 的 ActivationBlockedTags 中无效标签 Debuff.Lightning -> Debuff.Stun
import unreal, io, os

OUT = io.open(__file__.replace("l10n_fix_bugs.py", "fix_bugs_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")

def save(path):
    ok = unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(path), only_if_is_dirty=False)
    L("SAVE %s -> %s" % (path, ok))
    return ok

# ---------- 1. 悬空引脚改名 ----------
GA_ELEC = "/Game/Blueprints/AbilitySystem/Dura/Abilities/Lightning/GA_Electrocute"
bp = unreal.load_asset(GA_ELEC)
renamed = 0
for gp in ("ubergraph_pages", "function_graphs", "macro_graphs", "event_graphs"):
    try:
        graphs = bp.get_editor_property(gp)
    except Exception:
        continue
    if not graphs:
        continue
    for g in graphs:
        try:
            nodes = g.get_editor_property("nodes")
        except Exception:
            continue
        for node in nodes:
            ncls = node.get_class().get_name()
            if ncls != "K2Node_CallFunction":
                continue
            # 确认调用的函数名（function_reference 的路径）
            fn = ""
            try:
                ref = node.get_editor_property("function_reference")
                fn = ref.get_name() if ref else ""
            except Exception:
                pass
            try:
                pins = node.get_editor_property("pins")
            except Exception:
                continue
            for pin in pins:
                pn = str(pin.get_editor_property("pin_name"))
                if pn == "OutAddditionalTargets":
                    # 悬空引脚：改名为新参数名，连线保持不变
                    try:
                        pin.set_editor_property("pin_name", "OutAdditionalTargets")
                        after = str(pin.get_editor_property("pin_name"))
                        renamed += 1
                        L("PIN RENAMED on node(fn=%s): OutAddditionalTargets -> %s" % (fn, after))
                    except Exception as e:
                        L("PIN RENAME FAILED: " + repr(e)[:150])
L("renamed pins: %d" % renamed)
if renamed:
    save(GA_ELEC)

# ---------- 2. 无效标签替换 ----------
for path in (GA_ELEC, "/Game/Blueprints/AbilitySystem/Dura/Abilities/Fire/FireBolt/GA_FireBolt"):
    bp = unreal.load_asset(path)
    gencls = unreal.load_object(None, path + "_C")
    if gencls is None:
        L("no gen class for %s" % path)
        continue
    cdo = unreal.get_default_object(gencls)
    if cdo is None:
        L("no CDO for %s" % path)
        continue
    try:
        tags = cdo.get_editor_property("activation_blocked_tags")
    except Exception as e:
        L("read tags err %s %s" % (path, repr(e)[:100]))
        continue
    s = tags.export_text()
    L("TAGS %s before: %s" % (path.split("/")[-1], s))
    if "Debuff.Lightning" not in s:
        L("  no invalid tag, skip")
        continue
    # 替换 Debuff.Lightning -> Debuff.Stun（意图：眩晕期间禁止施法）
    new_s = s.replace("Debuff.Lightning", "Debuff.Stun")
    ok = tags.import_text(new_s)
    L("  import_text -> %s" % ok)
    cdo.set_editor_property("activation_blocked_tags", tags)
    after = cdo.get_editor_property("activation_blocked_tags").export_text()
    L("  after: %s" % after)
    save(path)

L("[FIX-DONE]")
OUT.close()
