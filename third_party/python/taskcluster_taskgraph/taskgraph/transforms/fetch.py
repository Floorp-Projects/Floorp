# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Support for running tasks that download remote content and re-export
# it as task artifacts.


import os
import re
from dataclasses import dataclass
from typing import Callable

from voluptuous import Extra, Optional, Required

import taskgraph

from ..util import path
from ..util.cached_tasks import add_optimization
from ..util.schema import Schema, validate_schema
from ..util.treeherder import join_symbol
from .base import TransformSequence

CACHE_TYPE = "content.v1"

FETCH_SCHEMA = Schema(
    {
        # Name of the task.
        Required("name"): str,
        # Relative path (from config.path) to the file the task was defined
        # in.
        Optional("task-from"): str,
        # Description of the task.
        Required("description"): str,
        Optional("docker-image"): object,
        Optional(
            "fetch-alias",
            description="An alias that can be used instead of the real fetch job name in "
            "fetch stanzas for jobs.",
        ): str,
        Optional(
            "artifact-prefix",
            description="The prefix of the taskcluster artifact being uploaded. "
            "Defaults to `public/`; if it starts with something other than "
            "`public/` the artifact will require scopes to access.",
        ): str,
        Optional("attributes"): {str: object},
        Required("fetch"): {
            Required("type"): str,
            Extra: object,
        },
    }
)


# define a collection of payload builders, depending on the worker implementation
fetch_builders = {}


@dataclass(frozen=True)
class FetchBuilder:
    schema: Schema
    builder: Callable


def fetch_builder(name, schema):
    schema = Schema({Required("type"): name}).extend(schema)

    def wrap(func):
        fetch_builders[name] = FetchBuilder(schema, func)
        return func

    return wrap


transforms = TransformSequence()
transforms.add_validate(FETCH_SCHEMA)


@transforms.add
def process_fetch_job(config, jobs):
    # Converts fetch-url entries to the job schema.
    for job in jobs:
        typ = job["fetch"]["type"]
        name = job["name"]
        fetch = job.pop("fetch")

        if typ not in fetch_builders:
            raise Exception(f"Unknown fetch type {typ} in fetch {name}")
        validate_schema(fetch_builders[typ].schema, fetch, f"In task.fetch {name!r}:")

        job.update(configure_fetch(config, typ, name, fetch))

        yield job


def configure_fetch(config, typ, name, fetch):
    if typ not in fetch_builders:
        raise Exception(f"No fetch type {typ} in fetch {name}")
    validate_schema(fetch_builders[typ].schema, fetch, f"In task.fetch {name!r}:")

    return fetch_builders[typ].builder(config, name, fetch)


@transforms.add
def make_task(config, jobs):
    # Fetch tasks are idempotent and immutable. Have them live for
    # essentially forever.
    if config.params["level"] == "3":
        expires = "1000 years"
    else:
        expires = "28 days"

    for job in jobs:
        name = job["name"]
        artifact_prefix = job.get("artifact-prefix", "public")
        env = job.get("env", {})
        env.update({"UPLOAD_DIR": "/builds/worker/artifacts"})
        attributes = job.get("attributes", {})
        attributes["fetch-artifact"] = path.join(artifact_prefix, job["artifact_name"])
        alias = job.get("fetch-alias")
        if alias:
            attributes["fetch-alias"] = alias

        task = {
            "attributes": attributes,
            "name": name,
            "description": job["description"],
            "expires-after": expires,
            "label": "fetch-%s" % name,
            "run-on-projects": [],
            "run": {
                "using": "run-task",
                "checkout": False,
                "command": job["command"],
            },
            "worker-type": "images",
            "worker": {
                "chain-of-trust": True,
                "docker-image": job.get("docker-image", {"in-tree": "fetch"}),
                "env": env,
                "max-run-time": 900,
                "artifacts": [
                    {
                        "type": "directory",
                        "name": artifact_prefix,
                        "path": "/builds/worker/artifacts",
                    }
                ],
            },
        }

        if "treeherder" in config.graph_config:
            task["treeherder"] = {
                "symbol": join_symbol("Fetch", name),
                "kind": "build",
                "platform": "fetch/opt",
                "tier": 1,
            }

        if job.get("secret", None):
            task["scopes"] = ["secrets:get:" + job.get("secret")]
            task["worker"]["taskcluster-proxy"] = True

        if not taskgraph.fast:
            cache_name = task["label"].replace(f"{config.kind}-", "", 1)

            # This adds the level to the index path automatically.
            add_optimization(
                config,
                task,
                cache_type=CACHE_TYPE,
                cache_name=cache_name,
                digest_data=job["digest_data"],
            )
        yield task


