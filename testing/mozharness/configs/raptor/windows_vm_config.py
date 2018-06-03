import os
import socket
import sys

PYTHON = sys.executable
PYTHON_DLL = 'c:/mozilla-build/python27/python27.dll'
VENV_PATH = os.path.join(os.getcwd(), 'build/venv')

config = {
    "log_name": "raptor",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "pip_index": False,
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "virtualenv_modules": ['pywin32', 'raptor', 'mozinstall'],
    "exes": {
        'python': PYTHON,
        'easy_install': ['%s/scripts/python' % VENV_PATH,
                         '%s/scripts/easy_install-2.7-script.py' % VENV_PATH],
        'mozinstall': ['%s/scripts/python' % VENV_PATH,
                       '%s/scripts/mozinstall-script.py' % VENV_PATH],
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
    "metro_harness_path_frmt": "%(metro_base_path)s/metro/metrotestharness.exe",
    "download_minidump_stackwalk": True,
    "tooltool_cache": os.path.join('c:\\', 'build', 'tooltool_cache'),
    "minidump_stackwalk_path": "win32-minidump_stackwalk.exe",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/win32/releng.manifest",
    "python3_manifest": {
        "win32": "python3.manifest",
        "win64": "python3_x64.manifest",
    },
    "env": {
        # python3 requires C runtime, found in firefox installation; see bug 1361732
        "PATH": "%(PATH)s;c:\\slave\\test\\build\\application\\firefox;"
    }
}
