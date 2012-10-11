# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os


def parse_test_path(test_path, topsrcdir):
    """Returns a dict containing:
    * 'normalized': the normalized path, relative to the topsrcdir
    * 'isdir': whether the path points to a directory
    """
    is_dir = os.path.isdir(test_path)

    if is_dir and not test_path.endswith(os.path.sep):
        test_path += os.path.sep

    normalized = test_path

    if test_path.startswith(topsrcdir):
        normalized = test_path[len(topsrcdir):]

    return {
        'normalized': normalized,
        'is_dir': is_dir,
    }

