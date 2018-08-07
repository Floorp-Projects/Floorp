import os

platform = "linux32"

config = {
    "locale": os.environ.get("LOCALE"),

    "download_config": {
        "target.tar.gz": os.environ.get("SIGNED_INPUT"),
        "mar": os.environ.get("UNSIGNED_MAR"),
    },

    "repackage_config": [[
        "mar",
        "-i", "{abs_input_dir}/target.tar.gz",
        "--mar", "{abs_input_dir}/mar",
        "-o", "{abs_output_dir}/target.complete.mar"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
