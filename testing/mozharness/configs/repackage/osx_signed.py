import os

config = {
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "locale": os.environ.get("LOCALE"),

    # ToolTool
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
