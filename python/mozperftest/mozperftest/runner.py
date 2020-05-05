# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Pure Python runner so we can execute perftest in the CI without
depending on the mach toolchain, that is not fully available in
all worker environments.
"""
import sys
import os


HERE = os.path.dirname(__file__)
SRC_ROOT = os.path.join(HERE, "..", "..", "..")
SEARCH_PATHS = [
    "python/mach",
    "python/mozboot",
    "python/mozbuild",
    "python/mozperftest",
    "python/mozterm",
    "python/mozversioncontrol",
    "testing/mozbase/mozdevice",
    "testing/mozbase/mozfile",
    "testing/mozbase/mozinfo",
    "testing/mozbase/mozlog",
    "testing/mozbase/mozprocess",
    "testing/mozbase/mozprofile",
    "testing/mozbase/mozproxy",
    "third_party/python/attrs/src",
    "third_party/python/dlmanager",
    "third_party/python/esprima",
    "third_party/python/importlib_metadata",
    "third_party/python/jsonschema",
    "third_party/python/pyrsistent",
    "third_party/python/pyyaml/lib3",
    "third_party/python/redo",
    "third_party/python/requests",
    "third_party/python/six",
]


# XXX need to make that for all systems flavors
if "SHELL" not in os.environ:
    os.environ["SHELL"] = "/bin/bash"


def _setup_path():
    for path in SEARCH_PATHS:
        path = os.path.abspath(path)
        if not os.path.exists(path):
            raise IOError("Can't find %s" % path)
        sys.path.insert(0, os.path.join(SRC_ROOT, path))


def main():
    _setup_path()

    from mozbuild.base import MachCommandBase, MozbuildObject
    from mozperftest import PerftestArgumentParser
    from mozboot.util import get_state_dir

    config = MozbuildObject.from_environment()
    config.topdir = config.topsrcdir
    config.cwd = os.getcwd()
    config.state_dir = get_state_dir()
    mach_cmd = MachCommandBase(config)
    parser = PerftestArgumentParser(description="vanilla perftest")
    args = parser.parse_args()
    run_tests(mach_cmd, **dict(args._get_kwargs()))


def run_tests(mach_cmd, **kwargs):
    _setup_path()

    from mozperftest.utils import build_test_list, install_package
    from mozperftest import MachEnvironment, Metadata

    flavor = kwargs["flavor"]
    kwargs["tests"] = build_test_list(kwargs["tests"], randomized=flavor != "doc")

    if flavor == "doc":
        location = os.path.join(mach_cmd.topsrcdir, "third_party", "python", "esprima")
        install_package(mach_cmd.virtualenv_manager, location)

        from mozperftest.scriptinfo import ScriptInfo

        for test in kwargs["tests"]:
            print(ScriptInfo(test))
        return

    env = MachEnvironment(mach_cmd, **kwargs)
    metadata = Metadata(mach_cmd, env, flavor)
    env.run_hook("before_runs")
    try:
        with env.frozen() as e:
            e.run(metadata)
    finally:
        env.run_hook("after_runs")


if __name__ == "__main__":
    sys.exit(main())
