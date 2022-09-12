from __future__ import absolute_import, print_function

import logging
import os
import pathlib
import sys
from collections import defaultdict

from mozbuild.base import MozbuildObject
from mozlint.pathutils import findobject
from mozlint.parser import Parser
from mozlint.result import ResultSummary
from mozlog.structuredlog import StructuredLogger
from mozpack import path

import pytest

here = path.abspath(path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here, virtualenv_name="python-test")

lintdir = path.dirname(here)
sys.path.insert(0, lintdir)
logger = logging.getLogger("mozlint")


def pytest_generate_tests(metafunc):
    """Finds, loads and returns the config for the linter name specified by the
    LINTER global variable in the calling module.

    This implies that each test file (that uses this fixture) should only be
    used to test a single linter. If no LINTER variable is defined, the test
    will fail.
    """
    if "config" in metafunc.fixturenames:
        if not hasattr(metafunc.module, "LINTER"):
            pytest.fail(
                "'config' fixture used from a module that didn't set the LINTER variable"
            )

        name = metafunc.module.LINTER
        config_path = path.join(lintdir, "{}.yml".format(name))
        parser = Parser(build.topsrcdir)
        configs = parser.parse(config_path)
        config_names = {config["name"] for config in configs}

        marker = metafunc.definition.get_closest_marker("lint_config")
        if marker:
            config_name = marker.kwargs["name"]
            if config_name not in config_names:
                pytest.fail(f"lint config {config_name} not present in {name}.yml")
            configs = [
                config for config in configs if config["name"] == marker.kwargs["name"]
            ]

        ids = [config["name"] for config in configs]
        metafunc.parametrize("config", configs, ids=ids)


@pytest.fixture(scope="module")
def root(request):
    """Return the root directory for the files of the linter under test.

    For example, with LINTER=flake8 this would be tools/lint/test/files/flake8.
    """
    if not hasattr(request.module, "LINTER"):
        pytest.fail(
            "'root' fixture used from a module that didn't set the LINTER variable"
        )
    return path.join(here, "files", request.module.LINTER)


@pytest.fixture(scope="module")
def paths(root):
    """Return a function that can resolve file paths relative to the linter
    under test.

    Can be used like `paths('foo.py', 'bar/baz')`. This will return a list of
    absolute paths under the `root` files directory.
    """

    def _inner(*paths):
        if not paths:
            return [root]
        return [path.normpath(path.join(root, p)) for p in paths]

    return _inner


@pytest.fixture(autouse=True)
def run_setup(config):
    """Make sure that if the linter named in the LINTER global variable has a
    setup function, it gets called before running the tests.
    """
    if "setup" not in config:
        return

    if config["name"] == "clang-format":
        # Skip the setup for the clang-format linter, as it requires a Mach context
        # (which we may not have if pytest is invoked directly).
        return

    log = logging.LoggerAdapter(
        logger, {"lintname": config.get("name"), "pid": os.getpid()}
    )

    func = findobject(config["setup"])
    func(
        build.topsrcdir,
        virtualenv_manager=build.virtualenv_manager,
        virtualenv_bin_path=build.virtualenv_manager.bin_path,
        log=log,
    )


@pytest.fixture
def lint(config, root, request):
    """Find and return the 'lint' function for the external linter named in the
    LINTER global variable.

    This will automatically pass in the 'config' and 'root' arguments if not
    specified.
    """
    try:
        func = findobject(config["payload"])
    except (ImportError, ValueError):
        pytest.fail(
            "could not resolve a lint function from '{}'".format(config["payload"])
        )

    ResultSummary.root = root

    def wrapper(paths, config=config, root=root, collapse_results=False, **lintargs):
        logger.setLevel(logging.DEBUG)
        lintargs["log"] = logging.LoggerAdapter(
            logger, {"lintname": config.get("name"), "pid": os.getpid()}
        )

        results = func(paths, config, root=root, **lintargs)
        if hasattr(request.module, "fixed") and isinstance(results, dict):
            request.module.fixed += results["fixed"]

        if isinstance(results, dict):
            results = results["results"]

        if isinstance(results, (list, tuple)):
            results = sorted(results)

        if not collapse_results:
            return results

        ret = defaultdict(list)
        for r in results:
            ret[r.relpath].append(r)
        return ret

    return wrapper


