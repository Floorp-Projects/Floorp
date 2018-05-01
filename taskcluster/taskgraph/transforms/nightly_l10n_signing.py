# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.signed_artifacts import generate_specifications_of_artifacts_to_sign
from taskgraph.util.treeherder import join_symbol

transforms = TransformSequence()


@transforms.add
def make_signing_description(config, jobs):
    for job in jobs:
        job['depname'] = 'unsigned-repack'

        dep_job = job['dependent-task']

        # add the chunk number to the TH symbol
        symbol = 'Ns{}'.format(dep_job.attributes.get('l10n_chunk'))
        group = 'L10n'

        job['treeherder'] = {
            'symbol': join_symbol(group, symbol),
        }

        yield job


@transforms.add
def define_upstream_artifacts(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        locale_specifications = generate_specifications_of_artifacts_to_sign(
            dep_job,
            keep_locale_template=True,
            project=config.params["project"],
        )

        upstream_artifacts = []
        for spec in locale_specifications:
            upstream_artifacts.append({
                'taskId': {'task-reference': '<unsigned-repack>'},
                'taskType': 'l10n',
                # Set paths based on artifacts in the specs (above) one per
                # locale present in the chunk this is signing stuff for.
                'paths': [
                    path_template.format(locale=locale)
                    for locale in dep_job.attributes.get('chunk_locales', [])
                    for path_template in spec['artifacts']
                ],
                'formats': spec['formats']
            })

        job['upstream-artifacts'] = upstream_artifacts

        yield job
