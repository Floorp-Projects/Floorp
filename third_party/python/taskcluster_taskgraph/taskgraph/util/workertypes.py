# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from dataclasses import dataclass

from .keyed_by import evaluate_keyed_by
from .memoize import memoize


@dataclass
class _BuiltinWorkerType:
    provisioner: str
    worker_type: str

    @property
    def implementation(self):
        """
        Since the list of built-in worker-types is small and fixed, we can get
        away with punning the implementation name (in
        `taskgraph.transforms.task`) and the worker_type.
        """
        return self.worker_type


_BUILTIN_TYPES = {
    "always-optimized": _BuiltinWorkerType("invalid", "always-optimized"),
    "succeed": _BuiltinWorkerType("built-in", "succeed"),
}


@memoize
def worker_type_implementation(graph_config, worker_type):
    """Get the worker implementation and OS for the given workerType, where the
    OS represents the host system, not the target OS, in the case of
    cross-compiles."""
    if worker_type in _BUILTIN_TYPES:
        # For the built-in worker-types, we use an `implementation that matches
        # the worker-type.
        return _BUILTIN_TYPES[worker_type].implementation, None
    worker_config = evaluate_keyed_by(
        {"by-worker-type": graph_config["workers"]["aliases"]},
        "worker-types.yml",
        {"worker-type": worker_type},
    )
    return worker_config["implementation"], worker_config.get("os")


@memoize
def get_worker_type(graph_config, alias, level):
    """
    Get the worker type based, evaluating aliases from the graph config.
    """
    if alias in _BUILTIN_TYPES:
        builtin_type = _BUILTIN_TYPES[alias]
        return builtin_type.provisioner, builtin_type.worker_type

    level = str(level)
    worker_config = evaluate_keyed_by(
        {"by-alias": graph_config["workers"]["aliases"]},
        "graph_config.workers.aliases",
        {"alias": alias},
    )
    provisioner = evaluate_keyed_by(
        worker_config["provisioner"],
        alias,
        {"level": level},
    ).format(
        **{"alias": alias, "level": level, "trust-domain": graph_config["trust-domain"]}
    )
    worker_type = evaluate_keyed_by(
        worker_config["worker-type"],
        alias,
        {"level": level},
    ).format(
        **{"alias": alias, "level": level, "trust-domain": graph_config["trust-domain"]}
    )
    return provisioner, worker_type
