# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Pure Python runner so we can execute perftest in the CI without
depending on a full mach toolchain, that is not fully available in
all worker environments.

This runner can be executed in two different ways:

- by calling run_tests() from the mach command
- by executing this module directly

When the module is executed directly, if the --on-try option is used,
it will fetch arguments from Tascluster's parameters, that were
populated via a local --push-to-try call.

The --push-to-try flow is:

- a user calls ./mach perftest --push-to-try --option1 --option2
- a new push to try commit is made and includes all options in its parameters
- a generic TC job triggers the perftest by calling this module with --on-try
- run_test() grabs the parameters artifact and converts them into args for
  perftest
"""
import json
import os
import shutil
import sys
import logging
import tempfile
from pathlib import Path


TASKCLUSTER = "TASK_ID" in os.environ.keys()
RUNNING_TESTS = "RUNNING_TESTS" in os.environ.keys()
HERE = Path(__file__).parent
SRC_ROOT = Path(HERE, "..", "..", "..").resolve()


# XXX need to make that for all systems flavors
if "SHELL" not in os.environ:
    os.environ["SHELL"] = "/bin/bash"


def _activate_mach_virtualenv():
    """Adds all available dependencies in the path.

    This is done so the runner can be used with no prior
    install in all execution environments.
    """

    # We need the "mach" module to access the logic to parse virtualenv
    # requirements. Since that depends on "packaging" (and, transitively,
    # "pyparsing"), we add those to the path too.
    sys.path[0:0] = [
        os.path.join(SRC_ROOT, module)
        for module in (
            os.path.join("python", "mach"),
            os.path.join("third_party", "python", "packaging"),
            os.path.join("third_party", "python", "pyparsing"),
        )
    ]

    from mach.site import (
        resolve_requirements,
        MachSiteManager,
        ExternalPythonSite,
        SitePackagesSource,
    )

    mach_site = MachSiteManager(
        str(SRC_ROOT),
        None,
        resolve_requirements(str(SRC_ROOT), "mach"),
        ExternalPythonSite(sys.executable),
        SitePackagesSource.NONE,
    )
    mach_site.activate()

    if TASKCLUSTER:
        # In CI, the directory structure is different: xpcshell code is in
        # "$topsrcdir/xpcshell/" rather than "$topsrcdir/testing/xpcshell".
        sys.path.append("xpcshell")


def _create_artifacts_dir(kwargs):
    artifacts = SRC_ROOT / "artifacts"

    artifacts = artifacts / "side-by-side"
    artifacts.mkdir(exist_ok=True)

    artifacts = artifacts / kwargs["test_name"]
    artifacts.mkdir(exist_ok=True)

    return artifacts


def run_tests(mach_cmd, kwargs, client_args):
    """This tests runner can be used directly via main or via Mach.

    When the --on-try option is used, the test runner looks at the
    `PERFTEST_OPTIONS` environment variable that contains all options passed by
    the user via a ./mach perftest --push-to-try call.
    """
    on_try = kwargs.pop("on_try", False)

    # trying to get the arguments from the task params
    if on_try:
        try_options = json.loads(os.environ["PERFTEST_OPTIONS"])
        print("Loading options from $PERFTEST_OPTIONS")
        print(json.dumps(try_options, indent=4, sort_keys=True))
        kwargs.update(try_options)

    from mozperftest.utils import build_test_list
    from mozperftest import MachEnvironment, Metadata
    from mozperftest.hooks import Hooks
    from mozperftest.script import ScriptInfo

    hooks_file = kwargs.pop("hooks", None)
    hooks = Hooks(mach_cmd, hooks_file)
    verbose = kwargs.get("verbose", False)
    log_level = logging.DEBUG if verbose else logging.INFO

    # If we run through mach, we just  want to set the level
    # of the existing termminal handler.
    # Otherwise, we're adding it.
    if mach_cmd.log_manager.terminal_handler is not None:
        mach_cmd.log_manager.terminal_handler.level = log_level
    else:
        mach_cmd.log_manager.add_terminal_logging(level=log_level)
        mach_cmd.log_manager.enable_all_structured_loggers()
        mach_cmd.log_manager.enable_unstructured()

    try:
        # Only pass the virtualenv to the before_iterations hook
        # so that users can install test-specific packages if needed.
        mach_cmd.activate_virtualenv()
        kwargs["virtualenv"] = mach_cmd.virtualenv_manager
        hooks.run("before_iterations", kwargs)
        del kwargs["virtualenv"]

        tests, tmp_dir = build_test_list(kwargs["tests"])

        for test in tests:
            script = ScriptInfo(test)

            # update the arguments with options found in the script, if any
            args = script.update_args(**client_args)
            # XXX this should be the default pool for update_args
            for key, value in kwargs.items():
                if key not in args:
                    args[key] = value

            # update the hooks, or use a copy of the general one
            script_hooks = Hooks(mach_cmd, args.pop("hooks", hooks_file))

            flavor = args["flavor"]
            if flavor == "doc":
                print(script)
                continue

            for iteration in range(args.get("test_iterations", 1)):
                try:
                    env = MachEnvironment(mach_cmd, hooks=script_hooks, **args)
                    metadata = Metadata(mach_cmd, env, flavor, script)
                    script_hooks.run("before_runs", env)
                    try:
                        with env.frozen() as e:
                            e.run(metadata)
                    finally:
                        script_hooks.run("after_runs", env)
                finally:
                    if tmp_dir is not None:
                        shutil.rmtree(tmp_dir)
    finally:
        hooks.cleanup()


def run_tools(mach_cmd, kwargs, client_args):
    """This tools runner can be used directly via main or via Mach.

    **TODO**: Before adding any more tools, we need to split this logic out
    into a separate file that runs the tools and sets them up dynamically
    in a similar way to how we use layers.
    """
    from mozperftest.utils import install_package

    mach_cmd.activate_virtualenv()
    install_package(mach_cmd.virtualenv_manager, "opencv-python==4.5.4.60")
    install_package(
        mach_cmd.virtualenv_manager,
        "mozperftest-tools==0.1.11",
    )

    log_level = logging.INFO
    if mach_cmd.log_manager.terminal_handler is not None:
        mach_cmd.log_manager.terminal_handler.level = log_level
    else:
        mach_cmd.log_manager.add_terminal_logging(level=log_level)
        mach_cmd.log_manager.enable_all_structured_loggers()
        mach_cmd.log_manager.enable_unstructured()

    from mozperftest_tools.side_by_side import SideBySide

    artifacts = _create_artifacts_dir(kwargs)

    tempdir = tempfile.mkdtemp()

    s = SideBySide(str(tempdir))
    s.run(**kwargs)

    try:
        for file in os.listdir(tempdir):
            if file.startswith("cold-") or file.startswith("warm-"):
                print(f"Copying from {tempdir}/{file} to {artifacts}/{file}")
                shutil.copy(Path(tempdir, file), artifacts)
    finally:
        shutil.rmtree(tempdir)


def main(argv=sys.argv[1:]):
    """Used when the runner is directly called from the shell"""
    _activate_mach_virtualenv()

    from mozbuild.mozconfig import MozconfigLoader
    from mozbuild.base import MachCommandBase, MozbuildObject
    from mozperftest import PerftestArgumentParser, PerftestToolsArgumentParser
    from mach.logging import LoggingManager
    from mach.util import get_state_dir

    mozconfig = SRC_ROOT / "browser" / "config" / "mozconfig"
    if mozconfig.exists():
        os.environ["MOZCONFIG"] = str(mozconfig)

    if "--xpcshell-mozinfo" in argv:
        mozinfo = argv[argv.index("--xpcshell-mozinfo") + 1]
        topobjdir = Path(mozinfo).parent
    else:
        topobjdir = None

    config = MozbuildObject(
        str(SRC_ROOT),
        None,
        LoggingManager(),
        topobjdir=topobjdir,
        mozconfig=MozconfigLoader.AUTODETECT,
    )
    config.topdir = config.topsrcdir
    config.cwd = os.getcwd()
    config.state_dir = get_state_dir()

    # This monkey patch forces mozbuild to reuse
    # our configuration when it tries to re-create
    # it from the environment.
    def _here(*args, **kw):
        return config

    MozbuildObject.from_environment = _here

    mach_cmd = MachCommandBase(config)

    if "tools" in argv[0]:
        if len(argv) == 1:
            raise SystemExit("No tool specified, cannot continue parsing")
        PerftestToolsArgumentParser.tool = argv[1]
        perftools_parser = PerftestToolsArgumentParser()
        args = dict(vars(perftools_parser.parse_args(args=argv[2:])))
        user_args = perftools_parser.get_user_args(args)
        run_tools(mach_cmd, args, user_args)
    else:
        perftest_parser = PerftestArgumentParser(description="vanilla perftest")
        args = dict(vars(perftest_parser.parse_args(args=argv)))
        user_args = perftest_parser.get_user_args(args)
        run_tests(mach_cmd, args, user_args)


if __name__ == "__main__":
    sys.exit(main())
