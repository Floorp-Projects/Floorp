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

from __future__ import absolute_import, print_function, unicode_literals

import copy
import logging
import re

from mozbuild.schedules import INCLUSIVE_COMPONENTS
from six import string_types, text_type
from voluptuous import (
    Any,
    Optional,
    Required,
    Exclusive,
)

import taskgraph
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import keymatch
from taskgraph.util.keyed_by import evaluate_keyed_by
from taskgraph.util.templates import merge
from taskgraph.util.treeherder import split_symbol, join_symbol
from taskgraph.util.platforms import platform_family
from taskgraph.util.schema import (
    resolve_keyed_by,
    optionally_keyed_by,
    Schema,
)
from taskgraph.optimize.schema import OptimizationSchema
from taskgraph.util.chunking import (
    chunk_manifests,
    get_manifest_loader,
    get_runtimes,
    guess_mozinfo_from_task,
    manifest_loaders,
    DefaultLoader,
)
from taskgraph.util.taskcluster import (
    get_artifact_path,
    get_index_url,
)
from taskgraph.util.perfile import perfile_number_of_chunks


# default worker types keyed by instance-size
LINUX_WORKER_TYPES = {
    "large": "t-linux-large",
    "xlarge": "t-linux-xlarge",
    "default": "t-linux-large",
}

# windows worker types keyed by test-platform and virtualization
WINDOWS_WORKER_TYPES = {
    "windows7-32": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-shippable": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-devedition": {  # build only, tests have no value
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-mingwclang": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-qr": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-aarch64": {
        "virtual": "t-win64-aarch64-laptop",
        "virtual-with-gpu": "t-win64-aarch64-laptop",
        "hardware": "t-win64-aarch64-laptop",
    },
    "windows10-64-ccov": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-ccov-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-devedition": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-shippable": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-asan": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-shippable-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-mingwclang": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-ref-hw-2017": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-ref-hw",
    },
}

# os x worker types keyed by test-platform
MACOSX_WORKER_TYPES = {
    "macosx1014-64": "t-osx-1014",
    "macosx1014-64-power": "t-osx-1014-power",
    "macosx1015-64": "t-osx-1015-r8",
}


def gv_e10s_filter(task):
    return get_mobile_project(task) == "geckoview" and task["e10s"]


def fission_filter(task):
    return task.get("e10s") in (True, "both") and get_mobile_project(task) != "fennec"


TEST_VARIANTS = {
    "a11y-checks": {
        "description": "{description} with accessibility checks enabled",
        "suffix": "a11y-checks",
        "replace": {
            "run-on-projects": {
                "by-test-platform": {
                    "linux.*64(-shippable)?/opt": ["trunk"],
                    "default": [],
                },
            },
            "tier": 2,
        },
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--enable-a11y-checks",
                ],
            },
        },
    },
    "geckoview-e10s-single": {
        "description": "{description} with single-process e10s",
        "filterfn": gv_e10s_filter,
        "replace": {
            "run-on-projects": ["trunk"],
        },
        "suffix": "e10s-single",
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--setpref=dom.ipc.processCount=1",
                ],
            },
        },
    },
    "geckoview-fission": {
        "description": "{description} with fission enabled",
        "filterfn": gv_e10s_filter,
        "suffix": "fis",
        "merge": {
            # Ensures the default state is to not run anywhere.
            "fission-run-on-projects": [],
            "mozharness": {
                "extra-options": [
                    "--enable-fission",
                ],
            },
        },
    },
    "fission": {
        "description": "{description} with fission enabled",
        "filterfn": fission_filter,
        "suffix": "fis",
        "replace": {
            "e10s": True,
        },
        "merge": {
            # Ensures the default state is to not run anywhere.
            "fission-run-on-projects": [],
            "mozharness": {
                "extra-options": [
                    "--setpref=fission.autostart=true",
                    "--setpref=dom.serviceWorkers.parent_intercept=true",
                ],
            },
        },
    },
    "fission-xorigin": {
        "description": "{description} with cross-origin and fission enabled",
        "filterfn": fission_filter,
        "suffix": "fis-xorig",
        "replace": {
            "e10s": True,
        },
        "merge": {
            # Ensures the default state is to not run anywhere.
            "fission-run-on-projects": [],
            "mozharness": {
                "extra-options": [
                    "--setpref=fission.autostart=true",
                    "--setpref=dom.serviceWorkers.parent_intercept=true",
                    "--enable-xorigin-tests",
                ],
            },
        },
    },
    "socketprocess": {
        "description": "{description} with socket process enabled",
        "suffix": "spi",
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--setpref=media.peerconnection.mtransport_process=true",
                    "--setpref=network.process.enabled=true",
                ],
            }
        },
    },
    "socketprocess_networking": {
        "description": "{description} with networking on socket process enabled",
        "suffix": "spi-nw",
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--setpref=network.process.enabled=true",
                    "--setpref=network.http.network_access_on_socket_process.enabled=true",
                    "--setpref=network.ssl_tokens_cache_enabled=true",
                ],
            }
        },
    },
    "webrender": {
        "description": "{description} with webrender enabled",
        "suffix": "wr",
        "merge": {
            "webrender": True,
        },
    },
    "webrender-sw": {
        "description": "{description} with software webrender enabled",
        "suffix": "swr",
        "merge": {
            "webrender": True,
            "mozharness": {
                "extra-options": [
                    "--setpref=gfx.webrender.software=true",
                ],
            },
        },
    },
    "webgl-ipc": {
        # TODO: After 2021-05-01, verify this variant is still needed.
        "description": "{description} with WebGL IPC process enabled",
        "suffix": "gli",
        "replace": {
            "run-on-projects": {
                "by-test-platform": {
                    "linux.*-64.*": ["trunk"],
                    "mac.*": ["trunk"],
                    "win.*": ["trunk"],
                    "default": [],
                },
            },
        },
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--setpref=webgl.out-of-process=true",
                ],
            },
        },
    },
    "webgl-ipc-profiling": {
        # TODO: After 2021-05-01, verify this variant is still needed.
        "description": "{description} with WebGL IPC process enabled",
        "suffix": "gli",
        "merge": {
            "mozharness": {
                "extra-options": [
                    "--setpref=webgl.out-of-process=true",
                ],
            },
        },
    },
}


