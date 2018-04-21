# Config file for firefox ui tests run via TaskCluster.

import os
import sys


config = {
    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",
}
