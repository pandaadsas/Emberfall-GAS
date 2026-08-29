# -*- coding: utf-8 -*-
# 蓝图/资产汉化盘点脚本 v2（只读，不修改任何资产）
import unreal, json, os

RESULT = {
    "fonts": [], "widgets": [], "bound_texts": [], "node_comments": [],
    "datatables": [], "dataassets": [], "bp_cdo": [], "classes_seen": {},
    "skipped_heavy": [], "errors": [],
}

HEAVY_CLASSES = {"World", "Level", "SoundWave", "SoundCue", "StaticMesh",
                 "SkeletalMesh", "ParticleSystem", "NiagaraSystem", "Texture2D",
                 "Material", "MaterialInstanceConstant", "MaterialFunction",
                 "AnimMontage", "AnimSequence", "AnimComposite", "PhysicsAsset",
                 "Skeleton", "AnimationBlueprint", "Blueprint", "LandscapeGrassType",
                 "CurveFloat", "CurveTable", "BlendSpace", "BlendSpace1D",
                 "AnimBlueprint", "MetaDataAsset", "RigVMBlueprint", "ControlRigBlueprint"}

def has_english(s):
    if not s:
        return False
    s = str(s)
    return any('a' <= c.lower() <= 'z' for c in s) and not any('\u4e00' <= c <= '\u9fff' for c in s)

def as_text(v):
    if isinstance(v, str):
        return v
    try:
        if type(v).__name__ == "Text":
            try:
                return v.to_string()
            except Exception:
                return str(v)
    except Exception:
        pass
    return None

def scan_struct(el, prefix, out):
    for n in dir(el):
        if n.startswith("_"):
            continue
        try:
            v = getattr(el, n)
        except Exception:
            continue
        t = as_text(v)
        if t is not None:
            if has_english(t):
                out.append((prefix + n, t))
        elif isinstance(v, list):
            for i, e2 in enumerate(v):
                t2 = as_text(e2)
                if t2 is not None and has_english(t2):
                    out.append(("%s[%d]" % (prefix + n, i), t2))

def scan_properties(obj, prefix=""):
    hits = []
    for n in dir(obj):
        if n.startswith("_") or n in ("get_editor_property", "set_editor_property"):
            continue
        try:
            v = obj.get_editor_property(n)
        except Exception:
            try:
                v = getattr(obj, n)
            except Exception:
                continue
        t = as_text(v)
        if t is not None:
            if has_english(t):
                hits.append((prefix + n, t))
        elif isinstance(v, list):
            for i, el in enumerate(v):
                t2 = as_text(el)
                if t2 is not None:
                    if has_english(t2):
                        hits.append(("%s[%d]" % (prefix + n, i), t2))
                else:
                    tn = type(el).__name__
                    if tn not in ("int", "float", "bool", "str", "bytes", "Name", "NoneType"):
                        scan_struct(el, "%s[%d]." % (prefix + n, i), hits)
    return hits

def walk_panel(w, cb, depth=0):
    cb(w)
    if depth > 14:
        return
    try:
        if isinstance(w, unreal.PanelWidget):
            for i in range(w.get_children_count()):
                child = w.get_child_at(i)
                if child is not None:
                    walk_panel(child, cb, depth + 1)
    except Exception:
        pass

def scan_widget(path, obj_name):
    try:
        # WidgetTree 是 Instanced 子对象，用子对象路径直接取
        tree = None
        for sub in (":WidgetTree", ":WidgetTree_0"):
            try:
                tree = unreal.load_object(None, str(path) + sub)
                if tree is not None:
                    break
            except Exception:
                continue
        if tree is None:
            RESULT["errors"].append([str(path), "no widget_tree subobject"])
            return

        root = None
        for pn in ("root_widget", "RootWidget"):
            try:
                root = tree.get_editor_property(pn)
                if root is not None:
                    break
            except Exception:
                try:
                    root = getattr(tree, pn)
                    if root is not None:
                        break
                except Exception:
                    continue

        count = [0, 0]
        def on_widget(w):
            cls = w.get_class().get_name()
            txt = None
            try:
                txt = as_text(w.get_editor_property("text"))
            except Exception:
                txt = None
            if txt and has_english(txt):
                RESULT["widgets"].append([obj_name, str(path), cls, w.get_name(), txt])
                count[0] += 1
            bind = None
            for bn in ("text_delegate", "bound_text"):
                try:
                    bind = w.get_editor_property(bn)
                    if bind is not None:
                        break
                except Exception:
                    continue
            if bind is not None:
                RESULT["bound_texts"].append([obj_name, str(path), cls, w.get_name()])
                count[1] += 1

        if root is not None:
            walk_panel(root, on_widget)
        unreal.log("[L10N] %s -> texts:%d bound:%d" % (path, count[0], count[1]))
    except Exception as e:
        RESULT["errors"].append([str(path), repr(e)[:200]])

