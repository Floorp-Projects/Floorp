# Config file for firefox ui tests run via TaskCluster.

config = {
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],

    "pip_index": False,

    "download_symbols": "ondemand",
    "download_minidump_stackwalk": True,

    "tooltool_cache": "/builds/tooltool_cache",
}
