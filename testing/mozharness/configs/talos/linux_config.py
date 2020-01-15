import os

VENV_PATH = '%s/build/venv' % os.getcwd()

config = {
    "log_name": "talos",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "title": os.uname()[1].lower().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
}
