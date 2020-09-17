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
from pathlib import Path


TASKCLUSTER = "TASK_ID" in os.environ.keys()
RUNNING_TESTS = "RUNNING_TESTS" in os.environ.keys()
HERE = Path(__file__).parent
SRC_ROOT = Path(HERE, "..", "..", "..").resolve()
SEARCH_PATHS = [
    "python/mach",
    "python/mozboot",
    "python/mozbuild",
    "python/mozperftest",
    "python/mozterm",
    "python/mozversioncontrol",
    "testing/condprofile",
    "testing/mozbase/mozdevice",
    "testing/mozbase/mozfile",
    "testing/mozbase/mozinfo",
    "testing/mozbase/mozlog",
    "testing/mozbase/mozprocess",
    "testing/mozbase/mozprofile",
    "testing/mozbase/mozproxy",
    "third_party/python/attrs/src",
    "third_party/python/blessings",
    "third_party/python/distro",
    "third_party/python/dlmanager",
    "third_party/python/esprima",
    "third_party/python/importlib_metadata",
    "third_party/python/jsmin",
    "third_party/python/jsonschema",
    "third_party/python/pyrsistent",
    "third_party/python/PyYAML/lib3",
    "third_party/python/redo",
    "third_party/python/requests",
    "third_party/python/six",
    "third_party/python/zipp",
]


if TASKCLUSTER:
    SEARCH_PATHS.append("xpcshell")


# XXX need to make that for all systems flavors
if "SHELL" not in os.environ:
    os.environ["SHELL"] = "/bin/bash"


def _setup_path():
    """Adds all available dependencies in the path.

    This is done so the runner can be used with no prior
    install in all execution environments.
    """
    for path in SEARCH_PATHS:
        path = Path(SRC_ROOT, path).resolve()
        if path.exists():
            sys.path.insert(0, str(path))


def run_tests(mach_cmd, kwargs, client_args):
    """This tests runner can be used directly via main or via Mach.

    When the --on-try option is used, the test runner looks at the
    `PERFTEST_OPTIONS` environment variable that contains all options passed by
    the user via a ./mach perftest --push-to-try call.
    """
    _setup_path()
    on_try = kwargs.pop("on_try", False)

    # trying to get the arguments from the task params
    if on_try:
        try_options = json.loads(os.environ["PERFTEST_OPTIONS"])
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
        hooks.run("before_iterations", kwargs)
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


def main(argv=sys.argv[1:]):
    """Used when the runner is directly called from the shell"""
    _setup_path()

    from mozbuild.mozconfig import MozconfigLoader
    from mozbuild.base import MachCommandBase, MozbuildObject
    from mozperftest import PerftestArgumentParser
    from mozboot.util import get_state_dir
    from mach.logging import LoggingManager

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
    parser = PerftestArgumentParser(description="vanilla perftest")
    args = dict(vars(parser.parse_args(args=argv)))
    user_args = parser.get_user_args(args)
    run_tests(mach_cmd, args, user_args)


if __name__ == "__main__":
    sys.exit(main())