@fetch_builder(
    "static-url",
    schema={
        # The URL to download.
        Required("url"): str,
        # The SHA-256 of the downloaded content.
        Required("sha256"): str,
        # Size of the downloaded entity, in bytes.
        Required("size"): int,
        # GPG signature verification.
        Optional("gpg-signature"): {
            # URL where GPG signature document can be obtained. Can contain the
            # value ``{url}``, which will be substituted with the value from
            # ``url``.
            Required("sig-url"): str,
            # Path to file containing GPG public key(s) used to validate
            # download.
            Required("key-path"): str,
        },
        # The name to give to the generated artifact. Defaults to the file
        # portion of the URL. Using a different extension converts the
        # archive to the given type. Only conversion to .tar.zst is
        # supported.
        Optional("artifact-name"): str,
        # Strip the given number of path components at the beginning of
        # each file entry in the archive.
        # Requires an artifact-name ending with .tar.zst.
        Optional("strip-components"): int,
        # Add the given prefix to each file entry in the archive.
        # Requires an artifact-name ending with .tar.zst.
        Optional("add-prefix"): str,
        # Headers to pass alongside the request.
        Optional("headers"): {
            str: str,
        },
        # IMPORTANT: when adding anything that changes the behavior of the task,
        # it is important to update the digest data used to compute cache hits.
    },
)
def create_fetch_url_task(config, name, fetch):
    artifact_name = fetch.get("artifact-name")
    if not artifact_name:
        artifact_name = fetch["url"].split("/")[-1]

    command = [
        "fetch-content",
        "static-url",
    ]

    # Arguments that matter to the cache digest
    args = [
        "--sha256",
        fetch["sha256"],
        "--size",
        "%d" % fetch["size"],
    ]

    if fetch.get("strip-components"):
        args.extend(["--strip-components", "%d" % fetch["strip-components"]])

    if fetch.get("add-prefix"):
        args.extend(["--add-prefix", fetch["add-prefix"]])

    command.extend(args)

    env = {}

    if "gpg-signature" in fetch:
        sig_url = fetch["gpg-signature"]["sig-url"].format(url=fetch["url"])
        key_path = os.path.join(taskgraph.GECKO, fetch["gpg-signature"]["key-path"])

        with open(key_path) as fh:
            gpg_key = fh.read()

        env["FETCH_GPG_KEY"] = gpg_key
        command.extend(
            [
                "--gpg-sig-url",
                sig_url,
                "--gpg-key-env",
                "FETCH_GPG_KEY",
            ]
        )

    if "headers" in fetch:
        for k, v in fetch["headers"].items():
            command.extend(["-H", f"{k}:{v}"])

    command.extend(
        [
            fetch["url"],
            "/builds/worker/artifacts/%s" % artifact_name,
        ]
    )

    return {
        "command": command,
        "artifact_name": artifact_name,
        "env": env,
        # We don't include the GPG signature in the digest because it isn't
        # materially important for caching: GPG signatures are supplemental
        # trust checking beyond what the shasum already provides.
        "digest_data": args + [artifact_name],
    }


@fetch_builder(
    "git",
    schema={
        Required("repo"): str,
        Required("revision"): str,
        Optional("include-dot-git"): bool,
        Optional("artifact-name"): str,
        Optional("path-prefix"): str,
        # ssh-key is a taskcluster secret path (e.g. project/civet/github-deploy-key)
        # In the secret dictionary, the key should be specified as
        #  "ssh_privkey": "-----BEGIN OPENSSH PRIVATE KEY-----\nkfksnb3jc..."
        # n.b. The OpenSSH private key file format requires a newline at the end of the file.
        Optional("ssh-key"): str,
    },
)
def create_git_fetch_task(config, name, fetch):
    path_prefix = fetch.get("path-prefix")
    if not path_prefix:
        path_prefix = fetch["repo"].rstrip("/").rsplit("/", 1)[-1]
    artifact_name = fetch.get("artifact-name")
    if not artifact_name:
        artifact_name = f"{path_prefix}.tar.zst"

    if not re.match(r"[0-9a-fA-F]{40}", fetch["revision"]):
        raise Exception(f'Revision is not a sha1 in fetch task "{name}"')

    args = [
        "fetch-content",
        "git-checkout-archive",
        "--path-prefix",
        path_prefix,
        fetch["repo"],
        fetch["revision"],
        "/builds/worker/artifacts/%s" % artifact_name,
    ]

    ssh_key = fetch.get("ssh-key")
    if ssh_key:
        args.append("--ssh-key-secret")
        args.append(ssh_key)

    digest_data = [fetch["revision"], path_prefix, artifact_name]
    if fetch.get("include-dot-git", False):
        args.append("--include-dot-git")
        digest_data.append(".git")

    return {
        "command": args,
        "artifact_name": artifact_name,
        "digest_data": digest_data,
        "secret": ssh_key,
    }
