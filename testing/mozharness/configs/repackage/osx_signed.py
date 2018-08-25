import os

config = {
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "locale": os.environ.get("LOCALE"),

    "repackage_config": [[
        "dmg",
        "-i", "{abs_input_dir}/target.tar.gz",
        "-o", "{abs_output_dir}/target.dmg"
    ], [
        "mar",
        "-i", "{abs_input_dir}/target.tar.gz",
        "--mar", "{abs_input_dir}/mar",
        "-o", "{abs_output_dir}/target.complete.mar"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
