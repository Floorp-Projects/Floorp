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
def make_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        if 'android' in dep_job.attributes.get('build_platform'):
            job_specs = [
                {
                    'artifacts': ['public/build/target.apk',
                                  'public/build/en-US/target.apk'],
                    'format': 'jar',
                },
            ]
        else:
            job_specs = [
                {
                    'artifacts': ['public/build/target.tar.bz2',
                                  'public/build/target.checksums'],
                    'format': 'gpg',
                }, {
                    'artifacts': ['public/build/update/target.complete.mar'],
                    'format': 'mar',
                }
            ]
        upstream_artifacts = []
        for spec in job_specs:
            fmt = spec["format"]
            upstream_artifacts.append({
                "taskId": {"task-reference": "<build>"},
                "taskType": "build",
                "paths": spec["artifacts"],
                "formats": [fmt]
            })

        job['upstream-artifacts'] = upstream_artifacts

        label = dep_job.label.replace("build-", "signing-")
        job['label'] = label

        yield job
