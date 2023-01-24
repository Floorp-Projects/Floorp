# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Config file for firefox ui tests run via TaskCluster.

import os
import sys

config = {
    "virtualenv_path": "venv",
    "exes": {
        "python": sys.executable,
        "hg": os.path.join(os.environ["PROGRAMFILES"], "Mercurial", "hg"),
    },
}
