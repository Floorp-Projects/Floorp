# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import contextlib
import sys
from six import StringIO


@contextlib.contextmanager
def silence():
    oldout, olderr = sys.stdout, sys.stderr
    try:
        sys.stdout, sys.stderr = StringIO(), StringIO()
        yield
    finally:
        sys.stdout, sys.stderr = oldout, olderr


def host_platform():
    is_64bits = sys.maxsize > 2 ** 32

    if sys.platform.startswith("win"):
        if is_64bits:
            return "win64"
    elif sys.platform.startswith("linux"):
        if is_64bits:
            return "linux64"
    elif sys.platform.startswith("darwin"):
        return "darwin"

    raise ValueError("sys.platform is not yet supported: {}".format(sys.platform))


def add_option(metadata, name, value):
    options = metadata.get_arg("extra_options", "")
    options += ",%s=%s" % (name, value)
    metadata.set_arg("extra_options", options)


def add_options(metadata, options):
    for name, value in options:
        add_option(metadata, name, value)
