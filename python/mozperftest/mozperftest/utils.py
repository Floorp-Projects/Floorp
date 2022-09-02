# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import logging
import contextlib
from datetime import datetime, date, timedelta
import sys
import os
from io import StringIO
from collections import defaultdict
from pathlib import Path
import tempfile
import shutil
import importlib
import subprocess
import shlex
import functools

from redo import retry
from requests.packages.urllib3.util.retry import Retry
import requests

RETRY_SLEEP = 10
API_ROOT = "https://firefox-ci-tc.services.mozilla.com/api/index/v1"
MULTI_REVISION_ROOT = f"{API_ROOT}/namespaces"
MULTI_TASK_ROOT = f"{API_ROOT}/tasks"
ON_TRY = "MOZ_AUTOMATION" in os.environ
DOWNLOAD_TIMEOUT = 30


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

    # Ensure that we are looking in the right places for packages. This
    # is required in CI because pip installs in an area that is not in
    # the search path.
    venv_site_lib = str(Path(virtualenv_manager.bin_path, "..", "lib").resolve())
    venv_site_packages = str(
        Path(
            venv_site_lib,
            f"python{sys.version_info.major}.{sys.version_info.minor}",
            "site-packages",
        )
    )
    if venv_site_packages not in sys.path and ON_TRY:
        sys.path.insert(0, venv_site_packages)

    req = install_req_from_line(package)
    req.check_if_exists(use_user_site=False)
    # already installed, check if it's in our venv
    if req.satisfied_by is not None:
        site_packages = os.path.abspath(req.satisfied_by.location)
        if site_packages.startswith(venv_site_lib):
            # already installed in this venv, we can skip
            return True
    with silence():
        try:
            subprocess.check_call(
                [virtualenv_manager.python_path, "-m", "pip", "install", package]
            )
            return True
        except Exception:
            if not ignore_failure:
                raise
    return False


# on try, we create tests packages where tests, like
# xpcshell tests, don't have the same path.
# see - python/mozbuild/mozbuild/action/test_archive.py
# this mapping will map paths when running there.
# The key is the source path, and the value the ci path
_TRY_MAPPING = {Path("netwerk"): Path("xpcshell", "tests", "netwerk")}


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

        p_test = Path(test)
        if ON_TRY and not p_test.resolve().exists():
            # until we have pathlib.Path.is_relative_to() (3.9)
            for src_path, ci_path in _TRY_MAPPING.items():
                src_path, ci_path = str(src_path), str(ci_path)
                if test.startswith(src_path):
                    p_test = Path(test.replace(src_path, ci_path))
                    break

        test = p_test.resolve()

        if test.is_file():
            res.append(str(test))
        elif test.is_dir():
            for file in test.rglob("perftest_*.js"):
                res.append(str(file))
        else:
            raise FileNotFoundError(str(test))
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
        if value is None and key in os.environ:
            del os.environ[key]
        elif value is not None:
            os.environ[key] = value
    try:
        yield
    finally:
        for key, value in old.items():
            if value is None and key in os.environ:
                del os.environ[key]
            elif value is not None:
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


def run_script(cmd, cmd_args=None, verbose=False, display=False, label=None):
    """Used to run a command in a subprocess."""
    if isinstance(cmd, str):
        cmd = shlex.split(cmd)
    try:
        joiner = shlex.join
    except AttributeError:
        # Python < 3.8
        joiner = subprocess.list2cmdline

    if label is None:
        label = joiner(cmd)
    sys.stdout.write(f"=> {label} ")
    if cmd_args is None:
        args = cmd
    else:
        args = cmd + list(cmd_args)
    sys.stdout.flush()
    try:
        if verbose:
            sys.stdout.write(f"\nRunning {' '.join(args)}\n")
            sys.stdout.flush()
        output = subprocess.check_output(args)
        if display:
            sys.stdout.write("\n")
            for line in output.split(b"\n"):
                sys.stdout.write(line.decode("utf8") + "\n")
        sys.stdout.write("[OK]\n")
        sys.stdout.flush()
        return True, output
    except subprocess.CalledProcessError as e:
        for line in e.output.split(b"\n"):
            sys.stdout.write(line.decode("utf8") + "\n")
        sys.stdout.write("[FAILED]\n")
        sys.stdout.flush()
        return False, e


def run_python_script(
    virtualenv_manager,
    module,
    module_args=None,
    verbose=False,
    display=False,
    label=None,
):
    """Used to run a Python script in isolation."""
    if label is None:
        label = module
    cmd = [virtualenv_manager.python_path, "-m", module]
    return run_script(cmd, module_args, verbose=verbose, display=display, label=label)


def checkout_script(cmd, cmd_args=None, verbose=False, display=False, label=None):
    return run_script(cmd, cmd_args, verbose, display, label)[0]


def checkout_python_script(
    virtualenv_manager,
    module,
    module_args=None,
    verbose=False,
    display=False,
    label=None,
):
    return run_python_script(
        virtualenv_manager, module, module_args, verbose, display, label
    )[0]


_URL = (
    "{0}/secrets/v1/secret/project"
    "{1}releng{1}gecko{1}build{1}level-{2}{1}conditioned-profiles"
)
_WPT_URL = "{0}/secrets/v1/secret/project/perftest/gecko/level-{1}/perftest-login"
_DEFAULT_SERVER = "https://firefox-ci-tc.services.mozilla.com"


@functools.lru_cache()
def get_tc_secret(wpt=False):
    """Returns the Taskcluster secret.

    Raises an OSError when not running on try
    """
    if not ON_TRY:
        raise OSError("Not running in Taskcluster")
    session = requests.Session()
    retry = Retry(total=5, backoff_factor=0.1, status_forcelist=[500, 502, 503, 504])
    http_adapter = requests.adapters.HTTPAdapter(max_retries=retry)
    session.mount("https://", http_adapter)
    session.mount("http://", http_adapter)
    secrets_url = _URL.format(
        os.environ.get("TASKCLUSTER_PROXY_URL", _DEFAULT_SERVER),
        "%2F",
        os.environ.get("MOZ_SCM_LEVEL", "1"),
    )
    if wpt:
        secrets_url = _WPT_URL.format(
            os.environ.get("TASKCLUSTER_PROXY_URL", _DEFAULT_SERVER),
            os.environ.get("MOZ_SCM_LEVEL", "1"),
        )
    res = session.get(secrets_url, timeout=DOWNLOAD_TIMEOUT)
    res.raise_for_status()
    return res.json()["secret"]


def get_output_dir(output, folder=None):
    if output is None:
        raise Exception("Output path was not provided.")

    result_dir = Path(output)
    if folder is not None:
        result_dir = Path(result_dir, folder)

    result_dir.mkdir(parents=True, exist_ok=True)
    result_dir = result_dir.resolve()

    return result_dir
