import os
import sys
import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

config = {
    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",

    "default_actions": [
        'clobber',
        'download-and-extract',
        "populate-webroot",
        'create-virtualenv',
        'install',
        'run-tests',
    ],
}
