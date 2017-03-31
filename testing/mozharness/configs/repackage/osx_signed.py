import os

config = {
    "input_filename": "target.tar.gz",
    "output_filename": "target.dmg",
    "input_home": "/home/worker/workspace/inputs",
    "src_mozconfig": "browser/config/mozconfigs/macosx64/repack",

    # ToolTool
    "tooltool_manifest_src": 'browser/config/tooltool-manifests/macosx64/cross-releng.manifest',
    "tooltool_url": 'http://relengapi/tooltool/',
    "tooltool_bootstrap": "setup.sh",
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),
}
