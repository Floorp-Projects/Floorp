# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess
import sys

from distutils.version import (
    StrictVersion,
)


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


def python_executable_version(exe):
    """Determine the version of a Python executable by invoking it.

    May raise ``subprocess.CalledProcessError`` or ``ValueError`` on failure.
    """
    program = "import sys; print('.'.join(map(str, sys.version_info[0:3])))"
    out = subprocess.check_output([exe, '-c', program]).rstrip()
    return StrictVersion(out)


def find_python3_executable(min_version='3.5.0'):
    """Find a Python 3 executable.

    Returns a tuple containing the the path to an executable binary and a
    version tuple. Both tuple entries will be None if a Python executable
    could not be resolved.
    """
    import which

    if not min_version.startswith('3.'):
        raise ValueError('min_version expected a 3.x string, got %s' %
                         min_version)

    min_version = StrictVersion(min_version)

    if sys.version_info.major >= 3:
        our_version = StrictVersion('%s.%s.%s' % (sys.version_info[0:3]))

        if our_version >= min_version:
            # This will potentially return a virtualenv Python. It's probably
            # OK for now...
            return sys.executable, our_version.version

        # Else fall back to finding another binary.

    # https://www.python.org/dev/peps/pep-0394/ defines how the Python binary
    # should be named. `python3` should refer to some Python 3. `python` may
    # refer to a Python 2 or 3. `pythonX.Y` may exist.
    #
    # Since `python` is ambiguous and `python3` should always exist, we
    # ignore `python` here. We instead look for the preferred `python3` first
    # and fall back to `pythonX.Y` if it isn't found or doesn't meet our
    # version requirements.
    names = ['python3']

    # Look for `python3.Y` down to our minimum version.
    for minor in range(9, min_version.version[1] - 1, -1):
        names.append('python3.%d' % minor)

    for name in names:
        try:
            exe = which.which(name)
        except which.WhichError:
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
