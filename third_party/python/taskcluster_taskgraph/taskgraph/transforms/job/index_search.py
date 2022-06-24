# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This transform allows including indexed tasks from other projects in the
current taskgraph.  The transform takes a list of indexes, and the optimization
phase will replace the task with the task from the other graph.
"""


from voluptuous import Required

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema

transforms = TransformSequence()

run_task_schema = Schema(
    {
        Required("using"): "index-search",
        Required(
            "index-search",
            "A list of indexes in decreasing order of priority at which to lookup for this "
            "task. This is interpolated with the graph parameters.",
        ): [str],
    }
)


@run_job_using("always-optimized", "index-search", schema=run_task_schema)
def fill_template(config, job, taskdesc):
    run = job["run"]
    taskdesc["optimization"] = {
        "index-search": [index.format(**config.params) for index in run["index-search"]]
    }
