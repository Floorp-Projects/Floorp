import os

config = {
    "input_home": "{abs_work_dir}/inputs",
    "output_home": "{abs_work_dir}/artifacts{repack_id}",
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "repack_id": os.environ.get("REPACK_ID"),

    "download_config": {
        "target.tar.gz": os.environ.get("SIGNED_INPUT"),
    },

    "repackage_config": [[
        "dmg",
        "-i", "{abs_work_dir}/inputs/target.tar.gz",
        "-o", "{output_home}/target.dmg"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
