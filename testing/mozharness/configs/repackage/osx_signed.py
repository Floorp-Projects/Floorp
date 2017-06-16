import os

config = {
    "input_home": "{abs_work_dir}/inputs",
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    "download_config": {
        "target.tar.gz": os.environ.get("SIGNED_INPUT"),
    },

    "repackage_config": [[
        "dmg",
        "-i", "{abs_work_dir}/inputs/target.tar.gz",
        "-o", "{output_home}/target.dmg"
    ]],

    # ToolTool
    "tooltool_manifest_src": 'browser/config/tooltool-manifests/macosx64/cross-releng.manifest',
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
