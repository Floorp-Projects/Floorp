import os
import platform
import sys

PYTHON = sys.executable
VENV_PATH = '%s/build/venv' % os.getcwd()

if platform.architecture()[0] == '64bit':
    TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/linux64/releng.manifest"
    MINIDUMP_STACKWALK_PATH = "linux64-minidump_stackwalk"
else:
    TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/linux32/releng.manifest"
    MINIDUMP_STACKWALK_PATH = "linux32-minidump_stackwalk"

exes = {
    'python': PYTHON,
}
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.tar.bz2")

config = {
    "log_name": "talos",
    "installer_path": INSTALLER_PATH,
    "virtualenv_path": VENV_PATH,
    "exes": exes,
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "setup-mitmproxy",
        "run-tests",
    ],
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": MINIDUMP_STACKWALK_PATH,
    "minidump_tooltool_manifest_path": TOOLTOOL_MANIFEST_PATH,
    "tooltool_cache": "/builds/worker/tooltool-cache",
}