DYNAMIC_CHUNK_DURATION = 20 * 60  # seconds
"""The approximate time each test chunk should take to run."""


DYNAMIC_CHUNK_MULTIPLIER = {
    # Desktop xpcshell tests run in parallel. Reduce the total runtime to
    # compensate.
    "^(?!android).*-xpcshell.*": 0.2,
}
"""A multiplication factor to tweak the total duration per platform / suite."""


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
        Required("description"): text_type,
        # test suite category and name
        Optional("suite"): Any(
            text_type,
            {Optional("category"): text_type, Optional("name"): text_type},
        ),
        # base work directory used to set up the task.
        Optional("workdir"): optionally_keyed_by(
            "test-platform", Any(text_type, "default")
        ),
        # the name by which this test suite is addressed in try syntax; defaults to
        # the test-name.  This will translate to the `unittest_try_name` or
        # `talos_try_name` attribute.
        Optional("try-name"): text_type,
        # additional tags to mark up this type of test
        Optional("tags"): {text_type: object},
        # the symbol, or group(symbol), under which this task should appear in
        # treeherder.
        Required("treeherder-symbol"): text_type,
        # the value to place in task.extra.treeherder.machine.platform; ideally
        # this is the same as build-platform, and that is the default, but in
        # practice it's not always a match.
        Optional("treeherder-machine-platform"): text_type,
        # attributes to appear in the resulting task (later transforms will add the
        # common attributes)
        Optional("attributes"): {text_type: object},
        # relative path (from config.path) to the file task was defined in
        Optional("job-from"): text_type,
        # The `run_on_projects` attribute, defaulting to "all".  This dictates the
        # projects on which this task should be included in the target task set.
        # See the attributes documentation for details.
        #
        # Note that the special case 'built-projects', the default, uses the parent
        # build task's run-on-projects, meaning that tests run only on platforms
        # that are built.
        Optional("run-on-projects"): optionally_keyed_by(
            "test-platform", "test-name", "variant", Any([text_type], "built-projects")
        ),
        # When set only run on projects where the build would already be running.
        # This ensures tasks where this is True won't be the cause of the build
        # running on a project it otherwise wouldn't have.
        Optional("built-projects-only"): bool,
        # Same as `run-on-projects` except it only applies to Fission tasks. Fission
        # tasks will ignore `run_on_projects` and non-Fission tasks will ignore
        # `fission-run-on-projects`.
        Optional("fission-run-on-projects"): optionally_keyed_by(
            "test-name", "test-platform", Any([text_type], "built-projects")
        ),
        # the sheriffing tier for this task (default: set based on test platform)
        Optional("tier"): optionally_keyed_by(
            "test-platform", "variant", Any(int, "default")
        ),
        # Same as `tier` except it only applies to Fission tasks. Fission tasks
        # will ignore `tier` and non-Fission tasks will ignore `fission-tier`.
        Optional("fission-tier"): optionally_keyed_by(
            "test-platform", Any(int, "default")
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
        Optional("expires-after"): text_type,
        # The different configurations that should be run against this task, defined
        # in the TEST_VARIANTS object.
        Optional("variants"): optionally_keyed_by(
            "test-platform", "project", Any(list(TEST_VARIANTS))
        ),
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
            "app", Any([text_type], "default")
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
                text_type,
                # an in-tree generated docker image (from `taskcluster/docker/<name>`)
                {"in-tree": text_type},
                # an indexed docker image
                {"indexed": text_type},
            ),
        ),
        # seconds of runtime after which the task will be killed.  Like 'chunks',
        # this can be keyed by test pltaform.
        Required("max-run-time"): optionally_keyed_by("test-platform", int),
        # the exit status code that indicates the task should be retried
        Optional("retry-exit-status"): [int],
        # Whether to perform a gecko checkout.
        Required("checkout"): bool,
        # Wheter to perform a machine reboot after test is done
        Optional("reboot"): Any(False, "always", "on-exception", "on-failure"),
        # What to run
        Required("mozharness"): {
            # the mozharness script used to run this task
            Required("script"): optionally_keyed_by("test-platform", text_type),
            # the config files required for the task
            Required("config"): optionally_keyed_by("test-platform", [text_type]),
            # mochitest flavor for mochitest runs
            Optional("mochitest-flavor"): text_type,
            # any additional actions to pass to the mozharness command
            Optional("actions"): [text_type],
            # additional command-line options for mozharness, beyond those
            # automatically added
            Required("extra-options"): optionally_keyed_by(
                "test-platform", [text_type]
            ),
            # the artifact name (including path) to test on the build task; this is
            # generally set in a per-kind transformation
            Optional("build-artifact-name"): text_type,
            Optional("installer-url"): text_type,
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
            [text_type],
            {"active": [text_type], "skipped": [text_type]},
        ),
        # The current chunk (if chunking is enabled).
        Optional("this-chunk"): int,
        # os user groups for test task workers; required scopes, will be
        # added automatically
        Optional("os-groups"): optionally_keyed_by("test-platform", [text_type]),
        Optional("run-as-administrator"): optionally_keyed_by("test-platform", bool),
        # -- values supplied by the task-generation infrastructure
        # the platform of the build this task is testing
        Required("build-platform"): text_type,
        # the label of the build task generating the materials to test
        Required("build-label"): text_type,
        # the label of the signing task generating the materials to test.
        # Signed builds are used in xpcshell tests on Windows, for instance.
        Optional("build-signing-label"): text_type,
        # the build's attributes
        Required("build-attributes"): {text_type: object},
        # the platform on which the tests will run
        Required("test-platform"): text_type,
        # limit the test-platforms (as defined in test-platforms.yml)
        # that the test will run on
        Optional("limit-platforms"): optionally_keyed_by("app", [text_type]),
        # the name of the test (the key in tests.yml)
        Required("test-name"): text_type,
        # the product name, defaults to firefox
        Optional("product"): text_type,
        # conditional files to determine when these tests should be run
        Exclusive("when", "optimization"): {
            Optional("files-changed"): [text_type],
        },
        # Optimization to perform on this task during the optimization phase.
        # Optimizations are defined in taskcluster/taskgraph/optimize.py.
        Exclusive("optimization", "optimization"): OptimizationSchema,
        # The SCHEDULES component for this task; this defaults to the suite
        # (not including the flavor) but can be overridden here.
        Exclusive("schedules-component", "optimization"): Any(
            text_type,
            [text_type],
        ),
        Optional("worker-type"): optionally_keyed_by(
            "test-platform",
            Any(text_type, None),
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
            "test-platform",
            Any(
                text_type,
                None,
                {Required("index"): text_type, Required("name"): text_type},
            ),
        ),
        # A list of artifacts to install from 'fetch' tasks.
        Optional("fetches"): {
            text_type: optionally_keyed_by("test-platform", [text_type])
        },
        # Opt-in to Python 3 support
        Optional("python-3"): bool,
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
            resolve_keyed_by(task, field, item_name=task["test-name"])
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

        task["mozharness"].setdefault("extra-options", [])
        task["mozharness"].setdefault("requires-signed-builds", False)
        task["mozharness"].setdefault("tooltool-downloads", "public")
        task["mozharness"].setdefault("set-moz-node-path", False)
        task["mozharness"].setdefault("chunked", False)
        yield task


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        resolve_keyed_by(
            task,
            "require-signed-extensions",
            item_name=task["test-name"],
            **{
                "release-type": config.params["release_type"],
            }
        )
        yield task


