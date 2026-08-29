# -*- coding: utf-8 -*-
# Phase B1（完整编辑器）：导入黑体 FontFace + 查看字体组合格式
import unreal, io, os

OUT = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts_out.txt"), "w", encoding="utf-8")
def L(s):
    OUT.write(s + "\n")
    OUT.flush()

DEST = "/Game/Assets/Fonts/SimHei"
try:
    existing = unreal.EditorAssetLibrary.does_asset_exist(DEST + "/SimHei")
    L("exists before: %s" % existing)
    if not existing:
        task = unreal.AssetImportTask()
        task.filename = "C:/Windows/Fonts/simhei.ttf"
        task.destination_path = DEST
        task.destination_name = "SimHei"
        task.automated = True
        task.save = True
        task.replace_existing = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        L("imported: %s" % unreal.EditorAssetLibrary.does_asset_exist(DEST + "/SimHei"))

    face = unreal.load_asset(DEST + "/SimHei") if unreal.EditorAssetLibrary.does_asset_exist(DEST + "/SimHei") else None
    if face is None:
        L("FATAL: FontFace not available")
    else:
        L("fontface class: %s" % face.get_class().get_name())
        try:
            face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
            unreal.EditorAssetLibrary.save_loaded_asset(face, only_if_is_dirty=False)
            L("loading_policy INLINE + saved")
        except Exception as e:
            L("loading_policy err " + repr(e)[:120])

        font = unreal.load_asset("/Game/Assets/Fonts/Amarante/Amarante-Regular_Font")
        cf = font.get_editor_property("composite_font")
        L("export_text:\n%s" % cf.export_text())
except Exception as e:
    L("FATAL " + repr(e)[:300])

L("[FONTS-DONE]")
OUT.close()
unreal.SystemLibrary.quit_editor()
