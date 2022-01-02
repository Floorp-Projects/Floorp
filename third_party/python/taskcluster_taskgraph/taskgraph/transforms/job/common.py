# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Common support for various job types.  These functions are all named after the
worker implementation they operate on, and take the same three parameters, for
consistency.
"""


import hashlib
import json

from taskgraph.util.taskcluster import get_artifact_prefix


def get_vcsdir_name(os):
    if os == "windows":
        return "src"
    else:
        return "vcs"


def add_cache(job, taskdesc, name, mount_point, skip_untrusted=False):
    """Adds a cache based on the worker's implementation.

    Args:
        job (dict): Task's job description.
        taskdesc (dict): Target task description to modify.
        name (str): Name of the cache.
        mount_point (path): Path on the host to mount the cache.
        skip_untrusted (bool): Whether cache is used in untrusted environments
            (default: False). Only applies to docker-worker.
    """
    if not job["run"].get("use-caches", True):
        return

    worker = job["worker"]

    if worker["implementation"] == "docker-worker":
        taskdesc["worker"].setdefault("caches", []).append(
            {
                "type": "persistent",
                "name": name,
                "mount-point": mount_point,
                "skip-untrusted": skip_untrusted,
            }
        )

    elif worker["implementation"] == "generic-worker":
        taskdesc["worker"].setdefault("mounts", []).append(
            {
                "cache-name": name,
                "directory": mount_point,
            }
        )

    else:
        # Caches not implemented
        pass


def docker_worker_add_workspace_cache(config, job, taskdesc, extra=None):
    """Add the workspace cache.

    Args:
        config (TransformConfig): Transform configuration object.
        job (dict): Task's job description.
        taskdesc (dict): Target task description to modify.
        extra (str): Optional context passed in that supports extending the cache
            key name to avoid undesired conflicts with other caches.
    """
    cache_name = "{}-build-{}-{}-workspace".format(
        config.params["project"],
        taskdesc["attributes"]["build_platform"],
        taskdesc["attributes"]["build_type"],
    )
    if extra:
        cache_name = f"{cache_name}-{extra}"

    mount_point = "{workdir}/workspace".format(**job["run"])

    # Don't enable the workspace cache when we can't guarantee its
    # behavior, like on Try.
    add_cache(job, taskdesc, cache_name, mount_point, skip_untrusted=True)


def add_artifacts(config, job, taskdesc, path):
    taskdesc["worker"].setdefault("artifacts", []).append(
        {
            "name": get_artifact_prefix(taskdesc),
            "path": path,
            "type": "directory",
        }
    )


def docker_worker_add_artifacts(config, job, taskdesc):
    """Adds an artifact directory to the task"""
    path = "{workdir}/artifacts/".format(**job["run"])
    taskdesc["worker"]["env"]["UPLOAD_DIR"] = path
    add_artifacts(config, job, taskdesc, path)


def generic_worker_add_artifacts(config, job, taskdesc):
    """Adds an artifact directory to the task"""
    # The path is the location on disk; it doesn't necessarily
    # mean the artifacts will be public or private; that is set via the name
    # attribute in add_artifacts.
    add_artifacts(config, job, taskdesc, path=get_artifact_prefix(taskdesc))


def support_vcs_checkout(config, job, taskdesc, repo_configs, sparse=False):
    """Update a job/task with parameters to enable a VCS checkout.

    This can only be used with ``run-task`` tasks, as the cache name is
    reserved for ``run-task`` tasks.
    """
    worker = job["worker"]
    is_mac = worker["os"] == "macosx"
    is_win = worker["os"] == "windows"
    is_linux = worker["os"] == "linux"
    is_docker = worker["implementation"] == "docker-worker"
    assert is_mac or is_win or is_linux

    if is_win:
        checkoutdir = "./build"
        hgstore = "y:/hg-shared"
    elif is_docker:
        checkoutdir = "{workdir}/checkouts".format(**job["run"])
        hgstore = f"{checkoutdir}/hg-store"
    else:
        checkoutdir = "./checkouts"
        hgstore = f"{checkoutdir}/hg-shared"

    vcsdir = checkoutdir + "/" + get_vcsdir_name(worker["os"])
    cache_name = "checkouts"

    # Robust checkout does not clean up subrepositories, so ensure  that tasks
    # that checkout different sets of paths have separate caches.
    # See https://bugzilla.mozilla.org/show_bug.cgi?id=1631610
    if len(repo_configs) > 1:
        checkout_paths = {
            "\t".join([repo_config.path, repo_config.prefix])
            for repo_config in sorted(
                repo_configs.values(), key=lambda repo_config: repo_config.path
            )
        }
        checkout_paths_str = "\n".join(checkout_paths).encode("utf-8")
        digest = hashlib.sha256(checkout_paths_str).hexdigest()
        cache_name += f"-repos-{digest}"

    # Sparse checkouts need their own cache because they can interfere
    # with clients that aren't sparse aware.
    if sparse:
        cache_name += "-sparse"

    # Workers using Mercurial >= 5.8 will enable revlog-compression-zstd, which
    # workers using older versions can't understand, so they can't share cache.
    # At the moment, only docker workers use the newer version.
    if is_docker:
        cache_name += "-hg58"

    add_cache(job, taskdesc, cache_name, checkoutdir)

    env = taskdesc["worker"].setdefault("env", {})
    env.update(
        {
            "HG_STORE_PATH": hgstore,
            "REPOSITORIES": json.dumps(
                {repo.prefix: repo.name for repo in repo_configs.values()}
            ),
            "VCS_PATH": vcsdir,
        }
    )
    for repo_config in repo_configs.values():
        env.update(
            {
                f"{repo_config.prefix.upper()}_{key}": value
                for key, value in {
                    "BASE_REPOSITORY": repo_config.base_repository,
                    "HEAD_REPOSITORY": repo_config.head_repository,
                    "HEAD_REV": repo_config.head_rev,
                    "HEAD_REF": repo_config.head_ref,
                    "REPOSITORY_TYPE": repo_config.type,
                    "SSH_SECRET_NAME": repo_config.ssh_secret_name,
                }.items()
                if value is not None
            }
        )
        if repo_config.ssh_secret_name:
            taskdesc["scopes"].append(f"secrets:get:{repo_config.ssh_secret_name}")

    if any(repo_config.type == "hg" for repo_config in repo_configs.values()):
        # Give task access to hgfingerprint secret so it can pin the certificate
        # for hg.mozilla.org.
        taskdesc["scopes"].append("secrets:get:project/taskcluster/gecko/hgfingerprint")

    # only some worker platforms have taskcluster-proxy enabled
    if job["worker"]["implementation"] in ("docker-worker",):
        taskdesc["worker"]["taskcluster-proxy"] = True
