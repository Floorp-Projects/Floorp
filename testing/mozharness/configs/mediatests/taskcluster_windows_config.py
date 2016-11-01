import os
import sys
import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    "virtualenv_python_dll": os.path.join(os.path.dirname(sys.executable), 'python27.dll'),
    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
        'virtualenv': [
            sys.executable,
            os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages', 'virtualenv.py')
        ],
        'mozinstall': ['build/venv/scripts/python', 'build/venv/scripts/mozinstall-script.py'],
        'tooltool.py': [sys.executable, os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')],
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },
    "proxxy": {},
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

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
