# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
from functools import partial
import json

from mach.decorators import Command, CommandArgument, SubCommand
from mozbuild.base import MachCommandConditions as conditions


_TRY_PLATFORMS = {
    "g5-browsertime": "perftest-android-hw-g5-browsertime",
    "p2-browsertime": "perftest-android-hw-p2-browsertime",
    "linux-xpcshell": "perftest-linux-try-xpcshell",
    "mac-xpcshell": "perftest-macosx-try-xpcshell",
    "linux-browsertime": "perftest-linux-try-browsertime",
    "mac-browsertime": "perftest-macosx-try-browsertime",
    "win-browsertimee": "perftest-windows-try-browsertime",
}


HERE = os.path.dirname(__file__)


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


def get_perftest_tools_parser(tool):
    def tools_parser_func():
        from mozperftest import PerftestToolsArgumentParser

        PerftestToolsArgumentParser.tool = tool
        return PerftestToolsArgumentParser

    return tools_parser_func


def get_parser():
    return run_perftest._mach_command._parser


@Command(
    "perftest",
    category="testing",
    conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
    description="Run any flavor of perftest",
    parser=get_perftest_parser,
)
def run_perftest(command_context, **kwargs):
    # original parser that brought us there
    original_parser = get_parser()

    from pathlib import Path

    # user selection with fuzzy UI
    from mozperftest.utils import ON_TRY
    from mozperftest.script import ScriptInfo, ScriptType, ParseError

    if not ON_TRY and kwargs.get("tests", []) == []:
        from moztest.resolve import TestResolver
        from mozperftest.fzf.fzf import select

        resolver = command_context._spawn(TestResolver)
        test_objects = list(resolver.resolve_tests(paths=None, flavor="perftest"))
        selected = select(test_objects)

        def full_path(selection):
            __, script_name, __, location = selection.split(" ")
            return str(
                Path(
                    command_context.topsrcdir.rstrip(os.sep),
                    location.strip(os.sep),
                    script_name,
                )
            )

        kwargs["tests"] = [full_path(s) for s in selected]

        if kwargs["tests"] == []:
            print("\nNo selection. Bye!")
            return

    if len(kwargs["tests"]) > 1:
        print("\nSorry no support yet for multiple local perftest")
        return

    sel = "\n".join(kwargs["tests"])
    print("\nGood job! Best selection.\n%s" % sel)
    # if the script is xpcshell, we can force the flavor here
    # XXX on multi-selection,  what happens if we have seeveral flavors?
    try:
        script_info = ScriptInfo(kwargs["tests"][0])
    except ParseError as e:
        if e.exception is IsADirectoryError:
            script_info = None
        else:
            raise
    else:
        if script_info.script_type == ScriptType.xpcshell:
            kwargs["flavor"] = script_info.script_type.name
        else:
            # we set the value only if not provided (so "mobile-browser"
            # can be picked)
            if "flavor" not in kwargs:
                kwargs["flavor"] = "desktop-browser"

    push_to_try = kwargs.pop("push_to_try", False)
    if push_to_try:
        sys.path.append(str(Path(command_context.topsrcdir, "tools", "tryselect")))

        from tryselect.push import push_to_try

        perftest_parameters = {}
        args = script_info.update_args(**original_parser.get_user_args(kwargs))
        platform = args.pop("try_platform", "linux")
        if isinstance(platform, str):
            platform = [platform]

        platform = ["%s-%s" % (plat, script_info.script_type.name) for plat in platform]

        for plat in platform:
            if plat not in _TRY_PLATFORMS:
                # we can extend platform support here: linux, win, macOs, pixel2
                # by adding more jobs in taskcluster/ci/perftest/kind.yml
                # then picking up the right one here
                raise NotImplementedError(
                    "%r doesn't exist or is not yet supported" % plat
                )

        def relative(path):
            if path.startswith(command_context.topsrcdir):
                return path[len(command_context.topsrcdir) :].lstrip(os.sep)
            return path

        for name, value in args.items():
            # ignore values that are set to default
            if original_parser.get_default(name) == value:
                continue
            if name == "tests":
                value = [relative(path) for path in value]
            perftest_parameters[name] = value

        parameters = {
            "try_task_config": {
                "tasks": [_TRY_PLATFORMS[plat] for plat in platform],
                "perftest-options": perftest_parameters,
            },
            "try_mode": "try_task_config",
        }

        task_config = {"parameters": parameters, "version": 2}
        if args.get("verbose"):
            print("Pushing run to try...")
            print(json.dumps(task_config, indent=4, sort_keys=True))

        push_to_try("perftest", "perftest", try_task_config=task_config)
        return

    from mozperftest.runner import run_tests

    run_tests(command_context, kwargs, original_parser.get_user_args(kwargs))

    print("\nFirefox. Fast For Good.\n")


