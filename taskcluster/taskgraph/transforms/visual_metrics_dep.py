# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transformations take a task description for a visual metrics task and
add the necessary environment variables to run on the given inputs.
"""

from __future__ import absolute_import, print_function, unicode_literals
from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()

SYMBOL = "%(groupSymbol)s(%(symbol)s-vismet)"
# the test- prefix makes the task SETA-optimized.
LABEL = "test-vismet-%(platform)s-%(raptor_try_name)s"


@transforms.add
def run_visual_metrics(config, jobs):
    for job in jobs:
        dep_job = job.pop('primary-dependency', None)
        if dep_job is not None:
            platform = dep_job.task['extra']['treeherder-platform']
            job['dependencies'] = {dep_job.label: dep_job.label}
            job['fetches'][dep_job.label] = ['/public/test_info/browsertime-results.tgz']
            attributes = dict(dep_job.attributes)
            attributes['platform'] = platform
            job['label'] = LABEL % attributes
            treeherder_info = dict(dep_job.task['extra']['treeherder'])
            job['treeherder']['symbol'] = SYMBOL % treeherder_info

            # Store the platform name so we can use it to calculate
            # the similarity metric against other tasks
            job['worker'].setdefault('env', {})['TC_PLATFORM'] = platform

            # vismet runs on Linux but we want to have it displayed
            # alongside the job it was triggered by to make it easier for
            # people to find it back.
            job['treeherder']['platform'] = platform

            # run-on-projects needs to be set based on the dependent task
            job['run-on-projects'] = attributes['run_on_projects']

            yield job
