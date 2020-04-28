import os

config = {
    "log_name": "raptor",
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "install-chrome-android",
        "install-chromium-distribution",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
    "download_tooltool": True,
    "hostutils_manifest_path": "testing/config/tooltool-manifests/linux64/hostutils.manifest",
}

# raptor will pick these up in mitmproxy.py, doesn't use the mozharness config
os.environ['TOOLTOOLCACHE'] = config['tooltool_cache']
os.environ['HOSTUTILS_MANIFEST_PATH'] = config['hostutils_manifest_path']
