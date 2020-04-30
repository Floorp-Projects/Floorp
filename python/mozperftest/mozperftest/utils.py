# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import logging
import contextlib
import sys
import os
import random

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


class MachLogger:
    """Wrapper around the mach logger to make logging simpler.
    """

    def __init__(self, mach_cmd):
        self._logger = mach_cmd.log

    @property
    def log(self):
        return self._logger

    def info(self, msg, name="mozperftest", **kwargs):
        self._logger(logging.INFO, name, kwargs, msg)

    def debug(self, msg, name="mozperftest", **kwargs):
        self._logger(logging.DEBUG, name, kwargs, msg)

    def warning(self, msg, name="mozperftest", **kwargs):
        self._logger(logging.WARNING, name, kwargs, msg)

    def error(self, msg, name="mozperftest", **kwargs):
        self._logger(logging.ERROR, name, kwargs, msg)


def install_package(virtualenv_manager, package):
    from pip._internal.req.constructors import install_req_from_line

    req = install_req_from_line(package)
    req.check_if_exists(use_user_site=False)
    # already installed, check if it's in our venv
    if req.satisfied_by is not None:
        venv_site_lib = os.path.abspath(
            os.path.join(virtualenv_manager.bin_path, "..", "lib")
        )
        site_packages = os.path.abspath(req.satisfied_by.location)
        if site_packages.startswith(venv_site_lib):
            # already installed in this venv, we can skip
            return
    virtualenv_manager._run_pip(["install", package])


def build_test_list(tests, randomized=False):
    if isinstance(tests, str):
        tests = [tests]
    res = []
    for test in tests:
        if os.path.isfile(test):
            res.append(test)
        elif os.path.isdir(test):
            for root, dirs, files in os.walk(test):
                for file in files:
                    if not file.startswith("perftest"):
                        continue
                    res.append(os.path.join(root, file))
    if not randomized:
        res.sort()
    else:
        # random shuffling is used to make sure
        # we don't always run tests in the same order
        random.shuffle(res)
    return res