@transforms.add
def setup_raptor(config, tasks):
    """Add options that are specific to raptor jobs (identified by suite=raptor)"""
    from taskgraph.transforms.raptor import transforms as raptor_transforms

    for task in tasks:
        if task["suite"] != "raptor":
            yield task
            continue

        for t in raptor_transforms(config, [task]):
            yield t


@transforms.add
def limit_platforms(config, tasks):
    for task in tasks:
        if not task["limit-platforms"]:
            yield task
            continue

        limited_platforms = {key: key for key in task["limit-platforms"]}
        if keymatch(limited_platforms, task["test-platform"]):
            yield task


transforms.add_validate(test_description_schema)


@transforms.add
def handle_suite_category(config, tasks):
    for task in tasks:
        task.setdefault("suite", {})

        if isinstance(task["suite"], text_type):
            task["suite"] = {"name": task["suite"]}

        suite = task["suite"].setdefault("name", task["test-name"])
        category = task["suite"].setdefault("category", suite)

        task.setdefault("attributes", {})
        task["attributes"]["unittest_suite"] = suite
        task["attributes"]["unittest_category"] = category

        script = task["mozharness"]["script"]
        category_arg = None
        if suite.startswith("test-verify") or suite.startswith("test-coverage"):
            pass
        elif script in ("android_emulator_unittest.py", "android_hardware_unittest.py"):
            category_arg = "--test-suite"
        elif script == "desktop_unittest.py":
            category_arg = "--{}-suite".format(category)

        if category_arg:
            task["mozharness"].setdefault("extra-options", [])
            extra = task["mozharness"]["extra-options"]
            if not any(arg.startswith(category_arg) for arg in extra):
                extra.append("{}={}".format(category_arg, suite))

        # From here on out we only use the suite name.
        task["suite"] = suite
        yield task


@transforms.add
def setup_talos(config, tasks):
    """Add options that are specific to talos jobs (identified by suite=talos)"""
    for task in tasks:
        if task["suite"] != "talos":
            yield task
            continue

        extra_options = task.setdefault("mozharness", {}).setdefault(
            "extra-options", []
        )
        extra_options.append("--use-talos-json")

        # win7 needs to test skip
        if task["build-platform"].startswith("win32"):
            extra_options.append("--add-option")
            extra_options.append("--setpref,gfx.direct2d.disabled=true")

        yield task


@transforms.add
def setup_browsertime_flag(config, tasks):
    """Optionally add `--browsertime` flag to Raptor pageload tests."""

    browsertime_flag = config.params["try_task_config"].get("browsertime", False)

    for task in tasks:
        if not browsertime_flag or task["suite"] != "raptor":
            yield task
            continue

        if task["treeherder-symbol"].startswith("Rap"):
            # The Rap group is subdivided as Rap{-fenix,-refbrow,-fennec}(...),
            # so `taskgraph.util.treeherder.replace_group` isn't appropriate.
            task["treeherder-symbol"] = task["treeherder-symbol"].replace(
                "Rap", "Btime", 1
            )

        extra_options = task.setdefault("mozharness", {}).setdefault(
            "extra-options", []
        )
        extra_options.append("--browsertime")

        yield task


@transforms.add
def handle_artifact_prefix(config, tasks):
    """Handle translating `artifact_prefix` appropriately"""
    for task in tasks:
        if task["build-attributes"].get("artifact_prefix"):
            task.setdefault("attributes", {}).setdefault(
                "artifact_prefix", task["build-attributes"]["artifact_prefix"]
            )
        yield task


@transforms.add
def set_target(config, tasks):
    for task in tasks:
        build_platform = task["build-platform"]
        target = None
        if "target" in task:
            resolve_keyed_by(task, "target", item_name=task["test-name"])
            target = task["target"]
        if not target:
            if build_platform.startswith("macosx"):
                target = "target.dmg"
            elif build_platform.startswith("android"):
                target = "target.apk"
            elif build_platform.startswith("win"):
                target = "target.zip"
            else:
                target = "target.tar.bz2"

        if isinstance(target, dict):
            # TODO Remove hardcoded mobile artifact prefix
            index_url = get_index_url(target["index"])
            installer_url = "{}/artifacts/public/{}".format(index_url, target["name"])
            task["mozharness"]["installer-url"] = installer_url
        else:
            task["mozharness"]["build-artifact-name"] = get_artifact_path(task, target)

        yield task


