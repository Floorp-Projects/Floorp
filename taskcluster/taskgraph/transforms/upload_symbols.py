# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-symbols task description template,
  taskcluster/ci/upload-symbols/job-template.yml
into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        # Fill out the dynamic fields in the task description
        task['label'] = task['build-label'] + '-upload-symbols'
        task['dependencies'] = {'build': task['build-label']}
        task['worker']['env']['GECKO_HEAD_REPOSITORY'] = config.params['head_repository']
        task['worker']['env']['GECKO_HEAD_REV'] = config.params['head_rev']

        build_platform, build_type = task['build-platform'].split('/')
        attributes = task.setdefault('attributes', {})
        attributes['build_platform'] = build_platform
        attributes['build_type'] = build_type

        # clear out the stuff that's not part of a task description
        del task['build-label']
        del task['build-platform']

        yield task
