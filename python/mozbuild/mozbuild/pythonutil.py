# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys


def iter_modules_in_path(*paths):
    paths = [os.path.abspath(os.path.normcase(p)) + os.sep
             for p in paths]
    for name, module in sys.modules.items():
        if not hasattr(module, '__file__'):
            continue

        path = module.__file__

        if path.endswith('.pyc'):
            path = path[:-1]
        path = os.path.abspath(os.path.normcase(path))

        if any(path.startswith(p) for p in paths):
            yield path