@pytest.fixture
def structuredlog_lint(config, root, logger=None):
    """Find and return the 'lint' function for the external linter named in the
    LINTER global variable. This variant of the lint function is for linters that
    use the 'structuredlog' type.

    This will automatically pass in the 'config' and 'root' arguments if not
    specified.
    """
    try:
        func = findobject(config["payload"])
    except (ImportError, ValueError):
        pytest.fail(
            "could not resolve a lint function from '{}'".format(config["payload"])
        )

    ResultSummary.root = root

    if not logger:
        logger = structured_logger()

    def wrapper(
        paths,
        config=config,
        root=root,
        logger=logger,
        collapse_results=False,
        **lintargs,
    ):
        lintargs["log"] = logging.LoggerAdapter(
            logger, {"lintname": config.get("name"), "pid": os.getpid()}
        )
        results = func(paths, config, root=root, logger=logger, **lintargs)
        if not collapse_results:
            return results

        ret = defaultdict(list)
        for r in results:
            ret[r.path].append(r)
        return ret

    return wrapper


@pytest.fixture
def create_temp_file(tmpdir):
    def inner(contents, name=None):
        name = name or "temp.py"
        path = tmpdir.join(name)
        path.write(contents)
        return path.strpath

    return inner


@pytest.fixture
def structured_logger():
    return StructuredLogger("logger")


@pytest.fixture
def perfdocs_sample():
    from test_perfdocs import (
        SAMPLE_TEST,
        SAMPLE_CONFIG,
        DYNAMIC_SAMPLE_CONFIG,
        SAMPLE_INI,
        temp_dir,
        temp_file,
    )

    with temp_dir() as tmpdir:
        suite_dir = pathlib.Path(tmpdir, "suite")
        raptor_dir = pathlib.Path(tmpdir, "raptor")
        raptor_suitedir = pathlib.Path(tmpdir, "raptor", "suite")
        raptor_another_suitedir = pathlib.Path(tmpdir, "raptor", "another_suite")
        perfdocs_dir = pathlib.Path(tmpdir, "perfdocs")

        perfdocs_dir.mkdir(parents=True, exist_ok=True)
        suite_dir.mkdir(parents=True, exist_ok=True)
        raptor_dir.mkdir(parents=True, exist_ok=True)
        raptor_suitedir.mkdir(parents=True, exist_ok=True)
        raptor_another_suitedir.mkdir(parents=True, exist_ok=True)

        with temp_file(
            "perftest.ini", tempdir=suite_dir, content="[perftest_sample.js]"
        ) as tmpmanifest, temp_file(
            "raptor_example1.ini", tempdir=raptor_suitedir, content=SAMPLE_INI
        ) as tmpexample1manifest, temp_file(
            "raptor_example2.ini", tempdir=raptor_another_suitedir, content=SAMPLE_INI
        ) as tmpexample2manifest, temp_file(
            "perftest_sample.js", tempdir=suite_dir, content=SAMPLE_TEST
        ) as tmptest, temp_file(
            "config.yml", tempdir=perfdocs_dir, content=SAMPLE_CONFIG
        ) as tmpconfig, temp_file(
            "config_2.yml", tempdir=perfdocs_dir, content=DYNAMIC_SAMPLE_CONFIG
        ) as tmpconfig_2, temp_file(
            "index.rst", tempdir=perfdocs_dir, content="{documentation}"
        ) as tmpindex:
            yield {
                "top_dir": tmpdir,
                "manifest": tmpmanifest,
                "example1_manifest": tmpexample1manifest,
                "example2_manifest": tmpexample2manifest,
                "test": tmptest,
                "config": tmpconfig,
                "config_2": tmpconfig_2,
                "index": tmpindex,
            }
