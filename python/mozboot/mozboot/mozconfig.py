# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import filecmp
import os
from pathlib import Path
from typing import Union

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


def find_mozconfig(topsrcdir: Union[str, Path], env=os.environ):
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
    topsrcdir = Path(topsrcdir)

    # Check for legacy methods first.
    if "MOZ_MYCONFIG" in env:
        raise MozconfigFindException(MOZ_MYCONFIG_ERROR)

    env_path = env.get("MOZCONFIG", None) or None

    if env_path is not None:
        env_path = Path(env_path)

    if env_path is not None:
        if not env_path.is_absolute():
            potential_roots = [topsrcdir, Path.cwd()]
            # Attempt to eliminate duplicates for e.g.
            # self.topsrcdir == Path.cwd().
            potential_roots_strings = set(str(p.resolve()) for p in potential_roots)
            existing = [
                root
                for root in potential_roots_strings
                if (Path(root) / env_path).exists()
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
                mozconfigs = [root / env_path for root in existing]
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
                        + ", ".join(potential_roots_strings)
                        + ". Remove all but one."
                    )
            elif not existing:
                raise MozconfigFindException(
                    "MOZCONFIG environment variable refers to a path that "
                    + "does not exist in any of "
                    + ", ".join(potential_roots_strings)
                )

            env_path = existing[0] / env_path
        elif not env_path.exists():  # non-relative path
            raise MozconfigFindException(
                "MOZCONFIG environment variable refers to a path that "
                f"does not exist: {env_path}"
            )

        if not env_path.is_file():
            raise MozconfigFindException(
                "MOZCONFIG environment variable refers to a " f"non-file: {env_path}"
            )

    srcdir_paths = [topsrcdir / p for p in DEFAULT_TOPSRCDIR_PATHS]
    existing = [p for p in srcdir_paths if p.is_file()]

    if env_path is None and len(existing) > 1:
        raise MozconfigFindException(
            "Multiple default mozconfig files "
            "present. Remove all but one. " + ", ".join(str(p) for p in existing)
        )

    path = None

    if env_path is not None:
        path = env_path
    elif len(existing):
        assert len(existing) == 1
        path = existing[0]

    if path is not None:
        return Path.cwd() / path

    deprecated_paths = [topsrcdir / s for s in DEPRECATED_TOPSRCDIR_PATHS]

    home = env.get("HOME", None)
    if home is not None:
        home = Path(home)
        deprecated_paths.extend([home / s for s in DEPRECATED_HOME_PATHS])

    for path in deprecated_paths:
        if path.exists():
            raise MozconfigFindException(
                MOZCONFIG_LEGACY_PATH_ERROR % (path, topsrcdir)
            )

    return None
