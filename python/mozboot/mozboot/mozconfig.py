# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import filecmp
import os


MOZ_MYCONFIG_ERROR = """
The MOZ_MYCONFIG environment variable to define the location of mozconfigs
is deprecated. If you wish to define the mozconfig path via an environment
variable, use MOZCONFIG instead.
""".strip()

MOZCONFIG_LEGACY_PATH_ERROR = """
You currently have a mozconfig at %s. This implicit location is no longer
supported. Please move it to %s/.mozconfig or set an explicit path
via the $MOZCONFIG environment variable.
""".strip()

DEFAULT_TOPSRCDIR_PATHS = (".mozconfig", "mozconfig")
DEPRECATED_TOPSRCDIR_PATHS = ("mozconfig.sh", "myconfig.sh")
DEPRECATED_HOME_PATHS = (".mozconfig", ".mozconfig.sh", ".mozmyconfig.sh")


class MozconfigFindException(Exception):
    """Raised when a mozconfig location is not defined properly."""


class MozconfigBuilder(object):
    def __init__(self):
        self._lines = []

    def append(self, block):
        self._lines.extend([line.strip() for line in block.split("\n") if line.strip()])

    def generate(self):
        return "".join(line + "\n" for line in self._lines)


def find_mozconfig(topsrcdir, env=os.environ):
    """Find the active mozconfig file for the current environment.

    This emulates the logic in mozconfig-find.

    1) If ENV[MOZCONFIG] is set, use that
    2) If $TOPSRCDIR/mozconfig or $TOPSRCDIR/.mozconfig exists, use it.
    3) If both exist or if there are legacy locations detected, error out.

    The absolute path to the found mozconfig will be returned on success.
    None will be returned if no mozconfig could be found. A
    MozconfigFindException will be raised if there is a bad state,
    including conditions from #3 above.
    """
    # Check for legacy methods first.
    if "MOZ_MYCONFIG" in env:
        raise MozconfigFindException(MOZ_MYCONFIG_ERROR)

    env_path = env.get("MOZCONFIG", None) or None
    if env_path is not None:
        if not os.path.isabs(env_path):
            potential_roots = [topsrcdir, os.getcwd()]
            # Attempt to eliminate duplicates for e.g.
            # self.topsrcdir == os.curdir.
            potential_roots = set(os.path.abspath(p) for p in potential_roots)
            existing = [
                root
                for root in potential_roots
                if os.path.exists(os.path.join(root, env_path))
            ]
            if len(existing) > 1:
                # There are multiple files, but we might have a setup like:
                #
                # somedirectory/
                #   srcdir/
                #   objdir/
                #
                # MOZCONFIG=../srcdir/some/path/to/mozconfig
                #
                # and be configuring from the objdir.  So even though we
                # have multiple existing files, they are actually the same
                # file.
                mozconfigs = [os.path.join(root, env_path) for root in existing]
                if not all(
                    map(
                        lambda p1, p2: filecmp.cmp(p1, p2, shallow=False),
                        mozconfigs[:-1],
                        mozconfigs[1:],
                    )
                ):
                    raise MozconfigFindException(
                        "MOZCONFIG environment variable refers to a path that "
                        + "exists in more than one of "
                        + ", ".join(potential_roots)
                        + ". Remove all but one."
                    )
            elif not existing:
                raise MozconfigFindException(
                    "MOZCONFIG environment variable refers to a path that "
                    + "does not exist in any of "
                    + ", ".join(potential_roots)
                )

            env_path = os.path.join(existing[0], env_path)
        elif not os.path.exists(env_path):  # non-relative path
            raise MozconfigFindException(
                "MOZCONFIG environment variable refers to a path that "
                "does not exist: " + env_path
            )

        if not os.path.isfile(env_path):
            raise MozconfigFindException(
                "MOZCONFIG environment variable refers to a " "non-file: " + env_path
            )

    srcdir_paths = [os.path.join(topsrcdir, p) for p in DEFAULT_TOPSRCDIR_PATHS]
    existing = [p for p in srcdir_paths if os.path.isfile(p)]

    if env_path is None and len(existing) > 1:
        raise MozconfigFindException(
            "Multiple default mozconfig files "
            "present. Remove all but one. " + ", ".join(existing)
        )

    path = None

    if env_path is not None:
        path = env_path
    elif len(existing):
        assert len(existing) == 1
        path = existing[0]

    if path is not None:
        return os.path.abspath(path)

    deprecated_paths = [os.path.join(topsrcdir, s) for s in DEPRECATED_TOPSRCDIR_PATHS]

    home = env.get("HOME", None)
    if home is not None:
        deprecated_paths.extend([os.path.join(home, s) for s in DEPRECATED_HOME_PATHS])

    for path in deprecated_paths:
        if os.path.exists(path):
            raise MozconfigFindException(
                MOZCONFIG_LEGACY_PATH_ERROR % (path, topsrcdir)
            )

    return None
