# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform passes options from `mach perftest` to the corresponding task.
"""

from __future__ import absolute_import, print_function, unicode_literals

from datetime import date, timedelta
import json

import six

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def pass_perftest_options(config, jobs):
    for job in jobs:
        env = job.setdefault('worker', {}).setdefault('env', {})
        env['PERFTEST_OPTIONS'] = six.ensure_text(
            json.dumps(config.params["try_task_config"].get('perftest-options'))
        )
        yield job


@transforms.add
def setup_perftest_test_date(config, jobs):
    for job in jobs:
        if (job.get("attributes", {}).get("batch", False) and
            "--test-date" not in job["run"]["command"]):
            yesterday = (date.today() - timedelta(1)).strftime("%Y.%m.%d")
            job["run"]["command"] += " --test-date %s" % yesterday
        yield job
