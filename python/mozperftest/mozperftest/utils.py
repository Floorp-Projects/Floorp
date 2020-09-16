# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import logging
import contextlib
from datetime import datetime, date, timedelta
import sys
import os
from io import StringIO
from redo import retry
import requests
from collections import defaultdict
from pathlib import Path
import tempfile
import shutil
import importlib


RETRY_SLEEP = 10
API_ROOT = "https://firefox-ci-tc.services.mozilla.com/api/index/v1"
MULTI_REVISION_ROOT = f"{API_ROOT}/namespaces"
MULTI_TASK_ROOT = f"{API_ROOT}/tasks"
ON_TRY = "MOZ_AUTOMATION" in os.environ


@contextlib.contextmanager
def silence(layer=None):
    if layer is None:
        to_patch = (MachLogger,)
    else:
        to_patch = (MachLogger, layer)

    meths = ("info", "debug", "warning", "error", "log")
    patched = defaultdict(dict)

    oldout, olderr = sys.stdout, sys.stderr
    sys.stdout, sys.stderr = StringIO(), StringIO()

    def _vacuum(*args, **kw):
        sys.stdout.write(str(args))

    for obj in to_patch:
        for meth in meths:
            if not hasattr(obj, meth):
                continue
            patched[obj][meth] = getattr(obj, meth)
            setattr(obj, meth, _vacuum)

    stdout = stderr = None
    try:
        sys.stdout.buffer = sys.stdout
        sys.stderr.buffer = sys.stderr
        sys.stdout.fileno = sys.stderr.fileno = lambda: -1
        try:
            yield sys.stdout, sys.stderr
        except Exception:
            sys.stdout.seek(0)
            stdout = sys.stdout.read()
            sys.stderr.seek(0)
            stderr = sys.stderr.read()
            raise
    finally:
        sys.stdout, sys.stderr = oldout, olderr
        for obj, meths in patched.items():
            for name, old_func in meths.items():
                try:
                    setattr(obj, name, old_func)
                except Exception:
                    pass
        if stdout is not None:
            print(stdout)
        if stderr is not None:
            print(stderr)


def simple_platform():
    plat = host_platform()

    if plat.startswith("win"):
        return "win"
    elif plat.startswith("linux"):
        return "linux"
    else:
        return "mac"


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

    raise ValueError(f"platform not yet supported: {sys.platform}")


class MachLogger:
    """Wrapper around the mach logger to make logging simpler."""

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


def install_package(virtualenv_manager, package, ignore_failure=False):
    """Installs a package using the virtualenv manager.

    Makes sure the package is really installed when the user already has it
    in their local installation.

    Returns True on success, or re-raise the error. If ignore_failure
    is set to True, ignore the error and return False
    """
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
            return True
    with silence():
        try:
            virtualenv_manager._run_pip(["install", package])
            return True
        except Exception:
            if not ignore_failure:
                raise
    return False


def build_test_list(tests):
    """Collects tests given a list of directories, files and URLs.

    Returns a tuple containing the list of tests found and a temp dir for tests
    that were downloaded from an URL.
    """
    temp_dir = None

    if isinstance(tests, str):
        tests = [tests]
    res = []
    for test in tests:
        if test.startswith("http"):
            if temp_dir is None:
                temp_dir = tempfile.mkdtemp()
            target = Path(temp_dir, test.split("/")[-1])
            download_file(test, target)
            res.append(str(target))
            continue

        test = Path(test)

        if test.is_file():
            res.append(str(test))
        elif test.is_dir():
            for file in test.rglob("perftest_*.js"):
                res.append(str(file))
    res.sort()
    return res, temp_dir


def download_file(url, target, retry_sleep=RETRY_SLEEP, attempts=3):
    """Downloads a file, given an URL in the target path.

    The function will attempt several times on failures.
    """

    def _download_file(url, target):
        req = requests.get(url, stream=True, timeout=30)
        target_dir = target.parent.resolve()
        if str(target_dir) != "":
            target_dir.mkdir(exist_ok=True)

        with target.open("wb") as f:
            for chunk in req.iter_content(chunk_size=1024):
                if not chunk:
                    continue
                f.write(chunk)
                f.flush()
        return target

    return retry(
        _download_file,
        args=(url, target),
        attempts=attempts,
        sleeptime=retry_sleep,
        jitter=0,
    )


@contextlib.contextmanager
def temporary_env(**env):
    old = {}
    for key, value in env.items():
        old[key] = os.environ.get(key)
        os.environ[key] = value
    try:
        yield
    finally:
        for key, value in old.items():
            if value is None:
                del os.environ[key]
            else:
                os.environ[key] = value


def convert_day(day):
    if day in ("yesterday", "today"):
        curr = date.today()
        if day == "yesterday":
            curr = curr - timedelta(1)
        day = curr.strftime("%Y.%m.%d")
    else:
        # verify that the user provided string is in the expected format
        # if it can't parse it, it'll raise a value error
        datetime.strptime(day, "%Y.%m.%d")

    return day


def get_revision_namespace_url(route, day="yesterday"):
    """Builds a URL to obtain all the namespaces of a given build route for a single day."""
    day = convert_day(day)
    return f"""{MULTI_REVISION_ROOT}/{route}.{day}.revision"""


def get_multi_tasks_url(route, revision, day="yesterday"):
    """Builds a URL to obtain all the tasks of a given build route for a single day.

    If previous is true, then we get builds from the previous day,
    otherwise, we look at the current day.
    """
    day = convert_day(day)
    return f"""{MULTI_TASK_ROOT}/{route}.{day}.revision.{revision}"""


def strtobool(val):
    if isinstance(val, (bool, int)):
        return bool(val)
    if not isinstance(bool, str):
        raise ValueError(val)
    val = val.lower()
    if val in ("y", "yes", "t", "true", "on", "1"):
        return 1
    elif val in ("n", "no", "f", "false", "off", "0"):
        return 0
    else:
        raise ValueError("invalid truth value %r" % (val,))


@contextlib.contextmanager
def temp_dir():
    tempdir = tempfile.mkdtemp()
    try:
        yield tempdir
    finally:
        shutil.rmtree(tempdir)


def load_class(path):
    """Loads a class given its path and returns it.

    The path is a string of the form `package.module:class` that points
    to the class to be imported.

    If if can't find it, or if the path is malformed,
    an ImportError is raised.
    """
    if ":" not in path:
        raise ImportError(f"Malformed path '{path}'")
    elmts = path.split(":")
    if len(elmts) != 2:
        raise ImportError(f"Malformed path '{path}'")
    mod_name, klass_name = elmts
    try:
        mod = importlib.import_module(mod_name)
    except ModuleNotFoundError:
        raise ImportError(f"Can't find '{mod_name}'")
    try:
        klass = getattr(mod, klass_name)
    except AttributeError:
        raise ImportError(f"Can't find '{klass_name}' in '{mod_name}'")
    return klass