@transforms.add
def set_treeherder_machine_platform(config, tasks):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    translation = {
        # Linux64 build platform for asan is specified differently to
        # treeherder.
        "macosx1014-64/debug": "osx-10-14/debug",
        "macosx1014-64/opt": "osx-10-14/opt",
        "macosx1014-64-shippable/opt": "osx-10-14-shippable/opt",
        "win64-asan/opt": "windows10-64/asan",
        "win64-aarch64/opt": "windows10-aarch64/opt",
    }
    for task in tasks:
        # For most desktop platforms, the above table is not used for "regular"
        # builds, so we'll always pick the test platform here.
        # On macOS though, the regular builds are in the table.  This causes a
        # conflict in `verify_task_graph_symbol` once you add a new test
        # platform based on regular macOS builds, such as for QR.
        # Since it's unclear if the regular macOS builds can be removed from
        # the table, workaround the issue for QR.
        if "android" in task["test-platform"] and "pgo/opt" in task["test-platform"]:
            platform_new = task["test-platform"].replace("-pgo/opt", "/pgo")
            task["treeherder-machine-platform"] = platform_new
        elif "android-em-7.0-x86_64-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86_64-shippable-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"]
        elif "android-hw" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"]
        elif "android-em-7.0-x86_64" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        # Bug 1602863 - must separately define linux64/asan and linux1804-64/asan
        # otherwise causes an exception during taskgraph generation about
        # duplicate treeherder platform/symbol.
        elif "linux64-asan/opt" in task["test-platform"]:
            task["treeherder-machine-platform"] = "linux64/asan"
        elif "linux1804-asan/opt" in task["test-platform"]:
            task["treeherder-machine-platform"] = "linux1804-64/asan"
        else:
            task["treeherder-machine-platform"] = translation.get(
                task["build-platform"], task["test-platform"]
            )
        yield task


@transforms.add
def set_download_symbols(config, tasks):
    """In general, we download symbols immediately for debug builds, but only
    on demand for everything else. ASAN builds shouldn't download
    symbols since they don't product symbol zips see bug 1283879"""
    for task in tasks:
        if task["test-platform"].split("/")[-1] == "debug":
            task["mozharness"]["download-symbols"] = True
        elif (
            task["build-platform"] == "linux64-asan/opt"
            or task["build-platform"] == "windows10-64-asan/opt"
        ):
            if "download-symbols" in task["mozharness"]:
                del task["mozharness"]["download-symbols"]
        else:
            task["mozharness"]["download-symbols"] = "ondemand"
        yield task


@transforms.add
def handle_keyed_by(config, tasks):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "instance-size",
        "docker-image",
        "max-run-time",
        "chunks",
        "variants",
        "e10s",
        "suite",
        "run-on-projects",
        "fission-run-on-projects",
        "os-groups",
        "run-as-administrator",
        "workdir",
        "worker-type",
        "virtualization",
        "fetches.fetch",
        "fetches.toolchain",
        "target",
        "webrender-run-on-projects",
    ]
    for task in tasks:
        for field in fields:
            resolve_keyed_by(
                task,
                field,
                item_name=task["test-name"],
                defer=["variant"],
                project=config.params["project"],
            )
        yield task


@transforms.add
def setup_browsertime(config, tasks):
    """Configure browsertime dependencies for Raptor pageload tests that have
    `--browsertime` extra option."""

    for task in tasks:
        # We need to make non-trivial changes to various fetches, and our
        # `by-test-platform` may not be "compatible" with existing
        # `by-test-platform` filters.  Therefore we do everything after
        # `handle_keyed_by` so that existing fields have been resolved down to
        # simple lists.  But we use the `by-test-platform` machinery to express
        # filters so that when the time comes to move browsertime into YAML
        # files, the transition is straight-forward.
        extra_options = task.get("mozharness", {}).get("extra-options", [])

        if task["suite"] != "raptor" or "--browsertime" not in extra_options:
            yield task
            continue

        # This is appropriate as the browsertime task variants mature.
        task["tier"] = max(task["tier"], 1)

        ts = {
            "by-test-platform": {
                "android.*": ["browsertime", "linux64-geckodriver", "linux64-node"],
                "linux.*": ["browsertime", "linux64-geckodriver", "linux64-node"],
                "macosx.*": ["browsertime", "macosx64-geckodriver", "macosx64-node"],
                "windows.*aarch64.*": [
                    "browsertime",
                    "win32-geckodriver",
                    "win32-node",
                ],
                "windows.*-32.*": ["browsertime", "win32-geckodriver", "win32-node"],
                "windows.*-64.*": ["browsertime", "win64-geckodriver", "win64-node"],
            },
        }

        task.setdefault("fetches", {}).setdefault("toolchain", []).extend(
            evaluate_keyed_by(ts, "fetches.toolchain", task)
        )

        fs = {
            "by-test-platform": {
                "android.*": ["linux64-ffmpeg-4.1.4"],
                "linux.*": ["linux64-ffmpeg-4.1.4"],
                "macosx.*": ["mac64-ffmpeg-4.1.1"],
                "windows.*aarch64.*": ["win64-ffmpeg-4.1.1"],
                "windows.*-32.*": ["win64-ffmpeg-4.1.1"],
                "windows.*-64.*": ["win64-ffmpeg-4.1.1"],
            },
        }

        cd_fetches = {
            "android.*": [
                "linux64-chromedriver-87",
                "linux64-chromedriver-88",
                "linux64-chromedriver-89",
            ],
            "linux.*": [
                "linux64-chromedriver-87",
                "linux64-chromedriver-88",
                "linux64-chromedriver-89",
            ],
            "macosx.*": [
                "mac64-chromedriver-87",
                "mac64-chromedriver-88",
                "mac64-chromedriver-89",
            ],
            "windows.*aarch64.*": [
                "win32-chromedriver-87",
                "win32-chromedriver-88",
                "win32-chromedriver-89",
            ],
            "windows.*-32.*": [
                "win32-chromedriver-87",
                "win32-chromedriver-88",
                "win32-chromedriver-89",
            ],
            "windows.*-64.*": [
                "win32-chromedriver-87",
                "win32-chromedriver-88",
                "win32-chromedriver-89",
            ],
        }

        chromium_fetches = {
            "linux.*": ["linux64-chromium"],
            "macosx.*": ["mac-chromium"],
            "windows.*aarch64.*": ["win32-chromium"],
            "windows.*-32.*": ["win32-chromium"],
            "windows.*-64.*": ["win64-chromium"],
        }

        cd_extracted_name = {
            "windows": "{}chromedriver.exe",
            "mac": "{}chromedriver",
            "default": "{}chromedriver",
        }

        if "--app=chrome" in extra_options or "--app=chrome-m" in extra_options:
            # Only add the chromedriver fetches when chrome is running
            for platform in cd_fetches:
                fs["by-test-platform"][platform].extend(cd_fetches[platform])
        if "--app=chromium" in extra_options:
            for platform in chromium_fetches:
                fs["by-test-platform"][platform].extend(chromium_fetches[platform])

            # The chromedrivers for chromium are repackaged into the archives
            # that we get the chromium binary from so we always have a compatible
            # version.
            cd_extracted_name = {
                "windows": "chrome-win/chromedriver.exe",
                "mac": "chrome-mac/chromedriver",
                "default": "chrome-linux/chromedriver",
            }

        # Disable the Raptor install step
        if "--app=chrome-m" in extra_options:
            extra_options.append("--noinstall")

        task.setdefault("fetches", {}).setdefault("fetch", []).extend(
            evaluate_keyed_by(fs, "fetches.fetch", task)
        )

        extra_options.extend(
            (
                "--browsertime-browsertimejs",
                "$MOZ_FETCHES_DIR/browsertime/node_modules/browsertime/bin/browsertime.js",
            )
        )  # noqa: E501

        eos = {
            "by-test-platform": {
                "windows.*": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/node.exe",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver.exe",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["windows"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.1-win64-static/bin/ffmpeg.exe",
                ],
                "macosx.*": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/bin/node",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["mac"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.1-macos64-static/bin/ffmpeg",
                ],
                "default": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/bin/node",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["default"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.4-i686-static/ffmpeg",
                ],
            }
        }

        extra_options.extend(evaluate_keyed_by(eos, "mozharness.extra-options", task))

        yield task


