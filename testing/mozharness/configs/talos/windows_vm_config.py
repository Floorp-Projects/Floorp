import os
import socket
import sys

PYTHON = sys.executable
PYTHON_DLL = 'c:/mozilla-build/python27/python27.dll'
VENV_PATH = os.path.join(os.getcwd(), 'build/venv')

config = {
    "log_name": "talos",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "exes": {
        'python': PYTHON,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg'),
    },
    "title": socket.gethostname().split('.')[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": os.path.join('c:\\', 'build', 'tooltool_cache'),
}
