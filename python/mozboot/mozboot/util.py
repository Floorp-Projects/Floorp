# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import hashlib
import os

here = os.path.join(os.path.dirname(__file__))


def get_state_dir(srcdir=False):
    """Obtain path to a directory to hold state.

    Args:
        srcdir (bool): If True, return a state dir specific to the current
            srcdir instead of the global state dir (default: False)

    Returns:
        A path to the state dir (str)
    """
    state_dir = os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser('~/.mozbuild'))
    if not srcdir:
        return state_dir

    # This function can be called without the build virutualenv, and in that
    # case srcdir is supposed to be False. Import mozbuild here to avoid
    # breaking that usage.
    from mozbuild.base import MozbuildObject

    srcdir = os.path.abspath(MozbuildObject.from_environment(cwd=here).topsrcdir)
    # Shortening to 12 characters makes these directories a bit more manageable
    # in a terminal and is more than good enough for this purpose.
    srcdir_hash = hashlib.sha256(srcdir).hexdigest()[:12]

    state_dir = os.path.join(state_dir, 'srcdirs', '{}-{}'.format(
        os.path.basename(srcdir), srcdir_hash))

    if not os.path.isdir(state_dir):
        # We create the srcdir here rather than 'mach_bootstrap.py' so direct
        # consumers of this function don't create the directory inconsistently.
        print('Creating local state directory: %s' % state_dir)
        os.makedirs(state_dir, mode=0o770)
        # Save the topsrcdir that this state dir corresponds to so we can clean
        # it up in the event its srcdir was deleted.
        with open(os.path.join(state_dir, 'topsrcdir.txt'), 'w') as fh:
            fh.write(srcdir)

    return state_dir
