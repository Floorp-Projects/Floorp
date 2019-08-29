# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

from mozlint import result
from mozlint.pathutils import expand_exclusions

results = []


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs['root']))
    for f in files:
        if os.access(f, os.X_OK):
            with open(f, 'r+') as content:
                # Some source files have +x permissions
                line = content.readline()
                if line.startswith("#!"):
                    # Check if the file doesn't start with a shebang
                    # if it does, not a warning
                    continue

            if fix:
                # We want to fix it, do it and leave
                os.chmod(f, 0o644)
                continue

            res = {'path': f,
                   'message': "Execution permissions on a source file",
                   'level': 'error'
                   }
            results.append(result.from_config(config, **res))
    return results
