# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partners import (get_partner_config_by_kind, check_if_partners_enabled)
from taskgraph.util.signed_artifacts import generate_specifications_of_artifacts_to_sign

transforms = TransformSequence()

transforms.add(check_if_partners_enabled)


@transforms.add
def define_upstream_artifacts(config, jobs):
    partner_configs = get_partner_config_by_kind(config, config.kind)
    if not partner_configs:
        return

    for job in jobs:
        dep_job = job['primary-dependency']
        job['attributes'] = copy_attributes_from_dependent_job(dep_job)

        repack_ids = job['extra']['repack_ids']
        artifacts_specifications = generate_specifications_of_artifacts_to_sign(
            config,
            job,
            keep_locale_template=True,
            kind=config.kind,
        )
        job['upstream-artifacts'] = [{
            'taskId': {'task-reference': '<{}>'.format(job['depname'])},
            'taskType': 'build',
            'paths': [
                path_template.format(locale=repack_id)
                for path_template in spec['artifacts']
                for repack_id in repack_ids
            ],
            'formats': spec['formats'],
        } for spec in artifacts_specifications]

        yield job
