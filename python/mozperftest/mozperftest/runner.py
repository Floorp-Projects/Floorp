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


HERE = os.path.dirname(__file__)
SRC_ROOT = os.path.join(HERE, "..", "..", "..")
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
    "third_party/python/distro",
    "third_party/python/dlmanager",
    "third_party/python/esprima",
    "third_party/python/importlib_metadata",
    "third_party/python/jsonschema",
    "third_party/python/pyrsistent",
    "third_party/python/PyYAML/lib3",
    "third_party/python/redo",
    "third_party/python/requests",
    "third_party/python/six",
    "third_party/python/zipp",
]


# XXX need to make that for all systems flavors
if "SHELL" not in os.environ:
    os.environ["SHELL"] = "/bin/bash"


def _setup_path():
    """Adds all dependencies in the path.

    This is done so the runner can be used with no prior
    install in all execution environments.
    """
    for path in SEARCH_PATHS:
        path = os.path.abspath(path)
        path = os.path.join(SRC_ROOT, path)
        if not os.path.exists(path):
            raise IOError("Can't find %s" % path)
        sys.path.insert(0, path)


def run_tests(mach_cmd, **kwargs):
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

    hooks = Hooks(mach_cmd, kwargs.pop("hooks", None))
    verbose = kwargs.get("verbose", False)
    log_level = logging.DEBUG if verbose else logging.INFO

    # If we run through mach, we just  want to set the level
    # of the existing termminal handler.
    # Otherwise, we're adding it.
    if mach_cmd.log_manager.terminal_handler is not None:
        mach_cmd.log_manager.terminal_handler.level = log_level
    else:
        mach_cmd.log_manager.add_terminal_logging(level=log_level)

    try:
        hooks.run("before_iterations", kwargs)

        for iteration in range(kwargs.get("test_iterations", 1)):
            flavor = kwargs["flavor"]
            kwargs["tests"], tmp_dir = build_test_list(
                kwargs["tests"], randomized=flavor != "doc"
            )
            try:
                # XXX this doc is specific to browsertime scripts
                # maybe we want to move it
                if flavor == "doc":
                    from mozperftest.test.browsertime.script import ScriptInfo

                    for test in kwargs["tests"]:
                        print(ScriptInfo(test))
                    return

                env = MachEnvironment(mach_cmd, hooks=hooks, **kwargs)
                metadata = Metadata(mach_cmd, env, flavor)
                hooks.run("before_runs", env)
                try:
                    with env.frozen() as e:
                        e.run(metadata)
                finally:
                    hooks.run("after_runs", env)
            finally:
                if tmp_dir is not None:
                    shutil.rmtree(tmp_dir)
    finally:
        hooks.cleanup()


def main(argv=sys.argv[1:]):
    """Used when the runner is directly called from the shell
    """
    _setup_path()

    from mozbuild.base import MachCommandBase, MozbuildObject
    from mozperftest import PerftestArgumentParser
    from mozboot.util import get_state_dir
    from mach.logging import LoggingManager

    config = MozbuildObject.from_environment()
    config.topdir = config.topsrcdir
    config.cwd = os.getcwd()
    config.state_dir = get_state_dir()
    config.log_manager = LoggingManager()
    mach_cmd = MachCommandBase(config)
    parser = PerftestArgumentParser(description="vanilla perftest")
    args = parser.parse_args(args=argv)
    run_tests(mach_cmd, **dict(args._get_kwargs()))


if __name__ == "__main__":
    sys.exit(main())