@Command(
    "perftest-test",
    category="testing",
    description="Run perftest tests",
    virtualenv_name="perftest-test",
)
@CommandArgument(
    "tests", default=None, nargs="*", help="Tests to run. By default will run all"
)
@CommandArgument(
    "-s",
    "--skip-linters",
    action="store_true",
    default=False,
    help="Skip flake8 and black",
)
@CommandArgument(
    "-v", "--verbose", action="store_true", default=False, help="Verbose mode"
)
def run_tests(command_context, **kwargs):
    from pathlib import Path
    from mozperftest.utils import temporary_env

    with temporary_env(
        COVERAGE_RCFILE=str(Path(HERE, ".coveragerc")), RUNNING_TESTS="YES"
    ):
        _run_tests(command_context, **kwargs)


def _run_tests(command_context, **kwargs):
    from pathlib import Path
    from mozperftest.utils import (
        ON_TRY,
        checkout_script,
        checkout_python_script,
    )

    venv = command_context.virtualenv_manager
    skip_linters = kwargs.get("skip_linters", False)
    verbose = kwargs.get("verbose", False)

    if not ON_TRY and not skip_linters:
        cmd = "./mach lint "
        if verbose:
            cmd += " -v"
        cmd += " " + str(HERE)
        if not checkout_script(cmd, label="linters", display=verbose, verbose=verbose):
            raise AssertionError("Please fix your code.")

    # running pytest with coverage
    # coverage is done in three steps:
    # 1/ coverage erase => erase any previous coverage data
    # 2/ coverage run pytest ... => run the tests and collect info
    # 3/ coverage report => generate the report
    tests_dir = Path(HERE, "tests").resolve()
    tests = kwargs.get("tests", [])
    if tests == []:
        tests = str(tests_dir)
        run_coverage_check = not skip_linters
    else:
        run_coverage_check = False

        def _get_test(test):
            if Path(test).exists():
                return str(test)
            return str(tests_dir / test)

        tests = " ".join([_get_test(test) for test in tests])

    # on macOS + try we skip the coverage
    # because macOS workers prevent us from installing
    # packages from PyPI
    if sys.platform == "darwin" and ON_TRY:
        run_coverage_check = False

    options = "-xs"
    if kwargs.get("verbose"):
        options += "v"

    if run_coverage_check:
        assert checkout_python_script(
            venv, "coverage", ["erase"], label="remove old coverage data"
        )
    args = ["run", "-m", "pytest", options, "--durations", "10", tests]
    assert checkout_python_script(
        venv, "coverage", args, label="running tests", verbose=verbose
    )
    if run_coverage_check and not checkout_python_script(
        venv, "coverage", ["report"], display=True
    ):
        raise ValueError("Coverage is too low!")


@Command(
    "perftest-tools",
    category="testing",
    description="Run perftest tools",
)
def run_tools(command_context, **kwargs):
    """
    Runs various perftest tools such as the side-by-side generator.
    """
    print("Runs various perftest tools such as the side-by-side generator.")


@SubCommand(
    "perftest-tools",
    "side-by-side",
    description="This tool can be used to generate a side-by-side visualization of two videos. "
    "When using this tool, make sure that the `--test-name` is an exact match, i.e. if you are "
    "comparing  the task `test-linux64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s` "
    "between two revisions, then use `browsertime-tp6-firefox-linkedin-e10s` as the suite name "
    "and `test-linux64-shippable-qr/opt` as the platform.",
    virtualenv_name="perftest-side-by-side",
    parser=get_perftest_tools_parser("side-by-side"),
)
def run_side_by_side(command_context, **kwargs):
    from mozperftest.runner import run_tools

    run_tools(command_context, kwargs, {})
