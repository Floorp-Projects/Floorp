import os
import socket
import sys

PYTHON = sys.executable
PYTHON_DLL = 'c:/mozilla-build/python27/python27.dll'
VENV_PATH = os.path.join(os.getcwd(), 'build/venv')

config = {
    "log_name": "talos",
    "buildbot_json_path": "buildprops.json",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "exes": {
        'python': PYTHON,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg'),
        'tooltool.py': [PYTHON, os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')],
    },
    "title": socket.gethostname().split('.')[0],
    "default_actions": [
        "clobber",
        "read-buildbot-config",
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
