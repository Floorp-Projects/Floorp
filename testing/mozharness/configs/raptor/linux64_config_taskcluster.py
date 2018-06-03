import os
import sys

PYTHON = sys.executable
VENV_PATH = '%s/build/venv' % os.getcwd()

TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/linux64/releng.manifest"
MINIDUMP_STACKWALK_PATH = "linux64-minidump_stackwalk"

exes = {
    'python': PYTHON,
}
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.tar.bz2")

config = {
    "log_name": "raptor",
    "installer_path": INSTALLER_PATH,
    "virtualenv_path": VENV_PATH,
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    "exes": exes,
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": MINIDUMP_STACKWALK_PATH,
    "minidump_tooltool_manifest_path": TOOLTOOL_MANIFEST_PATH,
    "tooltool_cache": "/builds/worker/tooltool-cache",
}
