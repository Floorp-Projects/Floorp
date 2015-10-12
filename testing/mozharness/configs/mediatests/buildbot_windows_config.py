import os
import sys
import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    "virtualenv_python_dll": 'c:/mozilla-build/python27/python27.dll',
    "virtualenv_path": 'venv',
    "exes": {
        'python': 'c:/mozilla-build/python27/python',
        'virtualenv': ['c:/mozilla-build/python27/python', 'c:/mozilla-build/buildbotve/virtualenv.py'],
        'hg': 'c:/mozilla-build/hg/hg',
        'mozinstall': ['%s/build/venv/scripts/python' % os.getcwd(),
                       '%s/build/venv/scripts/mozinstall-script.py' % os.getcwd()],
        'tooltool.py': [sys.executable, 'C:/mozilla-build/tooltool.py'],
        'gittool.py': [sys.executable,
                       os.path.join(external_tools_path, 'gittool.py')],
        'hgtool.py': [sys.executable,
                      os.path.join(external_tools_path, 'hgtool.py')],


    },

    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "buildbot_json_path": "buildprops.json",

    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'checkout',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-media-tests',
    ],
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),
    "in_tree_config": "config/mozharness/marionette.py",
    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",

    "firefox_media_repo": 'https://github.com/mjzffr/firefox-media-tests.git',
    "firefox_media_branch": 'master',
    "firefox_media_rev": 'b11d6c3d7f6af166be314d2ac6673e78c1edb566',
    "firefox_ui_repo": 'https://github.com/mozilla/firefox-ui-tests.git',
    "firefox_ui_branch": 'mozilla-central',
    "firefox_ui_rev": '6d6d57917f85399e903ac69b7e4297091b2d474c',

    "suite_definitions": {
        "media-tests": {
            "options": [],
        },
        "media-youtube-tests": {
            "options": [
                "--tests=%(test_manifest)s"
            ],
        },
    },
}
