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
    "use_talos_json": True,
    "virtualenv_modules": ['pywin32', 'talos', 'mozinstall'],
    "exes": {
        'python': PYTHON,
        'virtualenv': [PYTHON, 'c:/mozilla-build/buildbotve/virtualenv.py'],
        'easy_install': ['%s/scripts/python' % VENV_PATH,
                         '%s/scripts/easy_install-2.7-script.py' % VENV_PATH],
        'mozinstall': ['%s/scripts/python' % VENV_PATH,
                       '%s/scripts/mozinstall-script.py' % VENV_PATH],
        'hg': 'c:/mozilla-build/hg/hg',
    },
    "title": socket.gethostname().split('.')[0],
    "results_url": "http://graphs.mozilla.org/server/collect.cgi",
    "default_actions": [
        "clobber",
        "read-buildbot-config",
        "download-and-extract",
        "clone-talos",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "python_webserver": False,
    "webroot": 'c:/slave/talos-data',
    "populate_webroot": True,
    # Srsly gly? Ys
    "webroot_extract_cmd": r'''c:/mozilla-build/msys/bin/bash -c "PATH=/c/mozilla-build/msys/bin:$PATH tar zx --strip-components=1 -f '%(tarball)s' --wildcards '**/talos/'"''',
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "metro_harness_path_frmt": "%(metro_base_path)s/metro/metrotestharness.exe",
}
