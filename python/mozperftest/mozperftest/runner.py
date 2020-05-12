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


def _get_params():
    """Fetches the parameters.yml artifact and returns its content.
    """
    # XXX - this already exists in taskcluster code
    # in a more robust way, but for now let's not depend on it.
    import requests
    import yaml

    root = os.environ.get(
        "TASKCLUSTER_ROOT_URL", "https://firefox-ci-tc.services.mozilla.com"
    )
    # set by require-decision-task-id
    tid = os.environ["DECISION_TASK_ID"]
    url = root + "/api/queue/v1/task/%s/artifacts/public/parameters.yml" % tid
    response = requests.get(url)
    return yaml.load(response.text)


def run_tests(mach_cmd, **kwargs):
    """This tests runner can be used directly via main or via Mach.

    When the --on-try option is used, the test runner looks for the
    `parameters.yml` artifact that contains all options passed by
    the used via a ./mach perftest --push-to-try call.
    """
    _setup_path()
    on_try = kwargs.pop("on_try", False)

    # trying to get the arguments from the task params
    if on_try:
        params = _get_params()
        kwargs.update(params["try_options"]["perftest"])

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


def main():
    """Used when the runner is directly called from the shell
    """
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


if __name__ == "__main__":
    sys.exit(main())
