import os
import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    "virtualenv_path": 'venv',
    "exes": {
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
        'gittool.py': os.path.join(external_tools_path, 'gittool.py'),
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
    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",

    "firefox_media_repo": 'https://github.com/mjzffr/firefox-media-tests.git',
    "firefox_media_branch": 'master',
    "firefox_media_rev": '0830e972e4b95fef3507207fc6bce028da27f2d3',

    "suite_definitions": {
        "media-tests": {
            "options": [],
        },
        "media-youtube-tests": {
            "options": [
                "%(test_manifest)s"
            ],
        },
    },
}
