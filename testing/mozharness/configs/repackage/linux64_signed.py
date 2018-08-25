import os

platform = "linux64"

config = {
    "locale": os.environ.get("LOCALE"),

    "repackage_config": [[
        "mar",
        "-i", "{abs_input_dir}/target.tar.bz2",
        "--mar", "{abs_input_dir}/mar",
        "-o", "{abs_output_dir}/target.complete.mar"
    ]],

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
