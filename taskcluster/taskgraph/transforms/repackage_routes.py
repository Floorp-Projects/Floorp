# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add indexes to repackage kinds
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema
from taskgraph.transforms.job import job_description_schema

transforms = TransformSequence()


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job['label']
        validate_schema(
            job_description_schema, job,
            "In repackage-signing ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def add_indexes(config, jobs):
    for job in jobs:
        repackage_type = job['attributes'].get('repackage_type')
        if repackage_type:
            build_platform = job['attributes']['build_platform']
            job_name = '{}-{}'.format(build_platform, repackage_type)
            product = job.get('index', {}).get('product', 'firefox')
            index_type = 'generic'
            if job['attributes'].get('nightly') and job['attributes'].get('locale'):
                index_type = 'nightly-l10n'
            if job['attributes'].get('nightly'):
                index_type = 'nightly'
            if job['attributes'].get('locale'):
                index_type = 'l10n'
            job['index'] = {
                'job-name': job_name,
                'product': product,
                'type': index_type
            }

        yield job
