import os

PYTHON = '/tools/buildbot/bin/python'
VENV_PATH = '%s/build/venv' % os.getcwd()

config = {
    "log_name": "talos",
    "buildbot_json_path": "buildprops.json",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    "use_talos_json": True,
    "exes": {
        'python': PYTHON,
        'virtualenv': [PYTHON, '/tools/misc-python/virtualenv.py'],
    },
    "title": os.uname()[1].lower().split('.')[0],
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
    "webroot": '%s/../talos-data' % os.getcwd(),
    "populate_webroot": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
}
