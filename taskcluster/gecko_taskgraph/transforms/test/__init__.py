# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transforms construct a task description to run the given test, based on a
test description.  The implementation here is shared among all test kinds, but
contains specific support for how we run tests in Gecko (via mozharness,
invoked in particular ways).

This is a good place to translate a test-description option such as
`single-core: true` to the implementation of that option in a task description
(worker options, mozharness commandline, environment variables, etc.)

The test description should be fully formed by the time it reaches these
transforms, and these transforms should not embody any specific knowledge about
what should run where. this is the wrong place for special-casing platforms,
for example - use `all_tests.py` instead.
"""


import logging
from importlib import import_module

from mozbuild.schedules import INCLUSIVE_COMPONENTS
from voluptuous import (
    Any,
    Optional,
    Required,
    Exclusive,
)

from gecko_taskgraph.optimize.schema import OptimizationSchema
from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.transforms.test.other import get_mobile_project
from gecko_taskgraph.util.schema import (
    optionally_keyed_by,
    resolve_keyed_by,
    Schema,
)
from gecko_taskgraph.util.chunking import manifest_loaders


logger = logging.getLogger(__name__)
transforms = TransformSequence()


# Schema for a test description
#
# *****WARNING*****
#
# This is a great place for baffling cruft to accumulate, and that makes
# everyone move more slowly.  Be considerate of your fellow hackers!
# See the warnings in taskcluster/docs/how-tos.rst
#
# *****WARNING*****
test_description_schema = Schema(
    {
        # description of the suite, for the task metadata
        Required("description"): str,
        # test suite category and name
        Optional("suite"): Any(
            str,
            {Optional("category"): str, Optional("name"): str},
        ),
        # base work directory used to set up the task.
        Optional("workdir"): optionally_keyed_by("test-platform", Any(str, "default")),
        # the name by which this test suite is addressed in try syntax; defaults to
        # the test-name.  This will translate to the `unittest_try_name` or
        # `talos_try_name` attribute.
        Optional("try-name"): str,
        # additional tags to mark up this type of test
        Optional("tags"): {str: object},
        # the symbol, or group(symbol), under which this task should appear in
        # treeherder.
        Required("treeherder-symbol"): str,
        # the value to place in task.extra.treeherder.machine.platform; ideally
        # this is the same as build-platform, and that is the default, but in
        # practice it's not always a match.
        Optional("treeherder-machine-platform"): str,
        # attributes to appear in the resulting task (later transforms will add the
        # common attributes)
        Optional("attributes"): {str: object},
        # relative path (from config.path) to the file task was defined in
        Optional("job-from"): str,
        # The `run_on_projects` attribute, defaulting to "all".  This dictates the
        # projects on which this task should be included in the target task set.
        # See the attributes documentation for details.
        #
        # Note that the special case 'built-projects', the default, uses the parent
        # build task's run-on-projects, meaning that tests run only on platforms
        # that are built.
        Optional("run-on-projects"): optionally_keyed_by(
            "app",
            "subtest",
            "test-platform",
            "test-name",
            "variant",
            Any([str], "built-projects"),
        ),
        # When set only run on projects where the build would already be running.
        # This ensures tasks where this is True won't be the cause of the build
        # running on a project it otherwise wouldn't have.
        Optional("built-projects-only"): bool,
        # the sheriffing tier for this task (default: set based on test platform)
        Optional("tier"): optionally_keyed_by(
            "test-platform", "variant", "app", "subtest", Any(int, "default")
        ),
        # number of chunks to create for this task.  This can be keyed by test
        # platform by passing a dictionary in the `by-test-platform` key.  If the
        # test platform is not found, the key 'default' will be tried.
        Required("chunks"): optionally_keyed_by("test-platform", Any(int, "dynamic")),
        # Custom 'test_manifest_loader' to use, overriding the one configured in the
        # parameters. When 'null', no test chunking will be performed. Can also
        # be used to disable "manifest scheduling".
        Optional("test-manifest-loader"): Any(None, *list(manifest_loaders)),
        # the time (with unit) after which this task is deleted; default depends on
        # the branch (see below)
        Optional("expires-after"): str,
        # The different configurations that should be run against this task, defined
        # in the TEST_VARIANTS object in the variant.py transforms.
        Optional("variants"): [str],
        # Whether to run this task with e10s.  If false, run
        # without e10s; if true, run with e10s; if 'both', run one task with and
        # one task without e10s.  E10s tasks have "-e10s" appended to the test name
        # and treeherder group.
        Required("e10s"): optionally_keyed_by(
            "test-platform", "project", Any(bool, "both")
        ),
        # Whether the task should run with WebRender enabled or not.
        Optional("webrender"): bool,
        Optional("webrender-run-on-projects"): optionally_keyed_by(
            "app", Any([str], "default")
        ),
        # The EC2 instance size to run these tests on.
        Required("instance-size"): optionally_keyed_by(
            "test-platform", Any("default", "large", "xlarge")
        ),
        # type of virtualization or hardware required by test.
        Required("virtualization"): optionally_keyed_by(
            "test-platform", Any("virtual", "virtual-with-gpu", "hardware")
        ),
        # Whether the task requires loopback audio or video (whatever that may mean
        # on the platform)
        Required("loopback-audio"): bool,
        Required("loopback-video"): bool,
        # Whether the test can run using a software GL implementation on Linux
        # using the GL compositor. May not be used with "legacy" sized instances
        # due to poor LLVMPipe performance (bug 1296086).  Defaults to true for
        # unit tests on linux platforms and false otherwise
        Optional("allow-software-gl-layers"): bool,
        # For tasks that will run in docker-worker, this is the
        # name of the docker image or in-tree docker image to run the task in.  If
        # in-tree, then a dependency will be created automatically.  This is
        # generally `desktop-test`, or an image that acts an awful lot like it.
        Required("docker-image"): optionally_keyed_by(
            "test-platform",
            Any(
                # a raw Docker image path (repo/image:tag)
                str,
                # an in-tree generated docker image (from `taskcluster/docker/<name>`)
                {"in-tree": str},
                # an indexed docker image
                {"indexed": str},
            ),
        ),
        # seconds of runtime after which the task will be killed.  Like 'chunks',
        # this can be keyed by test pltaform.
        Required("max-run-time"): optionally_keyed_by("test-platform", "subtest", int),
        # the exit status code that indicates the task should be retried
        Optional("retry-exit-status"): [int],
        # Whether to perform a gecko checkout.
        Required("checkout"): bool,
        # Wheter to perform a machine reboot after test is done
        Optional("reboot"): Any(False, "always", "on-exception", "on-failure"),
        # What to run
        Required("mozharness"): {
            # the mozharness script used to run this task
            Required("script"): optionally_keyed_by("test-platform", str),
            # the config files required for the task
            Required("config"): optionally_keyed_by("test-platform", [str]),
            # mochitest flavor for mochitest runs
            Optional("mochitest-flavor"): str,
            # any additional actions to pass to the mozharness command
            Optional("actions"): [str],
            # additional command-line options for mozharness, beyond those
            # automatically added
            Required("extra-options"): optionally_keyed_by("test-platform", [str]),
            # the artifact name (including path) to test on the build task; this is
            # generally set in a per-kind transformation
            Optional("build-artifact-name"): str,
            Optional("installer-url"): str,
            # If not false, tooltool downloads will be enabled via relengAPIProxy
            # for either just public files, or all files.  Not supported on Windows
            Required("tooltool-downloads"): Any(
                False,
                "public",
                "internal",
            ),
            # Add --blob-upload-branch=<project> mozharness parameter
            Optional("include-blob-upload-branch"): bool,
            # The setting for --download-symbols (if omitted, the option will not
            # be passed to mozharness)
            Optional("download-symbols"): Any(True, "ondemand"),
            # If set, then MOZ_NODE_PATH=/usr/local/bin/node is included in the
            # environment.  This is more than just a helpful path setting -- it
            # causes xpcshell tests to start additional servers, and runs
            # additional tests.
            Required("set-moz-node-path"): bool,
            # If true, include chunking information in the command even if the number
            # of chunks is 1
            Required("chunked"): optionally_keyed_by("test-platform", bool),
            Required("requires-signed-builds"): optionally_keyed_by(
                "test-platform", bool
            ),
        },
        # The set of test manifests to run.
        Optional("test-manifests"): Any(
            [str],
            {"active": [str], "skipped": [str]},
        ),
        # The current chunk (if chunking is enabled).
        Optional("this-chunk"): int,
        # os user groups for test task workers; required scopes, will be
        # added automatically
        Optional("os-groups"): optionally_keyed_by("test-platform", [str]),
        Optional("run-as-administrator"): optionally_keyed_by("test-platform", bool),
        # -- values supplied by the task-generation infrastructure
        # the platform of the build this task is testing
        Required("build-platform"): str,
        # the label of the build task generating the materials to test
        Required("build-label"): str,
        # the label of the signing task generating the materials to test.
        # Signed builds are used in xpcshell tests on Windows, for instance.
        Optional("build-signing-label"): str,
        # the build's attributes
        Required("build-attributes"): {str: object},
        # the platform on which the tests will run
        Required("test-platform"): str,
        # limit the test-platforms (as defined in test-platforms.yml)
        # that the test will run on
        Optional("limit-platforms"): optionally_keyed_by("app", "subtest", [str]),
        # the name of the test (the key in tests.yml)
        Required("test-name"): str,
        # the product name, defaults to firefox
        Optional("product"): str,
        # conditional files to determine when these tests should be run
        Exclusive("when", "optimization"): {
            Optional("files-changed"): [str],
        },
        # Optimization to perform on this task during the optimization phase.
        # Optimizations are defined in taskcluster/gecko_taskgraph/optimize.py.
        Exclusive("optimization", "optimization"): OptimizationSchema,
        # The SCHEDULES component for this task; this defaults to the suite
        # (not including the flavor) but can be overridden here.
        Exclusive("schedules-component", "optimization"): Any(
            str,
            [str],
        ),
        Optional("worker-type"): optionally_keyed_by(
            "test-platform",
            Any(str, None),
        ),
        Optional(
            "require-signed-extensions",
            description="Whether the build being tested requires extensions be signed.",
        ): optionally_keyed_by("release-type", "test-platform", bool),
        # The target name, specifying the build artifact to be tested.
        # If None or not specified, a transform sets the target based on OS:
        # target.dmg (Mac), target.apk (Android), target.tar.bz2 (Linux),
        # or target.zip (Windows).
        Optional("target"): optionally_keyed_by(
            "app",
            "test-platform",
            Any(
                str,
                None,
                {Required("index"): str, Required("name"): str},
            ),
        ),
        # A list of artifacts to install from 'fetch' tasks. Validation deferred
        # to 'job' transforms.
        Optional("fetches"): object,
        # Opt-in to Python 3 support
        Optional("python-3"): bool,
        # Raptor / browsertime specific keys, defer validation to 'raptor.py'
        # transform.
        Optional("raptor"): object,
        # Raptor / browsertime specific keys that need to be here since 'raptor' schema
        # is evluated *before* test_description_schema
        Optional("app"): str,
        Optional("subtest"): str,
        # Define if a given task supports artifact builds or not, see bug 1695325.
        Optional("supports-artifact-builds"): bool,
    }
)


@transforms.add
def handle_keyed_by_mozharness(config, tasks):
    """Resolve a mozharness field if it is keyed by something"""
    fields = [
        "mozharness",
        "mozharness.chunked",
        "mozharness.config",
        "mozharness.extra-options",
        "mozharness.requires-signed-builds",
        "mozharness.script",
    ]
    for task in tasks:
        for field in fields:
            resolve_keyed_by(
                task, field, item_name=task["test-name"], enforce_single_match=False
            )
        yield task


@transforms.add
def set_defaults(config, tasks):
    for task in tasks:
        build_platform = task["build-platform"]
        if build_platform.startswith("android"):
            # all Android test tasks download internal objects from tooltool
            task["mozharness"]["tooltool-downloads"] = "internal"
            task["mozharness"]["actions"] = ["get-secrets"]

            # loopback-video is always true for Android, but false for other
            # platform phyla
            task["loopback-video"] = True
        task["mozharness"]["set-moz-node-path"] = True

        # software-gl-layers is only meaningful on linux unittests, where it defaults to True
        if task["test-platform"].startswith("linux") and task["suite"] not in [
            "talos",
            "raptor",
        ]:
            task.setdefault("allow-software-gl-layers", True)
        else:
            task["allow-software-gl-layers"] = False

        # Enable WebRender by default on the QuantumRender test platforms, since
        # the whole point of QuantumRender is to run with WebRender enabled.
        # This currently matches linux64-qr and windows10-64-qr; both of these
        # have /opt and /debug variants.
        if "-qr/" in task["test-platform"]:
            task["webrender"] = True
        else:
            task.setdefault("webrender", False)

        task.setdefault("e10s", True)
        task.setdefault("try-name", task["test-name"])
        task.setdefault("os-groups", [])
        task.setdefault("run-as-administrator", False)
        task.setdefault("chunks", 1)
        task.setdefault("run-on-projects", "built-projects")
        task.setdefault("built-projects-only", False)
        task.setdefault("instance-size", "default")
        task.setdefault("max-run-time", 3600)
        task.setdefault("reboot", False)
        task.setdefault("virtualization", "virtual")
        task.setdefault("loopback-audio", False)
        task.setdefault("loopback-video", False)
        task.setdefault("limit-platforms", [])
        task.setdefault("docker-image", {"in-tree": "ubuntu1804-test"})
        task.setdefault("checkout", False)
        task.setdefault("require-signed-extensions", False)
        task.setdefault("variants", [])
        task.setdefault("supports-artifact-builds", True)

        task["mozharness"].setdefault("extra-options", [])
        task["mozharness"].setdefault("requires-signed-builds", False)
        task["mozharness"].setdefault("tooltool-downloads", "public")
        task["mozharness"].setdefault("set-moz-node-path", False)
        task["mozharness"].setdefault("chunked", False)
        yield task


transforms.add_validate(test_description_schema)


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        resolve_keyed_by(
            task,
            "require-signed-extensions",
            item_name=task["test-name"],
            enforce_single_match=False,
            **{
                "release-type": config.params["release_type"],
            },
        )
        yield task


@transforms.add
def run_sibling_transforms(config, tasks):
    """Runs other transform files next to this module."""
    # List of modules to load transforms from in order.
    transform_modules = (
        ("variant", None),
        ("raptor", lambda t: t["suite"] == "raptor"),
        ("other", None),
        ("worker", None),
        # These transforms should always run last as there is never any
        # difference in configuration from one chunk to another (other than
        # chunk number).
        ("chunk", None),
    )

    for task in tasks:
        xforms = TransformSequence()
        for name, filterfn in transform_modules:
            if filterfn and not filterfn(task):
                continue

            mod = import_module(f"gecko_taskgraph.transforms.test.{name}")
            xforms.add(mod.transforms)

        yield from xforms(config, [task])


@transforms.add
def make_job_description(config, tasks):
    """Convert *test* descriptions to *job* descriptions (input to
    gecko_taskgraph.transforms.job)"""

    for task in tasks:
        attributes = task.get("attributes", {})

        mobile = get_mobile_project(task)
        if mobile and (mobile not in task["test-name"]):
            label = "{}-{}-{}-{}".format(
                config.kind, task["test-platform"], mobile, task["test-name"]
            )
        else:
            label = "{}-{}-{}".format(
                config.kind, task["test-platform"], task["test-name"]
            )

        try_name = task["try-name"]
        if attributes.get("unittest_variant"):
            suffix = task.pop("variant-suffix")
            label += suffix
            try_name += suffix

        if task["e10s"]:
            label += "-e10s"
        else:
            label += "-1proc"
            try_name += "-1proc"

        if task["chunks"] > 1:
            label += "-{}".format(task["this-chunk"])

        build_label = task["build-label"]

        if task["suite"] == "talos":
            attr_try_name = "talos_try_name"
        elif task["suite"] == "raptor":
            attr_try_name = "raptor_try_name"
        else:
            attr_try_name = "unittest_try_name"

        attr_build_platform, attr_build_type = task["build-platform"].split("/", 1)
        attributes.update(
            {
                "build_platform": attr_build_platform,
                "build_type": attr_build_type,
                "test_platform": task["test-platform"],
                "test_chunk": str(task["this-chunk"]),
                "supports-artifact-builds": task["supports-artifact-builds"],
                attr_try_name: try_name,
            }
        )

        if "test-manifests" in task:
            attributes["test_manifests"] = task["test-manifests"]

        jobdesc = {}
        name = "{}-{}".format(task["test-platform"], task["test-name"])
        jobdesc["name"] = name
        jobdesc["label"] = label
        jobdesc["description"] = task["description"]
        jobdesc["attributes"] = attributes
        jobdesc["dependencies"] = {"build": build_label}
        jobdesc["job-from"] = task["job-from"]

        if task.get("fetches"):
            jobdesc["fetches"] = task["fetches"]

        if task["mozharness"]["requires-signed-builds"] is True:
            jobdesc["dependencies"]["build-signing"] = task["build-signing-label"]

        if "expires-after" in task:
            jobdesc["expires-after"] = task["expires-after"]

        jobdesc["routes"] = []
        jobdesc["run-on-projects"] = sorted(task["run-on-projects"])
        jobdesc["scopes"] = []
        jobdesc["tags"] = task.get("tags", {})
        jobdesc["extra"] = {
            "chunks": {
                "current": task["this-chunk"],
                "total": task["chunks"],
            },
            "suite": attributes["unittest_suite"],
            "test-setting": task.pop("test-setting"),
        }
        jobdesc["treeherder"] = {
            "symbol": task["treeherder-symbol"],
            "kind": "test",
            "tier": task["tier"],
            "platform": task.get("treeherder-machine-platform", task["build-platform"]),
        }

        schedules = task.get("schedules-component", [])
        if task.get("when"):
            # This may still be used by comm-central.
            jobdesc["when"] = task["when"]
        elif "optimization" in task:
            jobdesc["optimization"] = task["optimization"]
        elif set(schedules) & set(INCLUSIVE_COMPONENTS):
            jobdesc["optimization"] = {"test-inclusive": schedules}
        else:
            jobdesc["optimization"] = {"test": schedules}

        run = jobdesc["run"] = {}
        run["using"] = "mozharness-test"
        run["test"] = task

        if "workdir" in task:
            run["workdir"] = task.pop("workdir")

        jobdesc["worker-type"] = task.pop("worker-type")
        if task.get("fetches"):
            jobdesc["fetches"] = task.pop("fetches")

        yield jobdesc


def normpath(path):
    return path.replace("/", "\\")


def get_firefox_version():
    with open("browser/config/version.txt") as f:
        return f.readline().strip()