def scan_graph_comments(bp, obj_name, path):
    try:
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
                    try:
                        c = node.get_editor_property("node_comment")
                    except Exception:
                        continue
                    if c and has_english(c):
                        RESULT["node_comments"].append([obj_name, str(path), str(c)])
    except Exception as e:
        RESULT["errors"].append([str(path), "comments: " + repr(e)[:150]])

def scan_datatable(path, obj_name):
    try:
        dt = unreal.load_asset(str(path))
        if dt is None:
            RESULT["errors"].append([str(path), "load None"])
            return
        rows = []
        try:
            rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
        except Exception as e:
            RESULT["errors"].append([str(path), "row_names: " + repr(e)[:120]])
            return
        count = 0
        for rn in rows:
            row = None
            got = False
            try:
                ok, row = unreal.DataTableFunctionLibrary.get_data_table_row(dt, rn, None)
                got = bool(ok)
            except Exception:
                pass
            if not got:
                try:
                    ok, row = unreal.DataTableFunctionLibrary.get_data_table_row(dt, rn)
                    got = bool(ok)
                except Exception:
                    pass
            if not got or row is None:
                continue
            hits = []
            scan_struct(row, str(rn) + ".", hits)
            for p, t in hits:
                RESULT["datatables"].append([obj_name, str(path), str(rn), p, t])
                count += 1
        unreal.log("[L10N] %s -> rows:%d hits:%d" % (path, len(rows), count))
    except Exception as e:
        RESULT["errors"].append([str(path), repr(e)[:200]])

ASSETS = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
unreal.log("[L10N] total assets: %d" % len(ASSETS))

for path in ASSETS:
    path = str(path)
    try:
        aid = unreal.EditorAssetLibrary.find_asset_data(path)
        cls = str(aid.asset_class_path.asset_name)
        obj_name = os.path.basename(path)
    except Exception as e:
        RESULT["errors"].append([path, "asset_data: " + repr(e)[:120]])
        continue

    RESULT["classes_seen"][cls] = RESULT["classes_seen"].get(cls, 0) + 1

    if cls in ("Font", "FontFace", "CompositeFont"):
        RESULT["fonts"].append([obj_name, cls, path])
        continue
    if cls in HEAVY_CLASSES:
        RESULT["skipped_heavy"].append([obj_name, cls])
        continue

    if cls == "WidgetBlueprint":
        scan_widget(path, obj_name)
        try:
            bp = unreal.load_asset(path)
            if bp is not None:
                scan_graph_comments(bp, obj_name, path)
        except Exception as e:
            RESULT["errors"].append([path, "wbp comments: " + repr(e)[:150]])
    elif cls == "DataTable":
        scan_datatable(path, obj_name)
    elif cls.endswith("Blueprint"):
        # 其它蓝图（角色/能力/Actor）：扫节点注释 + CDO 文本属性
        try:
            bp = unreal.load_asset(path)
            if bp is not None:
                scan_graph_comments(bp, obj_name, path)
                try:
                    gencls = unreal.load_object(None, path + "_C")
                    if gencls is not None:
                        cdo = unreal.get_default_object(gencls)
                        if cdo is not None:
                            for p, t in scan_properties(cdo):
                                RESULT["bp_cdo"].append([obj_name, cls, path, p, t])
                except Exception as e:
                    RESULT["errors"].append([path, "cdo: " + repr(e)[:150]])
        except Exception as e:
            RESULT["errors"].append([path, repr(e)[:200]])
    else:
        # 其余一切资产（数据资产/材质参数集合等）做通用文本属性扫描
        try:
            asset = unreal.load_asset(path)
            if asset is not None:
                for p, t in scan_properties(asset):
                    RESULT["dataassets"].append([obj_name, cls, path, p, t])
        except Exception as e:
            RESULT["errors"].append([path, repr(e)[:200]])

out_json = os.path.join(os.path.dirname(__file__), "l10n_inventory.json")
with open(out_json, "w", encoding="utf-8") as f:
    json.dump(RESULT, f, ensure_ascii=False, indent=1)

summary = {k: len(v) for k, v in RESULT.items() if k != "classes_seen"}
unreal.log("[L10N] SUMMARY %s" % json.dumps(summary, ensure_ascii=False))
with open(os.path.join(os.path.dirname(__file__), "l10n_summary.json"), "w", encoding="utf-8") as f:
    json.dump({"summary": summary, "classes_seen": RESULT["classes_seen"]}, f, ensure_ascii=False, indent=1)
print("[L10N-DONE]")