def get_mobile_project(task):
    """Returns the mobile project of the specified task or None."""

    if not task["build-platform"].startswith("android"):
        return

    mobile_projects = ("fenix", "fennec", "geckoview", "refbrow", "chrome-m")

    for name in mobile_projects:
        if name in task["test-name"]:
            return name

    target = task.get("target")
    if target:
        if isinstance(target, dict):
            target = target["name"]

        for name in mobile_projects:
            if name in target:
                return name

    return "fennec"


@transforms.add
def adjust_mobile_e10s(config, tasks):
    for task in tasks:
        project = get_mobile_project(task)
        if project == "geckoview":
            # Geckoview is always-e10s
            task["e10s"] = True
        elif project == "fennec":
            # Fennec is non-e10s
            task["e10s"] = False
        yield task


@transforms.add
def disable_wpt_timeouts_on_autoland(config, tasks):
    """do not run web-platform-tests that are expected TIMEOUT on autoland"""
    for task in tasks:
        if (
            "web-platform-tests" in task["test-name"]
            and config.params["project"] == "autoland"
        ):
            task["mozharness"].setdefault("extra-options", []).append("--skip-timeout")
        yield task


@transforms.add
def split_variants(config, tasks):
    for task in tasks:
        variants = task.pop("variants", [])

        yield copy.deepcopy(task)

        for name in variants:
            variant = TEST_VARIANTS[name]

            if "filterfn" in variant and not variant["filterfn"](task):
                continue

            taskv = copy.deepcopy(task)
            taskv["attributes"]["unittest_variant"] = name
            taskv["description"] = variant["description"].format(**taskv)

            suffix = "-" + variant["suffix"]
            taskv["test-name"] += suffix
            taskv["try-name"] += suffix

            group, symbol = split_symbol(taskv["treeherder-symbol"])
            if group != "?":
                group += suffix
            else:
                symbol += suffix
            taskv["treeherder-symbol"] = join_symbol(group, symbol)

            taskv.update(variant.get("replace", {}))

            if task["suite"] == "raptor":
                taskv["tier"] = max(taskv["tier"], 2)

            yield merge(taskv, variant.get("merge", {}))


@transforms.add
def handle_keyed_by_variant(config, tasks):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "run-on-projects",
        "tier",
    ]
    for task in tasks:
        for field in fields:
            resolve_keyed_by(
                task,
                field,
                item_name=task["test-name"],
                variant=task["attributes"].get("unittest_variant"),
            )
        yield task


@transforms.add
def enable_code_coverage(config, tasks):
    """Enable code coverage for the ccov build-platforms"""
    for task in tasks:
        if "ccov" in task["build-platform"]:
            # Do not run tests on fuzzing builds
            if "fuzzing" in task["build-platform"]:
                task["run-on-projects"] = []
                continue

            # Skip this transform for android code coverage builds.
            if "android" in task["build-platform"]:
                task.setdefault("fetches", {}).setdefault("toolchain", []).append(
                    "linux64-grcov"
                )
                task["mozharness"].setdefault("extra-options", []).append(
                    "--java-code-coverage"
                )
                yield task
                continue
            task["mozharness"].setdefault("extra-options", []).append("--code-coverage")
            task["instance-size"] = "xlarge"

            # Temporarily disable Mac tests on mozilla-central
            if "mac" in task["build-platform"]:
                task["run-on-projects"] = []

            # Ensure we always run on the projects defined by the build, unless the test
            # is try only or shouldn't run at all.
            if task["run-on-projects"] not in [[]]:
                task["run-on-projects"] = "built-projects"

            # Ensure we don't optimize test suites out.
            # We always want to run all test suites for coverage purposes.
            task.pop("schedules-component", None)
            task.pop("when", None)
            task["optimization"] = None

            # Add a toolchain and a fetch task for the grcov binary.
            if any(p in task["build-platform"] for p in ("linux", "osx", "win")):
                task.setdefault("fetches", {})
                task["fetches"].setdefault("fetch", [])
                task["fetches"].setdefault("toolchain", [])

            if "linux" in task["build-platform"]:
                task["fetches"]["toolchain"].append("linux64-grcov")
            elif "osx" in task["build-platform"]:
                task["fetches"]["fetch"].append("grcov-osx-x86_64")
            elif "win" in task["build-platform"]:
                task["fetches"]["toolchain"].append("win64-grcov")

            if "talos" in task["test-name"]:
                task["max-run-time"] = 7200
                if "linux" in task["build-platform"]:
                    task["docker-image"] = {"in-tree": "ubuntu1804-test"}
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--cycles,1")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--tppagecycles,1")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--no-upload-results")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--tptimeout,15000")
            if "raptor" in task["test-name"]:
                task["max-run-time"] = 1800
        yield task


