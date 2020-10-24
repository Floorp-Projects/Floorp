# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transformations take a task description for a visual metrics task and
add the necessary environment variables to run on the given inputs.
"""

from __future__ import absolute_import, print_function, unicode_literals

import json

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_visual_metrics_jobs(config, jobs):
    """Set the visual metrics configuration for the given jobs."""
    vismet_jobs = config.params['try_task_config'].get('visual-metrics-jobs')

    if vismet_jobs:
        vismet_jobs = json.dumps(vismet_jobs)

    for job in jobs:
        if vismet_jobs:
            job['task']['payload'].setdefault('env', {}).update(
                VISUAL_METRICS_JOBS_JSON=vismet_jobs)

        yield job
