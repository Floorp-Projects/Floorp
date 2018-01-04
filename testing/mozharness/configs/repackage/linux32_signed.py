import os

platform = "linux32"

config = {
    "input_home": "{abs_work_dir}/inputs",
    "output_home": "{abs_work_dir}/artifacts{locale}",

    "locale": os.environ.get("LOCALE"),

    "download_config": {
        "target.tar.gz": os.environ.get("SIGNED_INPUT"),
        "mar": os.environ.get("UNSIGNED_MAR"),
    },

    "repackage_config": [[
        "mar",
        "-i", "{abs_work_dir}/inputs/target.tar.gz",
        "--mar", "{abs_work_dir}/inputs/mar",
        "-o", "{output_home}/target.complete.mar"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
