# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.declarative_artifacts import get_geckoview_upstream_artifacts
from taskgraph.util.taskcluster import get_artifact_prefix


transforms = TransformSequence()


MOZ_ANDROID_FAT_AAR_ENV_MAP = {
    'android-api-16-nightly': 'MOZ_ANDROID_FAT_AAR_ARMEABI_V7A',
    'android-aarch64-nightly': 'MOZ_ANDROID_FAT_AAR_ARM64_V8A',
    'android-x86-nightly': 'MOZ_ANDROID_FAT_AAR_X86',
    'android-x86_64-nightly': 'MOZ_ANDROID_FAT_AAR_X86_64',

    'android-api-16-opt': 'MOZ_ANDROID_FAT_AAR_ARMEABI_V7A',
    'android-aarch64-opt': 'MOZ_ANDROID_FAT_AAR_ARM64_V8A',
    'android-x86-opt': 'MOZ_ANDROID_FAT_AAR_X86',
    'android-x86_64-opt': 'MOZ_ANDROID_FAT_AAR_X86_64',
}


@transforms.add
def set_fetches_and_locations(config, jobs):
    """Set defaults, including those that differ per worker implementation"""
    for job in jobs:
        dependencies = copy.deepcopy(job['dependencies'])

        for platform, label in dependencies.items():
            job['dependencies'] = {'build': label}

            aar_location = _get_aar_location(config, job, platform)
            prefix = get_artifact_prefix(job)
            if not prefix.endswith('/'):
                prefix = prefix + '/'
            if aar_location.startswith(prefix):
                aar_location = aar_location[len(prefix):]

            job.setdefault('fetches', {}).setdefault(platform, []).append({
                'artifact': aar_location,
                'extract': False,
            })

            aar_file_name = aar_location.split('/')[-1]
            env_var = MOZ_ANDROID_FAT_AAR_ENV_MAP[platform]
            job['worker']['env'][env_var] = aar_file_name

        job['dependencies'] = dependencies

        yield job


def _get_aar_location(config, job, platform):
    artifacts_locations = get_geckoview_upstream_artifacts(config, job, platform=platform)
    aar_locations = [
        path for path in artifacts_locations[0]['paths']
        if path.endswith('.aar')
    ]
    if len(aar_locations) != 1:
        raise ValueError('Only a single AAR must be given. Got: {}'.format(aar_locations))

    return aar_locations[0]
