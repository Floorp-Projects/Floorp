# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os


def get_state_dir():
    """Obtain path to a directory to hold state.

    Returns:
        A path to the state dir (str)
    """
    return os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser('~/.mozbuild'))
