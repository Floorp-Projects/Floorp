# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozbuild.util import memoize

from .keyed_by import evaluate_keyed_by

WORKER_TYPES = {
    'gce/gecko-1-b-linux': ('docker-worker', 'linux'),
    'releng-hardware/gecko-1-b-win2012-gamma': ('generic-worker', 'windows'),
    'gce/gecko-2-b-linux': ('docker-worker', 'linux'),
    'releng-hardware/gecko-2-b-win2012-gamma': ('generic-worker', 'windows'),
    'gce/gecko-3-b-linux': ('docker-worker', 'linux'),
    'releng-hardware/gecko-3-b-win2012-gamma': ('generic-worker', 'windows'),
    'invalid/invalid': ('invalid', None),
    'invalid/always-optimized': ('always-optimized', None),
    'scriptworker-prov-v1/balrog-dev': ('balrog', None),
    'scriptworker-prov-v1/balrogworker-v1': ('balrog', None),
    'scriptworker-prov-v1/beetmoverworker-v1': ('beetmover', None),
    'scriptworker-prov-v1/pushapk-v1': ('push-apk', None),
    "scriptworker-prov-v1/signing-linux-v1": ('scriptworker-signing', None),
    "scriptworker-prov-v1/shipit": ('shipit', None),
    "scriptworker-prov-v1/shipit-dev": ('shipit', None),
    "scriptworker-prov-v1/treescript-v1": ('treescript', None),
    'terraform-packet/gecko-t-linux': ('docker-worker', 'linux'),
    'releng-hardware/gecko-t-osx-1014': ('generic-worker', 'macosx'),
}


@memoize
def worker_type_implementation(graph_config, worker_type):
    """Get the worker implementation and OS for the given workerType, where the
    OS represents the host system, not the target OS, in the case of
    cross-compiles."""
    if '/' in worker_type:
        worker_type = worker_type.replace('{level}', '1')
        return WORKER_TYPES[worker_type]
    else:
        worker_config = evaluate_keyed_by(
            {"by-worker-type": graph_config["workers"]["aliases"]},
            "worker-types.yml",
            {'worker-type': worker_type},
        )
        return worker_config['implementation'], worker_config['os']


@memoize
def get_worker_type(graph_config, worker_type, level):
    """
    Get the worker type based, evaluating aliases from the graph config.
    """
    level = str(level)
    if '/' in worker_type:
        worker_type = worker_type.format(level=level)
        return worker_type.split("/", 1)
    else:
        worker_config = evaluate_keyed_by(
            {"by-worker-type": graph_config["workers"]["aliases"]},
            "worker-types.yml",
            {"worker-type": worker_type},
        )
        worker_type = evaluate_keyed_by(
            worker_config["worker-type"],
            worker_type,
            {"level": level},
        ).format(level=level, alias=worker_type)
        return worker_config["provisioner"], worker_type
