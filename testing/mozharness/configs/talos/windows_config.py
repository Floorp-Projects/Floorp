import os
import socket

PYTHON = 'c:/mozilla-build/python27/python.exe'
PYTHON_DLL = 'c:/mozilla-build/python27/python27.dll'
VENV_PATH = os.path.join(os.getcwd(), 'build/venv')

config = {
    "log_name": "talos",
    "buildbot_json_path": "buildprops.json",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "virtualenv_python_dll": PYTHON_DLL,
    "pip_index": False,
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "virtualenv_modules": ['pywin32', 'talos', 'mozinstall'],
    "exes": {
        'python': PYTHON,
        'virtualenv': [PYTHON, 'c:/mozilla-build/buildbotve/virtualenv.py'],
        'easy_install': ['%s/scripts/python' % VENV_PATH,
                         '%s/scripts/easy_install-2.7-script.py' % VENV_PATH],
        'mozinstall': ['%s/scripts/python' % VENV_PATH,
                       '%s/scripts/mozinstall-script.py' % VENV_PATH],
        'hg': 'c:/mozilla-build/hg/hg',
        'tooltool.py': [PYTHON, 'C:/mozilla-build/tooltool.py'],
    },
    "title": socket.gethostname().split('.')[0],
    "default_actions": [
        "clobber",
        "read-buildbot-config",
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
    "metro_harness_path_frmt": "%(metro_base_path)s/metro/metrotestharness.exe",
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": "win32-minidump_stackwalk.exe",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/win32/releng.manifest",
}
