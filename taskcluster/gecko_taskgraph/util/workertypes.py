# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.util import memoize
from taskgraph.util.attributes import keymatch
from taskgraph.util.keyed_by import evaluate_keyed_by

from gecko_taskgraph.util.attributes import release_level as _release_level

WORKER_TYPES = {
    "gce/gecko-1-b-linux": ("docker-worker", "linux"),
    "gce/gecko-2-b-linux": ("docker-worker", "linux"),
    "gce/gecko-3-b-linux": ("docker-worker", "linux"),
    "invalid/invalid": ("invalid", None),
    "invalid/always-optimized": ("always-optimized", None),
    "scriptworker-prov-v1/signing-linux-v1": ("scriptworker-signing", None),
}


@memoize
def _get(graph_config, alias, level, release_level, project):
    """Get the configuration for this worker_type alias: {provisioner,
    worker-type, implementation, os}"""
    level = str(level)

    # handle the legacy (non-alias) format
    if "/" in alias:
        alias = alias.format(level=level)
        provisioner, worker_type = alias.split("/", 1)
        try:
            implementation, os = WORKER_TYPES[alias]
            return {
                "provisioner": provisioner,
                "worker-type": worker_type,
                "implementation": implementation,
                "os": os,
            }
        except KeyError:
            return {
                "provisioner": provisioner,
                "worker-type": worker_type,
            }

    matches = keymatch(graph_config["workers"]["aliases"], alias)
    if len(matches) > 1:
        raise KeyError("Multiple matches for worker-type alias " + alias)
    elif not matches:
        raise KeyError("No matches for worker-type alias " + alias)
    worker_config = matches[0].copy()

    worker_config["provisioner"] = evaluate_keyed_by(
        worker_config["provisioner"],
        f"worker-type alias {alias} field provisioner",
        {"level": level},
    ).format(
        **{
            "trust-domain": graph_config["trust-domain"],
            "level": level,
            "alias": alias,
        }
    )
    attrs = {"level": level, "release-level": release_level}
    if project:
        attrs["project"] = project
    worker_config["worker-type"] = evaluate_keyed_by(
        worker_config["worker-type"],
        f"worker-type alias {alias} field worker-type",
        attrs,
    ).format(
        **{
            "trust-domain": graph_config["trust-domain"],
            "level": level,
            "alias": alias,
        }
    )

    return worker_config


def worker_type_implementation(graph_config, parameters, worker_type):
    """Get the worker implementation and OS for the given workerType, where the
    OS represents the host system, not the target OS, in the case of
    cross-compiles."""
    worker_config = _get(
        graph_config, worker_type, "1", "staging", parameters["project"]
    )
    return worker_config["implementation"], worker_config.get("os")


def get_worker_type(graph_config, parameters, worker_type):
    """
    Get the worker type provisioner and worker-type, optionally evaluating
    aliases from the graph config.
    """
    worker_config = _get(
        graph_config,
        worker_type,
        parameters["level"],
        _release_level(parameters.get("project")),
        parameters.get("project"),
    )
    return worker_config["provisioner"], worker_config["worker-type"]
