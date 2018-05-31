import os
import socket
import sys

PYTHON = sys.executable
PYTHON_DLL = 'c:/mozilla-build/python/python27.dll'
VENV_PATH = os.path.join(os.getcwd(), 'venv')

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
        "populate-webroot",
        "create-virtualenv",
        "install",
        "setup-mitmproxy",
        "run-tests",
    ],
    "metro_harness_path_frmt": "%(metro_base_path)s/metro/metrotestharness.exe",
    "download_minidump_stackwalk": True,
    "tooltool_cache": os.path.join('Y:\\', 'tooltool-cache'),
    "minidump_stackwalk_path": "win32-minidump_stackwalk.exe",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/win32/releng.manifest",
    "python3_manifest": {
        "win32": "python3.manifest",
        "win64": "python3_x64.manifest",
    },
    "env": {
        # python3 requires C runtime, found in firefox installation; see bug 1361732
        "PATH": "%(PATH)s;%(CD)s\\build\\application\\firefox;"
    }
}
