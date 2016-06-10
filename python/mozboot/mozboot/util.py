# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os


def get_state_dir():
    """Obtain path to a directory to hold state."""
    state_user_dir = os.path.expanduser('~/.mozbuild')
    state_env_dir = os.environ.get('MOZBUILD_STATE_PATH')
    return state_env_dir or state_user_dir
