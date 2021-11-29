# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import hashlib
import os
import sys


class UserError(Exception):
    """Represents an error caused by something the user did wrong rather than
    an internal `mach` failure. Exceptions that are subclasses of this class
    will not be reported as failures to Sentry.
    """


def setenv(key, value):
    """Compatibility shim to ensure the proper string type is used with
    os.environ for the version of Python being used.
    """
    from six import text_type

    encoding = "mbcs" if sys.platform == "win32" else "utf-8"

    if sys.version_info[0] == 2:
        if isinstance(key, text_type):
            key = key.encode(encoding)
        if isinstance(value, text_type):
            value = value.encode(encoding)
    else:
        if isinstance(key, bytes):
            key = key.decode(encoding)
        if isinstance(value, bytes):
            value = value.decode(encoding)

    os.environ[key] = value


def get_state_dir(specific_to_topsrcdir=False, topsrcdir=None):
    """Obtain path to a directory to hold state.

    Args:
        specific_to_topsrcdir (bool): If True, return a state dir specific to the current
            srcdir instead of the global state dir (default: False)

    Returns:
        A path to the state dir (str)
    """
    state_dir = os.environ.get("MOZBUILD_STATE_PATH", os.path.expanduser("~/.mozbuild"))
    if not specific_to_topsrcdir:
        return state_dir

    if not topsrcdir:
        # Only import MozbuildObject if topsrcdir isn't provided. This is to cover
        # the Mach initialization stage, where "mozbuild" isn't in the import scope.
        from mozbuild.base import MozbuildObject

        topsrcdir = os.path.abspath(
            MozbuildObject.from_environment(cwd=os.path.dirname(__file__)).topsrcdir
        )
    # Shortening to 12 characters makes these directories a bit more manageable
    # in a terminal and is more than good enough for this purpose.
    srcdir_hash = hashlib.sha256(topsrcdir.encode("utf-8")).hexdigest()[:12]

    state_dir = os.path.join(
        state_dir, "srcdirs", "{}-{}".format(os.path.basename(topsrcdir), srcdir_hash)
    )

    if not os.path.isdir(state_dir):
        # We create the srcdir here rather than 'mach_initialize.py' so direct
        # consumers of this function don't create the directory inconsistently.
        print("Creating local state directory: %s" % state_dir)
        os.makedirs(state_dir, mode=0o770)
        # Save the topsrcdir that this state dir corresponds to so we can clean
        # it up in the event its srcdir was deleted.
        with open(os.path.join(state_dir, "topsrcdir.txt"), "w") as fh:
            fh.write(topsrcdir)

    return state_dir
