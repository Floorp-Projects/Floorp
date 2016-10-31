import os
import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    # Python env
    "virtualenv_path": 'venv',
    "exes": {
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
    },

    # PIP
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    #mozcrash support
    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-media-tests',
    ],

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
