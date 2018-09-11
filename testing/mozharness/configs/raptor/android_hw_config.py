import os

config = {
    "log_name": "raptor",
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "install-chrome",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
    "download_tooltool": True,
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": "linux64-minidump_stackwalk",
    "tooltool_servers": ['https://tooltool.mozilla-releng.net/'],
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/linux64/releng.manifest",
}