@transforms.add
def handle_run_on_projects(config, tasks):
    """Handle translating `built-projects` appropriately"""
    for task in tasks:
        if task["run-on-projects"] == "built-projects":
            task["run-on-projects"] = task["build-attributes"].get(
                "run_on_projects", ["all"]
            )

        if task.pop("built-projects-only", False):
            built_projects = set(
                task["build-attributes"].get("run_on_projects", {"all"})
            )
            run_on_projects = set(task.get("run-on-projects", set()))

            # If 'all' exists in run-on-projects, then the intersection of both
            # is built-projects. Similarly if 'all' exists in built-projects,
            # the intersection is run-on-projects (so do nothing). When neither
            # contains 'all', take the actual set intersection.
            if "all" in run_on_projects:
                task["run-on-projects"] = sorted(built_projects)
            elif "all" not in built_projects:
                task["run-on-projects"] = sorted(run_on_projects & built_projects)
        yield task


@transforms.add
def handle_tier(config, tasks):
    """Set the tier based on policy for all test descriptions that do not
    specify a tier otherwise."""
    for task in tasks:
        if "tier" in task:
            resolve_keyed_by(task, "tier", item_name=task["test-name"])

        if "fission-tier" in task:
            resolve_keyed_by(task, "fission-tier", item_name=task["test-name"])

        # only override if not set for the test
        if "tier" not in task or task["tier"] == "default":
            if task["test-platform"] in [
                "linux64/opt",
                "linux64/debug",
                "linux64-shippable/opt",
                "linux64-devedition/opt",
                "linux64-asan/opt",
                "linux64-qr/opt",
                "linux64-qr/debug",
                "linux64-shippable-qr/opt",
                "linux1804-64/opt",
                "linux1804-64/debug",
                "linux1804-64-shippable/opt",
                "linux1804-64-devedition/opt",
                "linux1804-64-qr/opt",
                "linux1804-64-qr/debug",
                "linux1804-64-shippable-qr/opt",
                "linux1804-64-asan/opt",
                "linux1804-64-tsan/opt",
                "windows7-32/debug",
                "windows7-32/opt",
                "windows7-32-devedition/opt",
                "windows7-32-shippable/opt",
                "windows10-aarch64/opt",
                "windows10-64/debug",
                "windows10-64/opt",
                "windows10-64-shippable/opt",
                "windows10-64-devedition/opt",
                "windows10-64-asan/opt",
                "windows10-64-qr/opt",
                "windows10-64-qr/debug",
                "windows10-64-shippable-qr/opt",
                "macosx1014-64/opt",
                "macosx1014-64/debug",
                "macosx1014-64-shippable/opt",
                "macosx1014-64-devedition/opt",
                "macosx1014-64-devedition-qr/opt",
                "macosx1014-64-qr/opt",
                "macosx1014-64-shippable-qr/opt",
                "macosx1014-64-qr/debug",
                "android-em-7.0-x86_64-shippable/opt",
                "android-em-7.0-x86_64/debug",
                "android-em-7.0-x86_64/opt",
                "android-em-7.0-x86-shippable/opt",
                "android-em-7.0-x86_64-shippable-qr/opt",
                "android-em-7.0-x86_64-qr/debug",
                "android-em-7.0-x86_64-qr/opt",
            ]:
                task["tier"] = 1
            else:
                task["tier"] = 2

        yield task


@transforms.add
def handle_fission_attributes(config, tasks):
    """Handle run_on_projects for fission tasks."""
    for task in tasks:
        for attr in ("run-on-projects", "tier"):
            fission_attr = task.pop("fission-{}".format(attr), None)

            if (
                task["attributes"].get("unittest_variant")
                not in ("fission", "geckoview-fission", "fission-xorigin")
            ) or fission_attr is None:
                continue

            task[attr] = fission_attr

        yield task


@transforms.add
def disable_try_only_platforms(config, tasks):
    """Turns off platforms that should only run on try."""
    try_only_platforms = ("windows7-32-qr/.*",)
    for task in tasks:
        if any(re.match(k + "$", task["test-platform"]) for k in try_only_platforms):
            task["run-on-projects"] = []
            if "fission-run-on-projects" in task:
                task["fission-run-on-projects"] = []
        yield task


@transforms.add
def ensure_spi_disabled_on_all_but_spi(config, tasks):
    for task in tasks:
        variant = task["attributes"].get("unittest_variant", "")
        has_setpref = (
            "gtest" not in task["suite"]
            and "cppunit" not in task["suite"]
            and "jittest" not in task["suite"]
            and "junit" not in task["suite"]
            and "raptor" not in task["suite"]
        )

        if (
            has_setpref
            and variant != "socketprocess"
            and variant != "socketprocess_networking"
        ):
            task["mozharness"]["extra-options"].append(
                "--setpref=media.peerconnection.mtransport_process=false"
            )
            task["mozharness"]["extra-options"].append(
                "--setpref=network.process.enabled=false"
            )

        yield task


@transforms.add
def split_e10s(config, tasks):
    for task in tasks:
        e10s = task["e10s"]

        if e10s:
            task_copy = copy.deepcopy(task)
            task_copy["test-name"] += "-e10s"
            task_copy["e10s"] = True
            task_copy["attributes"]["e10s"] = True
            yield task_copy

        if not e10s or e10s == "both":
            task["test-name"] += "-1proc"
            task["try-name"] += "-1proc"
            task["e10s"] = False
            task["attributes"]["e10s"] = False
            group, symbol = split_symbol(task["treeherder-symbol"])
            if group != "?":
                group += "-1proc"
            task["treeherder-symbol"] = join_symbol(group, symbol)
            task["mozharness"]["extra-options"].append("--disable-e10s")
            yield task


@transforms.add
def set_test_verify_chunks(config, tasks):
    """Set the number of chunks we use for test-verify."""
    for task in tasks:
        if any(task["suite"].startswith(s) for s in ("test-verify", "test-coverage")):
            env = config.params.get("try_task_config", {}) or {}
            env = env.get("templates", {}).get("env", {})
            task["chunks"] = perfile_number_of_chunks(
                config.params.is_try(),
                env.get("MOZHARNESS_TEST_PATHS", ""),
                config.params.get("head_repository", ""),
                config.params.get("head_rev", ""),
                task["test-name"],
            )

            # limit the number of chunks we run for test-verify mode because
            # test-verify is comprehensive and takes a lot of time, if we have
            # >30 tests changed, this is probably an import of external tests,
            # or a patch renaming/moving files in bulk
            maximum_number_verify_chunks = 3
            if task["chunks"] > maximum_number_verify_chunks:
                task["chunks"] = maximum_number_verify_chunks

        yield task


