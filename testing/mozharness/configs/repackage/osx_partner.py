import os

config = {
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "repack_id": os.environ.get("REPACK_ID"),

    "repackage_config": [[
        "dmg",
        "-i", "{abs_input_dir}/target.tar.gz",
        "-o", "{abs_output_dir}/target.dmg"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
