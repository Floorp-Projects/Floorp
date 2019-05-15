# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
)
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

transforms = TransformSequence()

signing_description_schema = schema.extend({
    Required('depname', default='repackage'): basestring,
    Optional('label'): basestring,
    Optional('extra'): object,
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})

transforms.add_validate(signing_description_schema)


@transforms.add
def make_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes
        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = True  # cert_scope_per_platform uses this to choose the right cert

        description = (
            "Signing of OpenH264 Binaries for '"
            "{build_platform}/{build_type}'".format(
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        # we have a genuine repackage job as our parent
        dependencies = {"openh264": dep_job.label}

        my_attributes = copy_attributes_from_dependent_job(dep_job)

        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )

        scopes = [signing_cert_scope]

        if 'win' in build_platform:
            # job['primary-dependency'].task['payload']['command']
            scopes.append(add_scope_prefix(config, "signing:format:sha2signcode"))
            formats = ['sha2signcode']
        else:
            formats = ['autograph_gpg']

        rev = attributes['openh264_rev']
        upstream_artifacts = [{
            "taskId": {"task-reference": "<openh264>"},
            "taskType": "build",
            "paths": [
                "private/openh264/openh264-{}-{}.zip".format(build_platform, rev),
            ],
            "formats": formats
        }]

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault('symbol', _generate_treeherder_symbol(
            dep_job.task.get('extra', {}).get('treeherder', {}).get('symbol')
        ))

        task = {
            'label': job['label'],
            'description': description,
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': upstream_artifacts,
                       'max-run-time': 3600},
            'scopes': scopes,
            'dependencies': dependencies,
            'attributes': my_attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder
        }

        yield task


def _generate_treeherder_symbol(build_symbol):
    symbol = build_symbol + 's'
    return symbol
