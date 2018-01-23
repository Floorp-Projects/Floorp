# Config file for firefox ui tests run via TaskCluster.

import os
import sys


config = {
    "virtualenv_python_dll": os.path.join(os.path.dirname(sys.executable), 'python27.dll'),
    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
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
}
