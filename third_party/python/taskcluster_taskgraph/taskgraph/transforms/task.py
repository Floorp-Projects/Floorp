# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transformations take a task description and turn it into a TaskCluster
task definition (along with attributes, label, etc.).  The input to these
transformations is generic to any kind of task, but abstracts away some of the
complexities of worker implementations, scopes, and treeherder annotations.
"""


import hashlib
import os
import re
import time
from copy import deepcopy
from dataclasses import dataclass
from typing import Callable

from voluptuous import All, Any, Extra, NotIn, Optional, Required

from taskgraph import MAX_DEPENDENCIES
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.hash import hash_path
from taskgraph.util.keyed_by import evaluate_keyed_by
from taskgraph.util.memoize import memoize
from taskgraph.util.schema import (
    OptimizationSchema,
    Schema,
    optionally_keyed_by,
    resolve_keyed_by,
    taskref_or_string,
    validate_schema,
)
from taskgraph.util.treeherder import split_symbol, treeherder_defaults
from taskgraph.util.workertypes import worker_type_implementation

from ..util import docker as dockerutil
from ..util.workertypes import get_worker_type

RUN_TASK = os.path.join(
    os.path.dirname(os.path.dirname(__file__)), "run-task", "run-task"
)


@memoize
def _run_task_suffix():
    """String to append to cache names under control of run-task."""
    return hash_path(RUN_TASK)[0:20]


# A task description is a general description of a TaskCluster task
task_description_schema = Schema(
    {
        # the label for this task
        Required("label"): str,
        # description of the task (for metadata)
        Required("description"): str,
        # attributes for this task
        Optional("attributes"): {str: object},
        # relative path (from config.path) to the file task was defined in
        Optional("task-from"): str,
        # dependencies of this task, keyed by name; these are passed through
        # verbatim and subject to the interpretation of the Task's get_dependencies
        # method.
        Optional("dependencies"): {
            All(
                str,
                NotIn(
                    ["self", "decision"],
                    "Can't use 'self` or 'decision' as dependency names.",
                ),
            ): object,
        },
        # Soft dependencies of this task, as a list of tasks labels
        Optional("soft-dependencies"): [str],
        # Dependencies that must be scheduled in order for this task to run.
        Optional("if-dependencies"): [str],
        Optional("requires"): Any("all-completed", "all-resolved"),
        # expiration and deadline times, relative to task creation, with units
        # (e.g., "14 days").  Defaults are set based on the project.
        Optional("expires-after"): str,
        Optional("deadline-after"): str,
        # custom routes for this task; the default treeherder routes will be added
        # automatically
        Optional("routes"): [str],
        # custom scopes for this task; any scopes required for the worker will be
        # added automatically. The following parameters will be substituted in each
        # scope:
        #  {level} -- the scm level of this push
        #  {project} -- the project of this push
        Optional("scopes"): [str],
        # Tags
        Optional("tags"): {str: str},
        # custom "task.extra" content
        Optional("extra"): {str: object},
        # treeherder-related information; see
        # https://schemas.taskcluster.net/taskcluster-treeherder/v1/task-treeherder-config.json
        # This may be provided in one of two ways:
        # 1) A simple `true` will cause taskgraph to generate the required information
        # 2) A dictionary with one or more of the required keys. Any key not present
        #    will use a default as described below.
        # If not specified, no treeherder extra information or routes will be
        # added to the task
        Optional("treeherder"): Any(
            True,
            {
                # either a bare symbol, or "grp(sym)".
                # The default symbol is the uppercased first letter of each
                # section of the kind (delimited by "-") all smooshed together.
                # Eg: "test" becomes "T", "docker-image" becomes "DI", etc.
                "symbol": Optional(str),
                # the job kind
                # If "build" or "test" is found in the kind name, this defaults
                # to the appropriate value. Otherwise, defaults to "other"
                "kind": Optional(Any("build", "test", "other")),
                # tier for this task
                # Defaults to 1
                "tier": Optional(int),
                # task platform, in the form platform/collection, used to set
                # treeherder.machine.platform and treeherder.collection or
                # treeherder.labels
                # Defaults to "default/opt"
                "platform": Optional(str),
            },
        ),
        # information for indexing this build so its artifacts can be discovered;
        # if omitted, the build will not be indexed.
        Optional("index"): {
            # the name of the product this build produces
            "product": str,
            # the names to use for this job in the TaskCluster index
            "job-name": str,
            # Type of gecko v2 index to use
            "type": str,
            # The rank that the task will receive in the TaskCluster
            # index.  A newly completed task supersedes the currently
            # indexed task iff it has a higher rank.  If unspecified,
            # 'by-tier' behavior will be used.
            "rank": Any(
                # Rank is equal the timestamp of the build_date for tier-1
                # tasks, and zero for non-tier-1.  This sorts tier-{2,3}
                # builds below tier-1 in the index.
                "by-tier",
                # Rank is given as an integer constant (e.g. zero to make
                # sure a task is last in the index).
                int,
                # Rank is equal to the timestamp of the build_date.  This
                # option can be used to override the 'by-tier' behavior
                # for non-tier-1 tasks.
                "build_date",
            ),
        },
        # The `run_on_projects` attribute, defaulting to "all".  This dictates the
        # projects on which this task should be included in the target task set.
        # See the attributes documentation for details.
        Optional("run-on-projects"): optionally_keyed_by("build-platform", [str]),
        Optional("run-on-tasks-for"): [str],
        Optional("run-on-git-branches"): [str],
        # The `shipping_phase` attribute, defaulting to None. This specifies the
        # release promotion phase that this task belongs to.
        Optional("shipping-phase"): Any(
            None,
            "build",
            "promote",
            "push",
            "ship",
        ),
        # The `always-target` attribute will cause the task to be included in the
        # target_task_graph regardless of filtering. Tasks included in this manner
        # will be candidates for optimization even when `optimize_target_tasks` is
        # False, unless the task was also explicitly chosen by the target_tasks
        # method.
        Required("always-target"): bool,
        # Optimization to perform on this task during the optimization phase.
        # Optimizations are defined in taskcluster/taskgraph/optimize.py.
        Required("optimization"): OptimizationSchema,
        # the provisioner-id/worker-type for the task.  The following parameters will
        # be substituted in this string:
        #  {level} -- the scm level of this push
        "worker-type": str,
        # Whether the job should use sccache compiler caching.
        Required("needs-sccache"): bool,
        # information specific to the worker implementation that will run this task
        Optional("worker"): {
            Required("implementation"): str,
            Extra: object,
        },
    }
)

TC_TREEHERDER_SCHEMA_URL = (
    "https://github.com/taskcluster/taskcluster-treeherder/"
    "blob/master/schemas/task-treeherder-config.yml"
)


UNKNOWN_GROUP_NAME = (
    "Treeherder group {} (from {}) has no name; " "add it to taskcluster/ci/config.yml"
)

V2_ROUTE_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.latest.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.pushdate.{build_date_long}.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.pushlog-id.{pushlog_id}.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.revision.{branch_rev}.{product}.{job-name}",
]

# the roots of the treeherder routes
TREEHERDER_ROUTE_ROOT = "tc-treeherder"


def get_branch_rev(config):
    return config.params["head_rev"]


@memoize
def get_default_priority(graph_config, project):
    return evaluate_keyed_by(
        graph_config["task-priority"], "Graph Config", {"project": project}
    )


@memoize
def get_default_deadline(graph_config, project):
    return evaluate_keyed_by(
        graph_config["task-deadline-after"], "Graph Config", {"project": project}
    )


# define a collection of payload builders, depending on the worker implementation
payload_builders = {}


@dataclass(frozen=True)
class PayloadBuilder:
    schema: Schema
    builder: Callable


def payload_builder(name, schema):
    schema = Schema({Required("implementation"): name, Optional("os"): str}).extend(
        schema
    )

    def wrap(func):
        assert name not in payload_builders, f"duplicate payload builder name {name}"
        payload_builders[name] = PayloadBuilder(schema, func)
        return func

    return wrap


# define a collection of index builders, depending on the type implementation
index_builders = {}


def index_builder(name):
    def wrap(func):
        assert name not in index_builders, f"duplicate index builder name {name}"
        index_builders[name] = func
        return func

    return wrap


UNSUPPORTED_INDEX_PRODUCT_ERROR = """\
The index product {product} is not in the list of configured products in
`taskcluster/ci/config.yml'.
"""


def verify_index(config, index):
    product = index["product"]
    if product not in config.graph_config["index"]["products"]:
        raise Exception(UNSUPPORTED_INDEX_PRODUCT_ERROR.format(product=product))


@payload_builder(
    "docker-worker",
    schema={
        Required("os"): "linux",
        # For tasks that will run in docker-worker, this is the name of the docker
        # image or in-tree docker image to run the task in.  If in-tree, then a
        # dependency will be created automatically.  This is generally
        # `desktop-test`, or an image that acts an awful lot like it.
        Required("docker-image"): Any(
            # a raw Docker image path (repo/image:tag)
            str,
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {"in-tree": str},
            # an indexed docker image
            {"indexed": str},
        ),
        # worker features that should be enabled
        Required("relengapi-proxy"): bool,
        Required("chain-of-trust"): bool,
        Required("taskcluster-proxy"): bool,
        Required("allow-ptrace"): bool,
        Required("loopback-video"): bool,
        Required("loopback-audio"): bool,
        Required("docker-in-docker"): bool,  # (aka 'dind')
        Required("privileged"): bool,
        # Paths to Docker volumes.
        #
        # For in-tree Docker images, volumes can be parsed from Dockerfile.
        # This only works for the Dockerfile itself: if a volume is defined in
        # a base image, it will need to be declared here. Out-of-tree Docker
        # images will also require explicit volume annotation.
        #
        # Caches are often mounted to the same path as Docker volumes. In this
        # case, they take precedence over a Docker volume. But a volume still
        # needs to be declared for the path.
        Optional("volumes"): [str],
        # caches to set up for the task
        Optional("caches"): [
            {
                # only one type is supported by any of the workers right now
                "type": "persistent",
                # name of the cache, allowing re-use by subsequent tasks naming the
                # same cache
                "name": str,
                # location in the task image where the cache will be mounted
                "mount-point": str,
                # Whether the cache is not used in untrusted environments
                # (like the Try repo).
                Optional("skip-untrusted"): bool,
            }
        ],
        # artifacts to extract from the task image after completion
        Optional("artifacts"): [
            {
                # type of artifact -- simple file, or recursive directory
                "type": Any("file", "directory"),
                # task image path from which to read artifact
                "path": str,
                # name of the produced artifact (root of the names for
                # type=directory)
                "name": str,
            }
        ],
        # environment variables
        Required("env"): {str: taskref_or_string},
        # the command to run; if not given, docker-worker will default to the
        # command in the docker image
        Optional("command"): [taskref_or_string],
        # the maximum time to run, in seconds
        Required("max-run-time"): int,
        # the exit status code(s) that indicates the task should be retried
        Optional("retry-exit-status"): [int],
        # the exit status code(s) that indicates the caches used by the task
        # should be purged
        Optional("purge-caches-exit-status"): [int],
        # Whether any artifacts are assigned to this worker
        Optional("skip-artifacts"): bool,
    },
)
def build_docker_worker_payload(config, task, task_def):
    worker = task["worker"]
    level = int(config.params["level"])

    image = worker["docker-image"]
    if isinstance(image, dict):
        if "in-tree" in image:
            name = image["in-tree"]
            docker_image_task = "build-docker-image-" + image["in-tree"]
            task.setdefault("dependencies", {})["docker-image"] = docker_image_task

            image = {
                "path": "public/image.tar.zst",
                "taskId": {"task-reference": "<docker-image>"},
                "type": "task-image",
            }

            # Find VOLUME in Dockerfile.
            volumes = dockerutil.parse_volumes(name)
            for v in sorted(volumes):
                if v in worker["volumes"]:
                    raise Exception(
                        "volume %s already defined; "
                        "if it is defined in a Dockerfile, "
                        "it does not need to be specified in the "
                        "worker definition" % v
                    )

                worker["volumes"].append(v)

        elif "indexed" in image:
            image = {
                "path": "public/image.tar.zst",
                "namespace": image["indexed"],
                "type": "indexed-image",
            }
        else:
            raise Exception("unknown docker image type")

    features = {}

    if worker.get("relengapi-proxy"):
        features["relengAPIProxy"] = True

    if worker.get("taskcluster-proxy"):
        features["taskclusterProxy"] = True

    if worker.get("allow-ptrace"):
        features["allowPtrace"] = True
        task_def["scopes"].append("docker-worker:feature:allowPtrace")

    if worker.get("chain-of-trust"):
        features["chainOfTrust"] = True

    if worker.get("docker-in-docker"):
        features["dind"] = True

    if task.get("needs-sccache"):
        features["taskclusterProxy"] = True
        task_def["scopes"].append(
            "assume:project:taskcluster:{trust_domain}:level-{level}-sccache-buckets".format(
                trust_domain=config.graph_config["trust-domain"],
                level=config.params["level"],
            )
        )
        worker["env"]["USE_SCCACHE"] = "1"
        # Disable sccache idle shutdown.
        worker["env"]["SCCACHE_IDLE_TIMEOUT"] = "0"
    else:
        worker["env"]["SCCACHE_DISABLE"] = "1"

    capabilities = {}

    for lo in "audio", "video":
        if worker.get("loopback-" + lo):
            capitalized = "loopback" + lo.capitalize()
            devices = capabilities.setdefault("devices", {})
            devices[capitalized] = True
            task_def["scopes"].append("docker-worker:capability:device:" + capitalized)

    if worker.get("privileged"):
        capabilities["privileged"] = True
        task_def["scopes"].append("docker-worker:capability:privileged")

    task_def["payload"] = payload = {
        "image": image,
        "env": worker["env"],
    }
    if "command" in worker:
        payload["command"] = worker["command"]

    if "max-run-time" in worker:
        payload["maxRunTime"] = worker["max-run-time"]

    run_task = payload.get("command", [""])[0].endswith("run-task")

    # run-task exits EXIT_PURGE_CACHES if there is a problem with caches.
    # Automatically retry the tasks and purge caches if we see this exit
    # code.
    # TODO move this closer to code adding run-task once bug 1469697 is
    # addressed.
    if run_task:
        worker.setdefault("retry-exit-status", []).append(72)
        worker.setdefault("purge-caches-exit-status", []).append(72)

    payload["onExitStatus"] = {}
    if "retry-exit-status" in worker:
        payload["onExitStatus"]["retry"] = worker["retry-exit-status"]
    if "purge-caches-exit-status" in worker:
        payload["onExitStatus"]["purgeCaches"] = worker["purge-caches-exit-status"]

    if "artifacts" in worker:
        artifacts = {}
        for artifact in worker["artifacts"]:
            artifacts[artifact["name"]] = {
                "path": artifact["path"],
                "type": artifact["type"],
                "expires": task_def["expires"],  # always expire with the task
            }
        payload["artifacts"] = artifacts

    if isinstance(worker.get("docker-image"), str):
        out_of_tree_image = worker["docker-image"]
    else:
        out_of_tree_image = None
        image = worker.get("docker-image", {}).get("in-tree")

    if "caches" in worker:
        caches = {}

        # run-task knows how to validate caches.
        #
        # To help ensure new run-task features and bug fixes don't interfere
        # with existing caches, we seed the hash of run-task into cache names.
        # So, any time run-task changes, we should get a fresh set of caches.
        # This means run-task can make changes to cache interaction at any time
        # without regards for backwards or future compatibility.
        #
        # But this mechanism only works for in-tree Docker images that are built
        # with the current run-task! For out-of-tree Docker images, we have no
        # way of knowing their content of run-task. So, in addition to varying
        # cache names by the contents of run-task, we also take the Docker image
        # name into consideration. This means that different Docker images will
        # never share the same cache. This is a bit unfortunate. But it is the
        # safest thing to do. Fortunately, most images are defined in-tree.
        #
        # For out-of-tree Docker images, we don't strictly need to incorporate
        # the run-task content into the cache name. However, doing so preserves
        # the mechanism whereby changing run-task results in new caches
        # everywhere.

        # As an additional mechanism to force the use of different caches, the
        # string literal in the variable below can be changed. This is
        # preferred to changing run-task because it doesn't require images
        # to be rebuilt.
        cache_version = "v3"

        if run_task:
            suffix = f"{cache_version}-{_run_task_suffix()}"

            if out_of_tree_image:
                name_hash = hashlib.sha256(
                    out_of_tree_image.encode("utf-8")
                ).hexdigest()
                suffix += name_hash[0:12]

        else:
            suffix = cache_version

        skip_untrusted = config.params.is_try() or level == 1

        for cache in worker["caches"]:
            # Some caches aren't enabled in environments where we can't
            # guarantee certain behavior. Filter those out.
            if cache.get("skip-untrusted") and skip_untrusted:
                continue

            name = "{trust_domain}-level-{level}-{name}-{suffix}".format(
                trust_domain=config.graph_config["trust-domain"],
                level=config.params["level"],
                name=cache["name"],
                suffix=suffix,
            )
            caches[name] = cache["mount-point"]
            task_def["scopes"].append("docker-worker:cache:%s" % name)

        # Assertion: only run-task is interested in this.
        if run_task:
            payload["env"]["TASKCLUSTER_CACHES"] = ";".join(sorted(caches.values()))

        payload["cache"] = caches

    # And send down volumes information to run-task as well.
    if run_task and worker.get("volumes"):
        payload["env"]["TASKCLUSTER_VOLUMES"] = ";".join(sorted(worker["volumes"]))

    if payload.get("cache") and skip_untrusted:
        payload["env"]["TASKCLUSTER_UNTRUSTED_CACHES"] = "1"

    if features:
        payload["features"] = features
    if capabilities:
        payload["capabilities"] = capabilities

    check_caches_are_volumes(task)


@payload_builder(
    "generic-worker",
    schema={
        Required("os"): Any("windows", "macosx", "linux", "linux-bitbar"),
        # see http://schemas.taskcluster.net/generic-worker/v1/payload.json
        # and https://docs.taskcluster.net/reference/workers/generic-worker/payload
        # command is a list of commands to run, sequentially
        # on Windows, each command is a string, on OS X and Linux, each command is
        # a string array
        Required("command"): Any(
            [taskref_or_string], [[taskref_or_string]]  # Windows  # Linux / OS X
        ),
        # artifacts to extract from the task image after completion; note that artifacts
        # for the generic worker cannot have names
        Optional("artifacts"): [
            {
                # type of artifact -- simple file, or recursive directory
                "type": Any("file", "directory"),
                # filesystem path from which to read artifact
                "path": str,
                # if not specified, path is used for artifact name
                Optional("name"): str,
            }
        ],
        # Directories and/or files to be mounted.
        # The actual allowed combinations are stricter than the model below,
        # but this provides a simple starting point.
        # See https://docs.taskcluster.net/reference/workers/generic-worker/payload
        Optional("mounts"): [
            {
                # A unique name for the cache volume, implies writable cache directory
                # (otherwise mount is a read-only file or directory).
                Optional("cache-name"): str,
                # Optional content for pre-loading cache, or mandatory content for
                # read-only file or directory. Pre-loaded content can come from either
                # a task artifact or from a URL.
                Optional("content"): {
                    # *** Either (artifact and task-id) or url must be specified. ***
                    # Artifact name that contains the content.
                    Optional("artifact"): str,
                    # Task ID that has the artifact that contains the content.
                    Optional("task-id"): taskref_or_string,
                    # URL that supplies the content in response to an unauthenticated
                    # GET request.
                    Optional("url"): str,
                },
                # *** Either file or directory must be specified. ***
                # If mounting a cache or read-only directory, the filesystem location of
                # the directory should be specified as a relative path to the task
                # directory here.
                Optional("directory"): str,
                # If mounting a file, specify the relative path within the task
                # directory to mount the file (the file will be read only).
                Optional("file"): str,
                # Required if and only if `content` is specified and mounting a
                # directory (not a file). This should be the archive format of the
                # content (either pre-loaded cache or read-only directory).
                Optional("format"): Any("rar", "tar.bz2", "tar.gz", "zip"),
            }
        ],
        # environment variables
        Required("env"): {str: taskref_or_string},
        # the maximum time to run, in seconds
        Required("max-run-time"): int,
        # the exit status code(s) that indicates the task should be retried
        Optional("retry-exit-status"): [int],
        # the exit status code(s) that indicates the caches used by the task
        # should be purged
        Optional("purge-caches-exit-status"): [int],
        # os user groups for test task workers
        Optional("os-groups"): [str],
        # feature for test task to run as administarotr
        Optional("run-as-administrator"): bool,
        # optional features
        Required("chain-of-trust"): bool,
        Optional("taskcluster-proxy"): bool,
        # Whether any artifacts are assigned to this worker
        Optional("skip-artifacts"): bool,
    },
)
def build_generic_worker_payload(config, task, task_def):
    worker = task["worker"]

    task_def["payload"] = {
        "command": worker["command"],
        "maxRunTime": worker["max-run-time"],
    }

    on_exit_status = {}
    if "retry-exit-status" in worker:
        on_exit_status["retry"] = worker["retry-exit-status"]
    if "purge-caches-exit-status" in worker:
        on_exit_status["purgeCaches"] = worker["purge-caches-exit-status"]
    if worker["os"] == "windows":
        on_exit_status.setdefault("retry", []).extend(
            [
                # These codes (on windows) indicate a process interruption,
                # rather than a task run failure. See bug 1544403.
                1073807364,  # process force-killed due to system shutdown
                3221225786,  # sigint (any interrupt)
            ]
        )
    if on_exit_status:
        task_def["payload"]["onExitStatus"] = on_exit_status

    env = worker.get("env", {})

    if task.get("needs-sccache"):
        env["USE_SCCACHE"] = "1"
        # Disable sccache idle shutdown.
        env["SCCACHE_IDLE_TIMEOUT"] = "0"
    else:
        env["SCCACHE_DISABLE"] = "1"

    if env:
        task_def["payload"]["env"] = env

    artifacts = []

    for artifact in worker.get("artifacts", []):
        a = {
            "path": artifact["path"],
            "type": artifact["type"],
        }
        if "name" in artifact:
            a["name"] = artifact["name"]
        artifacts.append(a)

    if artifacts:
        task_def["payload"]["artifacts"] = artifacts

    # Need to copy over mounts, but rename keys to respect naming convention
    #   * 'cache-name' -> 'cacheName'
    #   * 'task-id'    -> 'taskId'
    # All other key names are already suitable, and don't need renaming.
    mounts = deepcopy(worker.get("mounts", []))
    for mount in mounts:
        if "cache-name" in mount:
            mount["cacheName"] = "{trust_domain}-level-{level}-{name}".format(
                trust_domain=config.graph_config["trust-domain"],
                level=config.params["level"],
                name=mount.pop("cache-name"),
            )
            task_def["scopes"].append(
                "generic-worker:cache:{}".format(mount["cacheName"])
            )
        if "content" in mount:
            if "task-id" in mount["content"]:
                mount["content"]["taskId"] = mount["content"].pop("task-id")
            if "artifact" in mount["content"]:
                if not mount["content"]["artifact"].startswith("public/"):
                    task_def["scopes"].append(
                        "queue:get-artifact:{}".format(mount["content"]["artifact"])
                    )

    if mounts:
        task_def["payload"]["mounts"] = mounts

    if worker.get("os-groups"):
        task_def["payload"]["osGroups"] = worker["os-groups"]
        task_def["scopes"].extend(
            [
                "generic-worker:os-group:{}/{}".format(task["worker-type"], group)
                for group in worker["os-groups"]
            ]
        )

    features = {}

    if worker.get("chain-of-trust"):
        features["chainOfTrust"] = True

    if worker.get("taskcluster-proxy"):
        features["taskclusterProxy"] = True

    if worker.get("run-as-administrator", False):
        features["runAsAdministrator"] = True
        task_def["scopes"].append(
            "generic-worker:run-as-administrator:{}".format(task["worker-type"]),
        )

    if features:
        task_def["payload"]["features"] = features


@payload_builder(
    "beetmover",
    schema={
        # the maximum time to run, in seconds
        Required("max-run-time"): int,
        # locale key, if this is a locale beetmover job
        Optional("locale"): str,
        Optional("partner-public"): bool,
        Required("release-properties"): {
            "app-name": str,
            "app-version": str,
            "branch": str,
            "build-id": str,
            "hash-type": str,
            "platform": str,
        },
        # list of artifact URLs for the artifacts that should be beetmoved
        Required("upstream-artifacts"): [
            {
                # taskId of the task with the artifact
                Required("taskId"): taskref_or_string,
                # type of signing task (for CoT)
                Required("taskType"): str,
                # Paths to the artifacts to sign
                Required("paths"): [str],
                # locale is used to map upload path and allow for duplicate simple names
                Required("locale"): str,
            }
        ],
        Optional("artifact-map"): object,
    },
)
def build_beetmover_payload(config, task, task_def):
    worker = task["worker"]
    release_properties = worker["release-properties"]

    task_def["payload"] = {
        "maxRunTime": worker["max-run-time"],
        "releaseProperties": {
            "appName": release_properties["app-name"],
            "appVersion": release_properties["app-version"],
            "branch": release_properties["branch"],
            "buildid": release_properties["build-id"],
            "hashType": release_properties["hash-type"],
            "platform": release_properties["platform"],
        },
        "upload_date": config.params["build_date"],
        "upstreamArtifacts": worker["upstream-artifacts"],
    }
    if worker.get("locale"):
        task_def["payload"]["locale"] = worker["locale"]
    if worker.get("artifact-map"):
        task_def["payload"]["artifactMap"] = worker["artifact-map"]
    if worker.get("partner-public"):
        task_def["payload"]["is_partner_repack_public"] = worker["partner-public"]


@payload_builder(
    "invalid",
    schema={
        # an invalid task is one which should never actually be created; this is used in
        # release automation on branches where the task just doesn't make sense
        Extra: object,
    },
)
def build_invalid_payload(config, task, task_def):
    task_def["payload"] = "invalid task - should never be created"


@payload_builder(
    "always-optimized",
    schema={
        Extra: object,
    },
)
@payload_builder("succeed", schema={})
def build_dummy_payload(config, task, task_def):
    task_def["payload"] = {}


transforms = TransformSequence()


@transforms.add
def set_implementation(config, tasks):
    """
    Set the worker implementation based on the worker-type alias.
    """
    for task in tasks:
        worker = task.setdefault("worker", {})
        if "implementation" in task["worker"]:
            yield task
            continue

        impl, os = worker_type_implementation(config.graph_config, task["worker-type"])

        tags = task.setdefault("tags", {})
        tags["worker-implementation"] = impl
        if os:
            task["tags"]["os"] = os
        worker["implementation"] = impl
        if os:
            worker["os"] = os

        yield task


@transforms.add
def set_defaults(config, tasks):
    for task in tasks:
        task.setdefault("always-target", False)
        task.setdefault("optimization", None)
        task.setdefault("needs-sccache", False)

        worker = task["worker"]
        if worker["implementation"] in ("docker-worker",):
            worker.setdefault("relengapi-proxy", False)
            worker.setdefault("chain-of-trust", False)
            worker.setdefault("taskcluster-proxy", False)
            worker.setdefault("allow-ptrace", False)
            worker.setdefault("loopback-video", False)
            worker.setdefault("loopback-audio", False)
            worker.setdefault("docker-in-docker", False)
            worker.setdefault("privileged", False)
            worker.setdefault("volumes", [])
            worker.setdefault("env", {})
            if "caches" in worker:
                for c in worker["caches"]:
                    c.setdefault("skip-untrusted", False)
        elif worker["implementation"] == "generic-worker":
            worker.setdefault("env", {})
            worker.setdefault("os-groups", [])
            if worker["os-groups"] and worker["os"] != "windows":
                raise Exception(
                    "os-groups feature of generic-worker is only supported on "
                    "Windows, not on {}".format(worker["os"])
                )
            worker.setdefault("chain-of-trust", False)
        elif worker["implementation"] in (
            "scriptworker-signing",
            "beetmover",
            "beetmover-push-to-release",
            "beetmover-maven",
        ):
            worker.setdefault("max-run-time", 600)
        elif worker["implementation"] == "push-apk":
            worker.setdefault("commit", False)

        yield task


@transforms.add
def task_name_from_label(config, tasks):
    for task in tasks:
        if "label" not in task:
            if "name" not in task:
                raise Exception("task has neither a name nor a label")
            task["label"] = "{}-{}".format(config.kind, task["name"])
        if task.get("name"):
            del task["name"]
        yield task


@transforms.add
def validate(config, tasks):
    for task in tasks:
        validate_schema(
            task_description_schema,
            task,
            "In task {!r}:".format(task.get("label", "?no-label?")),
        )
        validate_schema(
            payload_builders[task["worker"]["implementation"]].schema,
            task["worker"],
            "In task.run {!r}:".format(task.get("label", "?no-label?")),
        )
        yield task


@index_builder("generic")
def add_generic_index_routes(config, task):
    index = task.get("index")
    routes = task.setdefault("routes", [])

    verify_index(config, index)

    subs = config.params.copy()
    subs["job-name"] = index["job-name"]
    subs["build_date_long"] = time.strftime(
        "%Y.%m.%d.%Y%m%d%H%M%S", time.gmtime(config.params["build_date"])
    )
    subs["product"] = index["product"]
    subs["trust-domain"] = config.graph_config["trust-domain"]
    subs["branch_rev"] = get_branch_rev(config)

    for tpl in V2_ROUTE_TEMPLATES:
        routes.append(tpl.format(**subs))

    return task


@transforms.add
def process_treeherder_metadata(config, tasks):
    for task in tasks:
        routes = task.get("routes", [])
        extra = task.get("extra", {})
        task_th = task.get("treeherder")

        if task_th:
            # This `merged_th` object is just an intermediary that combines
            # the defaults and whatever is in the task. Ultimately, the task
            # transforms this data a bit in the `treeherder` object that is
            # eventually set in the task.
            merged_th = treeherder_defaults(config.kind, task["label"])
            if isinstance(task_th, dict):
                merged_th.update(task_th)

            treeherder = extra.setdefault("treeherder", {})
            extra.setdefault("treeherder-platform", merged_th["platform"])

            machine_platform, collection = merged_th["platform"].split("/", 1)
            treeherder["machine"] = {"platform": machine_platform}
            treeherder["collection"] = {collection: True}

            group_names = config.graph_config["treeherder"]["group-names"]
            groupSymbol, symbol = split_symbol(merged_th["symbol"])
            if groupSymbol != "?":
                treeherder["groupSymbol"] = groupSymbol
                if groupSymbol not in group_names:
                    path = os.path.join(config.path, task.get("task-from", ""))
                    raise Exception(UNKNOWN_GROUP_NAME.format(groupSymbol, path))
                treeherder["groupName"] = group_names[groupSymbol]
            treeherder["symbol"] = symbol
            if len(symbol) > 25 or len(groupSymbol) > 25:
                raise RuntimeError(
                    "Treeherder group and symbol names must not be longer than "
                    "25 characters: {} (see {})".format(
                        treeherder["symbol"],
                        TC_TREEHERDER_SCHEMA_URL,
                    )
                )
            treeherder["jobKind"] = merged_th["kind"]
            treeherder["tier"] = merged_th["tier"]

            branch_rev = get_branch_rev(config)

            if config.params["tasks_for"].startswith("github-pull-request"):
                # In the past we used `project` for this, but that ends up being
                # set to the repository name of the _head_ repo, which is not correct
                # (and causes scope issues) if it doesn't match the name of the
                # base repo
                base_project = config.params["base_repository"].split("/")[-1]
                if base_project.endswith(".git"):
                    base_project = base_project[:-4]
                th_project_suffix = "-pr"
            else:
                base_project = config.params["project"]
                th_project_suffix = ""

            routes.append(
                "{}.v2.{}.{}.{}".format(
                    TREEHERDER_ROUTE_ROOT,
                    base_project + th_project_suffix,
                    branch_rev,
                    config.params["pushlog_id"],
                )
            )

        task["routes"] = routes
        task["extra"] = extra
        yield task


@transforms.add
def add_index_routes(config, tasks):
    for task in tasks:
        index = task.get("index", {})

        # The default behavior is to rank tasks according to their tier
        extra_index = task.setdefault("extra", {}).setdefault("index", {})
        rank = index.get("rank", "by-tier")

        if rank == "by-tier":
            # rank is zero for non-tier-1 tasks and based on pushid for others;
            # this sorts tier-{2,3} builds below tier-1 in the index
            tier = task.get("extra", {}).get("treeherder", {}).get("tier", 3)
            extra_index["rank"] = 0 if tier > 1 else int(config.params["build_date"])
        elif rank == "build_date":
            extra_index["rank"] = int(config.params["build_date"])
        else:
            extra_index["rank"] = rank

        if not index:
            yield task
            continue

        index_type = index.get("type", "generic")
        if index_type not in index_builders:
            raise ValueError(f"Unknown index-type {index_type}")
        task = index_builders[index_type](config, task)

        del task["index"]
        yield task


@transforms.add
def build_task(config, tasks):
    for task in tasks:
        level = str(config.params["level"])

        provisioner_id, worker_type = get_worker_type(
            config.graph_config,
            task["worker-type"],
            level,
        )
        task["worker-type"] = "/".join([provisioner_id, worker_type])
        project = config.params["project"]

        routes = task.get("routes", [])
        scopes = [
            s.format(level=level, project=project) for s in task.get("scopes", [])
        ]

        # set up extra
        extra = task.get("extra", {})
        extra["parent"] = os.environ.get("TASK_ID", "")

        if "expires-after" not in task:
            task["expires-after"] = "28 days" if config.params.is_try() else "1 year"

        if "deadline-after" not in task:
            if "task-deadline-after" in config.graph_config:
                task["deadline-after"] = get_default_deadline(
                    config.graph_config, config.params["project"]
                )
            else:
                task["deadline-after"] = "1 day"

        if "priority" not in task:
            task["priority"] = get_default_priority(
                config.graph_config, config.params["project"]
            )

        tags = task.get("tags", {})
        tags.update(
            {
                "createdForUser": config.params["owner"],
                "kind": config.kind,
                "label": task["label"],
            }
        )

        task_def = {
            "provisionerId": provisioner_id,
            "workerType": worker_type,
            "routes": routes,
            "created": {"relative-datestamp": "0 seconds"},
            "deadline": {"relative-datestamp": task["deadline-after"]},
            "expires": {"relative-datestamp": task["expires-after"]},
            "scopes": scopes,
            "metadata": {
                "description": task["description"],
                "name": task["label"],
                "owner": config.params["owner"],
                "source": config.params.file_url(config.path, pretty=True),
            },
            "extra": extra,
            "tags": tags,
            "priority": task["priority"],
        }

        if task.get("requires", None):
            task_def["requires"] = task["requires"]

        if task.get("extra", {}).get("treeherder"):
            branch_rev = get_branch_rev(config)
            if config.params["tasks_for"].startswith("github-pull-request"):
                # In the past we used `project` for this, but that ends up being
                # set to the repository name of the _head_ repo, which is not correct
                # (and causes scope issues) if it doesn't match the name of the
                # base repo
                base_project = config.params["base_repository"].split("/")[-1]
                if base_project.endswith(".git"):
                    base_project = base_project[:-4]
                th_project_suffix = "-pr"
            else:
                base_project = config.params["project"]
                th_project_suffix = ""

            # link back to treeherder in description
            th_push_link = (
                "https://treeherder.mozilla.org/#/jobs?repo={}&revision={}".format(
                    config.params["project"] + th_project_suffix, branch_rev
                )
            )
            task_def["metadata"]["description"] += " ([Treeherder push]({}))".format(
                th_push_link
            )

        # add the payload and adjust anything else as required (e.g., scopes)
        payload_builders[task["worker"]["implementation"]].builder(
            config, task, task_def
        )

        attributes = task.get("attributes", {})
        # Resolve run-on-projects
        build_platform = attributes.get("build_platform")
        resolve_keyed_by(
            task,
            "run-on-projects",
            item_name=task["label"],
            **{"build-platform": build_platform},
        )
        attributes["run_on_projects"] = task.get("run-on-projects", ["all"])
        attributes["run_on_tasks_for"] = task.get("run-on-tasks-for", ["all"])
        # We don't want to pollute non git repos with this attribute. Moreover, target_tasks
        # already assumes the default value is ['all']
        if task.get("run-on-git-branches"):
            attributes["run_on_git_branches"] = task["run-on-git-branches"]

        attributes["always_target"] = task["always-target"]
        # This logic is here since downstream tasks don't always match their
        # upstream dependency's shipping_phase.
        # A text_type task['shipping-phase'] takes precedence, then
        # an existing attributes['shipping_phase'], then fall back to None.
        if task.get("shipping-phase") is not None:
            attributes["shipping_phase"] = task["shipping-phase"]
        else:
            attributes.setdefault("shipping_phase", None)

        # Set MOZ_AUTOMATION on all jobs.
        if task["worker"]["implementation"] in (
            "generic-worker",
            "docker-worker",
        ):
            payload = task_def.get("payload")
            if payload:
                env = payload.setdefault("env", {})
                env["MOZ_AUTOMATION"] = "1"

        dependencies = task.get("dependencies", {})
        if_dependencies = task.get("if-dependencies", [])
        if if_dependencies:
            for i, dep in enumerate(if_dependencies):
                if dep in dependencies:
                    if_dependencies[i] = dependencies[dep]
                    continue

                raise Exception(
                    "{label} specifies '{dep}' in if-dependencies, "
                    "but {dep} is not a dependency!".format(
                        label=task["label"], dep=dep
                    )
                )

        yield {
            "label": task["label"],
            "description": task["description"],
            "task": task_def,
            "dependencies": dependencies,
            "if-dependencies": if_dependencies,
            "soft-dependencies": task.get("soft-dependencies", []),
            "attributes": attributes,
            "optimization": task.get("optimization", None),
        }


@transforms.add
def add_github_checks(config, tasks):
    """
    For git repositories, add checks route to all tasks.

    This will be replaced by a configurable option in the future.
    """
    if config.params["repository_type"] != "git":
        for task in tasks:
            yield task

    for task in tasks:
        task["task"]["routes"].append("checks")
        yield task


@transforms.add
def chain_of_trust(config, tasks):
    for task in tasks:
        if task["task"].get("payload", {}).get("features", {}).get("chainOfTrust"):
            image = task.get("dependencies", {}).get("docker-image")
            if image:
                cot = (
                    task["task"].setdefault("extra", {}).setdefault("chainOfTrust", {})
                )
                cot.setdefault("inputs", {})["docker-image"] = {
                    "task-reference": "<docker-image>"
                }
        yield task


@transforms.add
def check_task_identifiers(config, tasks):
    """Ensures that all tasks have well defined identifiers:
    ``^[a-zA-Z0-9_-]{1,38}$``
    """
    e = re.compile("^[a-zA-Z0-9_-]{1,38}$")
    for task in tasks:
        for attrib in ("workerType", "provisionerId"):
            if not e.match(task["task"][attrib]):
                raise Exception(
                    "task {}.{} is not a valid identifier: {}".format(
                        task["label"], attrib, task["task"][attrib]
                    )
                )
        yield task


@transforms.add
def check_task_dependencies(config, tasks):
    """Ensures that tasks don't have more than 100 dependencies."""
    for task in tasks:
        number_of_dependencies = (
            len(task["dependencies"])
            + len(task["if-dependencies"])
            + len(task["soft-dependencies"])
        )
        if number_of_dependencies > MAX_DEPENDENCIES:
            raise Exception(
                "task {}/{} has too many dependencies ({} > {})".format(
                    config.kind,
                    task["label"],
                    number_of_dependencies,
                    MAX_DEPENDENCIES,
                )
            )
        yield task


def check_caches_are_volumes(task):
    """Ensures that all cache paths are defined as volumes.

    Caches and volumes are the only filesystem locations whose content
    isn't defined by the Docker image itself. Some caches are optional
    depending on the job environment. We want paths that are potentially
    caches to have as similar behavior regardless of whether a cache is
    used. To help enforce this, we require that all paths used as caches
    to be declared as Docker volumes. This check won't catch all offenders.
    But it is better than nothing.
    """
    volumes = set(task["worker"]["volumes"])
    paths = {c["mount-point"] for c in task["worker"].get("caches", [])}
    missing = paths - volumes

    if not missing:
        return

    raise Exception(
        "task {} (image {}) has caches that are not declared as "
        "Docker volumes: {} "
        "(have you added them as VOLUMEs in the Dockerfile?)".format(
            task["label"], task["worker"]["docker-image"], ", ".join(sorted(missing))
        )
    )


@transforms.add
def check_run_task_caches(config, tasks):
    """Audit for caches requiring run-task.

    run-task manages caches in certain ways. If a cache managed by run-task
    is used by a non run-task task, it could cause problems. So we audit for
    that and make sure certain cache names are exclusive to run-task.

    IF YOU ARE TEMPTED TO MAKE EXCLUSIONS TO THIS POLICY, YOU ARE LIKELY
    CONTRIBUTING TECHNICAL DEBT AND WILL HAVE TO SOLVE MANY OF THE PROBLEMS
    THAT RUN-TASK ALREADY SOLVES. THINK LONG AND HARD BEFORE DOING THAT.
    """
    re_reserved_caches = re.compile(
        """^
        (checkouts|tooltool-cache)
    """,
        re.VERBOSE,
    )

    cache_prefix = "{trust_domain}-level-{level}-".format(
        trust_domain=config.graph_config["trust-domain"],
        level=config.params["level"],
    )

    suffix = _run_task_suffix()

    for task in tasks:
        payload = task["task"].get("payload", {})
        command = payload.get("command") or [""]

        main_command = command[0] if isinstance(command[0], str) else ""
        run_task = main_command.endswith("run-task")

        for cache in payload.get("cache", {}):
            if not cache.startswith(cache_prefix):
                raise Exception(
                    "{} is using a cache ({}) which is not appropriate "
                    "for its trust-domain and level. It should start with {}.".format(
                        task["label"], cache, cache_prefix
                    )
                )

            cache = cache[len(cache_prefix) :]

            if not re_reserved_caches.match(cache):
                continue

            if not run_task:
                raise Exception(
                    f"{task['label']} is using a cache ({cache}) reserved for run-task "
                    "change the task to use run-task or use a different "
                    "cache name"
                )

            if not cache.endswith(suffix):
                raise Exception(
                    f"{task['label']} is using a cache ({cache}) reserved for run-task "
                    "but the cache name is not dependent on the contents "
                    "of run-task; change the cache name to conform to the "
                    "naming requirements"
                )

        yield task
