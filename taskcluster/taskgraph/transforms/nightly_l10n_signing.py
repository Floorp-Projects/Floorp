# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import join_symbol

transforms = TransformSequence()


@transforms.add
def make_signing_description(config, jobs):
    for job in jobs:
        job['depname'] = 'unsigned-repack'

        dep_job = job['dependent-task']
        dep_platform = dep_job.attributes.get('build_platform')

        job['upstream-artifacts'] = []
        if 'android' in dep_platform:
            job_specs = [
                {
                    'artifacts': ['public/build/{locale}/target.apk'],
                    'format': 'jar',
                },
            ]
        elif 'macosx' in dep_platform:
            job_specs = [
                 {
                    'artifacts': ['public/build/{locale}/target.dmg'],
                    'format': 'macapp',
                 }, {
                    'artifacts': ['public/build/{locale}/target.complete.mar'],
                    'format': 'mar',
                 }
            ]
        else:
            job_specs = [
                {
                    'artifacts': ['public/build/{locale}/target.tar.bz2'],
                    'format': 'gpg',
                }, {
                    'artifacts': ['public/build/{locale}/target.complete.mar'],
                    'format': 'mar',
                }
            ]
        upstream_artifacts = []
        for spec in job_specs:
            fmt = spec['format']
            upstream_artifacts.append({
                "taskId": {"task-reference": "<unsigned-repack>"},
                "taskType": "l10n",
                # Set paths based on artifacts in the specs (above) one per
                # locale present in the chunk this is signing stuff for.
                "paths": [f.format(locale=l)
                          for l in dep_job.attributes.get('chunk_locales', [])
                          for f in spec['artifacts']],
                "formats": [fmt]
            })

        job['upstream-artifacts'] = upstream_artifacts

        label = dep_job.label.replace("nightly-l10n-", "signing-l10n-")
        job['label'] = label

        # add the chunk number to the TH symbol
        symbol = 'Ns{}'.format(dep_job.attributes.get('l10n_chunk'))
        group = 'tc-L10n'

        job['treeherder'] = {
            'symbol': join_symbol(group, symbol),
        }

        # Announce job status on funsize specific routes, so that it can
        # start the partial generation for nightlies only.
        job['use-funsize-route'] = True

        yield job
