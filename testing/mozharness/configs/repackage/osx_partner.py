import os

config = {
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "repack_id": os.environ.get("REPACK_ID"),

    # ToolTool
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
