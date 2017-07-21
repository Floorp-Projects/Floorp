# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_signed_routes(config, jobs):
    """Add routes corresponding to the routes of the build task
       this corresponds to, with .signed inserted, for all gecko.v2 routes"""

    for job in jobs:
        dep_job = job['dependent-task']

        job['routes'] = []
        if dep_job.attributes.get('nightly'):
            for dep_route in dep_job.task.get('routes', []):
                if not dep_route.startswith('index.gecko.v2'):
                    continue
                branch = dep_route.split(".")[3]
                rest = ".".join(dep_route.split(".")[4:])
                job['routes'].append(
                    'index.gecko.v2.{}.signed-nightly.{}'.format(branch, rest))

        yield job


@transforms.add
def make_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        job['upstream-artifacts'] = _generate_upstream_artifacts(
            dep_job.attributes.get('build_platform'),
            dep_job.attributes.get('nightly')
        )

        label = dep_job.label.replace("build-", "signing-")
        job['label'] = label

        # Announce job status on funsize specific routes, so that it can
        # start the partial generation for nightlies only.
        job['use-funsize-route'] = True

        yield job


def _generate_upstream_artifacts(build_platform, is_nightly=False):
    if 'android' in build_platform:
        artifacts_specificities = [{
            'artifacts': [
                'public/build/target.apk',
                'public/build/en-US/target.apk'
            ],
            'format': 'jar',
        }]
    # XXX: Mac and Windows don't sign mars because internal aren't signed at
    # this stage of the release
    elif 'macosx' in build_platform:
        artifacts_specificities = [{
            'artifacts': ['public/build/target.dmg'],
            'format': 'macapp',
        }]
    elif 'win64' in build_platform:
        artifacts_specificities = [{
            'artifacts': [
                'public/build/target.zip',
                'public/build/setup.exe'
            ],
            'format': 'sha2signcode',
        }]
    elif 'win32' in build_platform:
        artifacts_specificities = [{
            'artifacts': [
                'public/build/target.zip',
                'public/build/setup.exe',
                ],
            'format': 'sha2signcode',
        }]
        if is_nightly:
            artifacts_specificities[0]['artifacts'] += ['public/build/setup-stub.exe']
    elif 'linux' in build_platform:
        artifacts_specificities = [{
            'artifacts': ['public/build/target.tar.bz2'],
            'format': 'gpg',
        }, {
            'artifacts': ['public/build/update/target.complete.mar'],
            'format': 'mar',
        }]
    else:
        raise Exception("Platform not implemented for signing")

    return [{
        'taskId': {'task-reference': '<build>'},
        'taskType': 'build',
        'paths': specificity['artifacts'],
        'formats': [specificity['format']],
    } for specificity in artifacts_specificities]