@transforms.add
def set_test_manifests(config, tasks):
    """Determine the set of test manifests that should run in this task."""

    for task in tasks:
        # When a task explicitly requests no 'test_manifest_loader', test
        # resolving will happen at test runtime rather than in the taskgraph.
        if "test-manifest-loader" in task and task["test-manifest-loader"] is None:
            yield task
            continue

        # Set 'tests_grouped' to "1", so we can differentiate between suites that are
        # chunked at the test runtime and those that are chunked in the taskgraph.
        task.setdefault("tags", {})["tests_grouped"] = "1"

        if taskgraph.fast:
            # We want to avoid evaluating manifests when taskgraph.fast is set. But
            # manifests are required for dynamic chunking. Just set the number of
            # chunks to one in this case.
            if task["chunks"] == "dynamic":
                task["chunks"] = 1
            yield task
            continue

        manifests = task.get("test-manifests")
        if manifests:
            if isinstance(manifests, list):
                task["test-manifests"] = {"active": manifests, "skipped": []}
            yield task
            continue

        mozinfo = guess_mozinfo_from_task(task)

        loader_name = task.pop(
            "test-manifest-loader", config.params["test_manifest_loader"]
        )
        loader = get_manifest_loader(loader_name, config.params)

        task["test-manifests"] = loader.get_manifests(
            task["suite"],
            frozenset(mozinfo.items()),
        )

        # The default loader loads all manifests. If we use a non-default
        # loader, we'll only run some subset of manifests and the hardcoded
        # chunk numbers will no longer be valid. Dynamic chunking should yield
        # better results.
        if not isinstance(loader, DefaultLoader):
            task["chunks"] = "dynamic"

        yield task


@transforms.add
def resolve_dynamic_chunks(config, tasks):
    """Determine how many chunks are needed to handle the given set of manifests."""

    for task in tasks:
        if task["chunks"] != "dynamic":
            yield task
            continue

        if not task.get("test-manifests"):
            raise Exception(
                "{} must define 'test-manifests' to use dynamic chunking!".format(
                    task["test-name"]
                )
            )

        runtimes = {
            m: r
            for m, r in get_runtimes(task["test-platform"], task["suite"]).items()
            if m in task["test-manifests"]["active"]
        }

        # Truncate runtimes that are above the desired chunk duration. They
        # will be assigned to a chunk on their own and the excess duration
        # shouldn't cause additional chunks to be needed.
        times = [min(DYNAMIC_CHUNK_DURATION, r) for r in runtimes.values()]
        avg = round(sum(times) / len(times), 2) if times else 0
        total = sum(times)

        # If there are manifests missing from the runtimes data, fill them in
        # with the average of all present manifests.
        missing = [m for m in task["test-manifests"]["active"] if m not in runtimes]
        total += avg * len(missing)

        # Apply any chunk multipliers if found.
        key = "{}-{}".format(task["test-platform"], task["test-name"])
        matches = keymatch(DYNAMIC_CHUNK_MULTIPLIER, key)
        if len(matches) > 1:
            raise Exception(
                "Multiple matching values for {} found while "
                "determining dynamic chunk multiplier!".format(key)
            )
        elif matches:
            total = total * matches[0]

        chunks = int(round(total / DYNAMIC_CHUNK_DURATION))

        # Make sure we never exceed the number of manifests, nor have a chunk
        # length of 0.
        task["chunks"] = min(chunks, len(task["test-manifests"]["active"])) or 1
        yield task


@transforms.add
def split_chunks(config, tasks):
    """Based on the 'chunks' key, split tests up into chunks by duplicating
    them and assigning 'this-chunk' appropriately and updating the treeherder
    symbol.
    """

    for task in tasks:
        # If test-manifests are set, chunk them ahead of time to avoid running
        # the algorithm more than once.
        chunked_manifests = None
        if "test-manifests" in task:
            manifests = task["test-manifests"]
            chunked_manifests = chunk_manifests(
                task["suite"],
                task["test-platform"],
                task["chunks"],
                manifests["active"],
            )

            # Add all skipped manifests to the first chunk of backstop pushes
            # so they still show up in the logs. They won't impact runtime much
            # and this way tools like ActiveData are still aware that they
            # exist.
            if config.params["backstop"] and manifests["active"]:
                chunked_manifests[0].extend(manifests["skipped"])

        for i in range(task["chunks"]):
            this_chunk = i + 1

            # copy the test and update with the chunk number
            chunked = copy.deepcopy(task)
            chunked["this-chunk"] = this_chunk

            if chunked_manifests is not None:
                chunked["test-manifests"] = sorted(chunked_manifests[i])

            group, symbol = split_symbol(chunked["treeherder-symbol"])
            if task["chunks"] > 1 or not symbol:
                # add the chunk number to the TH symbol
                symbol += str(this_chunk)
                chunked["treeherder-symbol"] = join_symbol(group, symbol)

            yield chunked


@transforms.add
def allow_software_gl_layers(config, tasks):
    """
    Handle the "allow-software-gl-layers" property for platforms where it
    applies.
    """
    for task in tasks:
        if task.get("allow-software-gl-layers"):
            # This should be set always once bug 1296086 is resolved.
            task["mozharness"].setdefault("extra-options", []).append(
                "--allow-software-gl-layers"
            )

        yield task


@transforms.add
def enable_webrender(config, tasks):
    """
    Handle the "webrender" property by passing a flag to mozharness if it is
    enabled.
    """
    for task in tasks:
        if task.get("webrender"):
            extra_options = task.get("mozharness", {}).get("extra-options", [])

            if task["attributes"]["unittest_category"] in ["raptor"] and (
                "--app=chrome" in extra_options or "--app=chromium" in extra_options
            ):
                continue

            extra_options.append("--enable-webrender")
            # We only want to 'setpref' on tests that have a profile
            if not task["attributes"]["unittest_category"] in [
                "cppunittest",
                "geckoview-junit",
                "gtest",
                "jittest",
                "raptor",
            ]:
                extra_options.append("--setpref=layers.d3d11.enable-blacklist=false")

            # run webrender variants on the projects specified on webrender-run-on-projects
            if task.get("webrender-run-on-projects") is not None:
                task["run-on-projects"] = task["webrender-run-on-projects"]

        yield task


