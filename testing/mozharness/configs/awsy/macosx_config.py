import os

PYTHON = "/usr/bin/env python"
VENV_PATH = '%s/build/venv' % os.getcwd()
TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/macosx64/releng.manifest"
MINIDUMP_STACKWALK_PATH = "macosx64-minidump_stackwalk"
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.dmg")

config = {
    "log_name": "awsy",
    "installer_path": INSTALLER_PATH,
    "virtualenv_path": VENV_PATH,
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "cmd_timeout": 6500,
    "exes": {
    },
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": MINIDUMP_STACKWALK_PATH,
    "minidump_tooltool_manifest_path": TOOLTOOL_MANIFEST_PATH,
    "tooltool_cache": os.path.join(os.getcwd(), "tooltool_cache"),
}
