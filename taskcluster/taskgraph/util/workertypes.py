# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

WORKER_TYPES = {
    'aws-provisioner-v1/gecko-1-b-android': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-1-b-linux': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-1-b-linux-large': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-1-b-linux-xlarge': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-1-b-macosx64': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-1-b-win2012': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-1-images': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-android': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-linux': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-linux-large': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-linux-xlarge': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-macosx64': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-2-b-win2012': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-2-images': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-android': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-linux': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-linux-large': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-linux-xlarge': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-macosx64': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-3-b-win2012': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-3-images': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-symbol-upload': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-t-linux-large': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-t-linux-medium': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-t-linux-xlarge': ('docker-worker', 'linux'),
    'aws-provisioner-v1/gecko-t-win10-64': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-t-win10-64-gpu': ('generic-worker', 'windows'),
    'releng-hardware/gecko-t-win10-64-hw': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-t-win7-32': ('generic-worker', 'windows'),
    'aws-provisioner-v1/gecko-t-win7-32-gpu': ('generic-worker', 'windows'),
    'releng-hardware/gecko-t-win7-32-hw': ('generic-worker', 'windows'),
    'aws-provisioner-v1/taskcluster-generic': ('docker-worker', 'linux'),
    'invalid/invalid': ('invalid', None),
    'invalid/always-optimized': ('always-optimized', None),
    'releng-hardware/gecko-t-linux-talos': ('native-engine', 'linux'),
    'scriptworker-prov-v1/balrog-dev': ('balrog', None),
    'scriptworker-prov-v1/balrogworker-v1': ('balrog', None),
    'scriptworker-prov-v1/beetmoverworker-v1': ('beetmover', None),
    'scriptworker-prov-v1/pushapk-v1': ('push-apk', None),
    "scriptworker-prov-v1/signing-linux-v1": ('scriptworker-signing', None),
    "scriptworker-prov-v1/shipit": ('shipit', None),
    "scriptworker-prov-v1/shipit-dev": ('shipit', None),
    "scriptworker-prov-v1/treescript-v1": ('treescript', None),
    'releng-hardware/gecko-t-osx-1010': ('generic-worker', 'macosx'),
}


def worker_type_implementation(worker_type):
    """Get the worker implementation and OS for the given workerType, where the
    OS represents the host system, not the target OS, in the case of
    cross-compiles."""
    # assume that worker types for all levels are the same implementation
    worker_type = worker_type.replace('{level}', '1')
    return WORKER_TYPES[worker_type]
