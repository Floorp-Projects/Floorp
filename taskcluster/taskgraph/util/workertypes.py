# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozbuild.util import memoize

from .keyed_by import evaluate_keyed_by
from .attributes import keymatch

WORKER_TYPES = {
    'gce/gecko-1-b-linux': ('docker-worker', 'linux'),
    'gce/gecko-2-b-linux': ('docker-worker', 'linux'),
    'gce/gecko-3-b-linux': ('docker-worker', 'linux'),
    'invalid/invalid': ('invalid', None),
    'invalid/always-optimized': ('always-optimized', None),
    'scriptworker-prov-v1/pushapk-v1': ('push-apk', None),
    "scriptworker-prov-v1/signing-linux-v1": ('scriptworker-signing', None),
    "scriptworker-k8s/gecko-3-shipit": ('shipit', None),
    "scriptworker-k8s/gecko-1-shipit": ('shipit', None),
    'terraform-packet/gecko-t-linux': ('docker-worker', 'linux'),
}


@memoize
def _get(graph_config, alias, level, release_level):
    """Get the configuration for this worker_type alias: {provisioner,
    worker-type, implementation, os}"""
    level = str(level)

    # handle the legacy (non-alias) format
    if '/' in alias:
        alias = alias.format(level=level)
        provisioner, worker_type = alias.split("/", 1)
        try:
            implementation, os = WORKER_TYPES[alias]
            return {
                'provisioner': provisioner,
                'worker-type': worker_type,
                'implementation': implementation,
                'os': os,
            }
        except KeyError:
            return {
                'provisioner': provisioner,
                'worker-type': worker_type,
            }

    matches = keymatch(graph_config['workers']['aliases'], alias)
    if len(matches) > 1:
        raise KeyError("Multiple matches for worker-type alias " + alias)
    elif not matches:
        raise KeyError("No matches for worker-type alias " + alias)
    worker_config = matches[0].copy()

    worker_config['provisioner'] = evaluate_keyed_by(
        worker_config['provisioner'],
        "worker-type alias {} field provisioner".format(alias),
        {"level": level}).format(level=level, alias=alias)
    worker_config['worker-type'] = evaluate_keyed_by(
        worker_config['worker-type'],
        "worker-type alias {} field worker-type".format(alias),
        {"level": level, 'release-level': release_level}).format(level=level, alias=alias)

    return worker_config


def worker_type_implementation(graph_config, worker_type):
    """Get the worker implementation and OS for the given workerType, where the
    OS represents the host system, not the target OS, in the case of
    cross-compiles."""
    worker_config = _get(graph_config, worker_type, '1', 'staging')
    return worker_config['implementation'], worker_config.get('os')


def get_worker_type(graph_config, worker_type, level, release_level):
    """
    Get the worker type provisioner and worker-type, optionally evaluating
    aliases from the graph config.
    """
    worker_config = _get(graph_config, worker_type, level, release_level)
    return worker_config['provisioner'], worker_config['worker-type']
