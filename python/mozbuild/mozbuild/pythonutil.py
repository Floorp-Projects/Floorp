# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import six
import subprocess
import sys

from distutils.version import StrictVersion


def iter_modules_in_path(*paths):
    paths = [os.path.abspath(os.path.normcase(p)) + os.sep for p in paths]
    for name, module in sys.modules.items():
        if getattr(module, "__file__", None) is None:
            continue
        if module.__file__ is None:
            continue
        path = module.__file__

        if path.endswith(".pyc"):
            path = path[:-1]
        path = os.path.abspath(os.path.normcase(path))

        if any(path.startswith(p) for p in paths):
            yield path


def python_executable_version(exe):
    """Determine the version of a Python executable by invoking it.

    May raise ``subprocess.CalledProcessError`` or ``ValueError`` on failure.
    """
    program = "import sys; print('.'.join(map(str, sys.version_info[0:3])))"
    out = six.ensure_text(
        subprocess.check_output([exe, "-c", program], universal_newlines=True)
    ).rstrip()
    return StrictVersion(out)


def _find_python_executable(major):
    if major not in (2, 3):
        raise ValueError("Expected a Python major version of 2 or 3")
    min_versions = {2: "2.7.0", 3: "3.6.0"}

    def ret(min_version=min_versions[major]):
        from mozfile import which

        prefix = min_version[0] + "."
        if not min_version.startswith(prefix):
            raise ValueError(
                "min_version expected a %sx string, got %s" % (prefix, min_version)
            )

        min_version = StrictVersion(min_version)
        major = min_version.version[0]

        if sys.version_info.major == major:
            our_version = StrictVersion("%s.%s.%s" % (sys.version_info[0:3]))

            if our_version >= min_version:
                # This will potentially return a virtualenv Python. It's probably
                # OK for now...
                return sys.executable, our_version.version

            # Else fall back to finding another binary.

        # https://www.python.org/dev/peps/pep-0394/ defines how the Python binary
        # should be named. `python3` should refer to some Python 3, and
        # `python2` to some Python 2. `python` may refer to a Python 2 or 3.
        # `pythonX.Y` may exist.
        #
        # Since `python` is ambiguous and `pythonX` should always exist, we
        # ignore `python` here. We instead look for the preferred `pythonX` first
        # and fall back to `pythonX.Y` if it isn't found or doesn't meet our
        # version requirements.
        names = ["python%d" % major]

        # Look for `pythonX.Y` down to our minimum version.
        for minor in range(9, min_version.version[1] - 1, -1):
            names.append("python%d.%d" % (major, minor))

        for name in names:
            exe = which(name)
            if not exe:
                continue

            # We always verify we can invoke the executable and its version is
            # sane.
            try:
                version = python_executable_version(exe)
            except (subprocess.CalledProcessError, ValueError):
                continue

            if version >= min_version:
                return exe, version.version

        return None, None

    return ret


find_python3_executable = _find_python_executable(3)
