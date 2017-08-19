# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-generated-files task description template,
  taskcluster/ci/upload-generated-sources/kind.yml
into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.taskcluster import get_artifact_url


transforms = TransformSequence()


@transforms.add
def add_task_info(config, jobs):
    for job in jobs:
        dep_task = job['dependent-task']
        del job['dependent-task']

        # Add a dependency on the build task.
        job['dependencies'] = {'build': dep_task.label}
        # Label the job to match the build task it's uploading from.
        job['label'] = dep_task.label.replace("build-", "upload-generated-sources-")
        # Copy over some bits of metdata from the build task.
        dep_th = dep_task.task['extra']['treeherder']
        job.setdefault('attributes', {})
        job['attributes']['build_platform'] = dep_task.attributes.get('build_platform')
        plat = '{}/{}'.format(dep_th['machine']['platform'], dep_task.attributes.get('build_type'))
        job['treeherder']['platform'] = plat
        job['treeherder']['tier'] = dep_th['tier']
        # Add an environment variable pointing at the artifact from the build.
        artifact_url = get_artifact_url('<build>',
                                        'public/build/target.generated-files.tar.gz')
        job['worker'].setdefault('env', {})['ARTIFACT_URL'] = {
            'task-reference': artifact_url
        }

        yield job