@transforms.add
def set_schedules_for_webrender_android(config, tasks):
    """android-hw has limited resources, we need webrender on phones"""
    for task in tasks:
        if task["suite"] in ["crashtest", "reftest"] and task[
            "test-platform"
        ].startswith("android-hw"):
            task["schedules-component"] = "android-hw-gfx"
        yield task


@transforms.add
def set_retry_exit_status(config, tasks):
    """Set the retry exit status to TBPL_RETRY, the value returned by mozharness
    scripts to indicate a transient failure that should be retried."""
    for task in tasks:
        task["retry-exit-status"] = [4]
        yield task


@transforms.add
def set_profile(config, tasks):
    """Set profiling mode for tests."""
    profile = config.params["try_task_config"].get("gecko-profile", False)

    for task in tasks:
        if profile and task["suite"] in ["talos", "raptor"]:
            task["mozharness"]["extra-options"].append("--gecko-profile")
        yield task


@transforms.add
def set_tag(config, tasks):
    """Set test for a specific tag."""
    tag = None
    if config.params["try_mode"] == "try_option_syntax":
        tag = config.params["try_options"]["tag"]
    for task in tasks:
        if tag:
            task["mozharness"]["extra-options"].extend(["--tag", tag])
        yield task


@transforms.add
def set_test_type(config, tasks):
    types = ["mochitest", "reftest", "talos", "raptor", "geckoview-junit", "gtest"]
    for task in tasks:
        for test_type in types:
            if test_type in task["suite"] and "web-platform" not in task["suite"]:
                task.setdefault("tags", {})["test-type"] = test_type
        yield task


@transforms.add
def set_worker_type(config, tasks):
    """Set the worker type based on the test platform."""
    for task in tasks:
        # during the taskcluster migration, this is a bit tortured, but it
        # will get simpler eventually!
        test_platform = task["test-platform"]
        if task.get("worker-type"):
            # This test already has its worker type defined, so just use that (yields below)
            pass
        elif test_platform.startswith("macosx1014-64"):
            if "--power-test" in task["mozharness"]["extra-options"]:
                task["worker-type"] = MACOSX_WORKER_TYPES["macosx1014-64-power"]
            else:
                task["worker-type"] = MACOSX_WORKER_TYPES["macosx1014-64"]
        elif test_platform.startswith("macosx1015-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1015-64"]
        elif test_platform.startswith("win"):
            # figure out what platform the job needs to run on
            if task["virtualization"] == "hardware":
                # some jobs like talos and reftest run on real h/w - those are all win10
                if test_platform.startswith("windows10-64-ref-hw-2017"):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES[
                        "windows10-64-ref-hw-2017"
                    ]
                elif test_platform.startswith("windows10-aarch64"):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES["windows10-aarch64"]
                else:
                    win_worker_type_platform = WINDOWS_WORKER_TYPES["windows10-64"]
            else:
                # the other jobs run on a vm which may or may not be a win10 vm
                win_worker_type_platform = WINDOWS_WORKER_TYPES[
                    test_platform.split("/")[0]
                ]
            # now we have the right platform set the worker type accordingly
            task["worker-type"] = win_worker_type_platform[task["virtualization"]]
        elif test_platform.startswith("android-hw-g5"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-g5"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-g5"
        elif test_platform.startswith("android-hw-p2"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-p2"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-p2"
        elif test_platform.startswith("android-hw-s7"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-s7"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-s7"
        elif test_platform.startswith("android-em-7.0-x86"):
            task["worker-type"] = "t-linux-metal"
        elif test_platform.startswith("linux") or test_platform.startswith("android"):
            if task.get("suite", "") in ["talos", "raptor"] and not task[
                "build-platform"
            ].startswith("linux64-ccov"):
                task["worker-type"] = "t-linux-talos-1804"
            else:
                task["worker-type"] = LINUX_WORKER_TYPES[task["instance-size"]]
        else:
            raise Exception("unknown test_platform {}".format(test_platform))

        yield task


@transforms.add
def set_schedules_components(config, tasks):
    for task in tasks:
        if "optimization" in task or "when" in task:
            yield task
            continue

        category = task["attributes"]["unittest_category"]
        schedules = task.get("schedules-component", category)
        if isinstance(schedules, string_types):
            schedules = [schedules]

        schedules = set(schedules)
        if schedules & set(INCLUSIVE_COMPONENTS):
            # if this is an "inclusive" test, then all files which might
            # cause it to run are annotated with SCHEDULES in moz.build,
            # so do not include the platform or any other components here
            task["schedules-component"] = sorted(schedules)
            yield task
            continue

        schedules.add(category)
        schedules.add(platform_family(task["build-platform"]))

        if task["webrender"]:
            schedules.add("webrender")

        task["schedules-component"] = sorted(schedules)
        yield task


@transforms.add
def make_job_description(config, tasks):
    """Convert *test* descriptions to *job* descriptions (input to
    taskgraph.transforms.job)"""

    for task in tasks:
        mobile = get_mobile_project(task)
        if mobile and (mobile not in task["test-name"]):
            label = "{}-{}-{}-{}".format(
                config.kind, task["test-platform"], mobile, task["test-name"]
            )
        else:
            label = "{}-{}-{}".format(
                config.kind, task["test-platform"], task["test-name"]
            )
        if task["chunks"] > 1:
            label += "-{}".format(task["this-chunk"])

        build_label = task["build-label"]

        try_name = task["try-name"]
        if task["suite"] == "talos":
            attr_try_name = "talos_try_name"
        elif task["suite"] == "raptor":
            attr_try_name = "raptor_try_name"
        else:
            attr_try_name = "unittest_try_name"

        attr_build_platform, attr_build_type = task["build-platform"].split("/", 1)

        attributes = task.get("attributes", {})
        attributes.update(
            {
                "build_platform": attr_build_platform,
                "build_type": attr_build_type,
                "test_platform": task["test-platform"],
                "test_chunk": str(task["this-chunk"]),
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
    with open("browser/config/version.txt", "r") as f:
        return f.readline().strip()
