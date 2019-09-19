# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol


transforms = TransformSequence()


_CHANNEL_PER_TASK_NAME = {
    'nightly': 'nightly',
    'nightly-legacy': 'production',
    'production': 'production',
}

_GOOGLE_PLAY_TRACK_PER_TASK_NAME = {
    'nightly': 'internal',
    'nightly-legacy': 'nightly',
}


@transforms.add
def build_name_and_attributes(config, tasks):
    for task in tasks:
        dep = task["primary-dependency"]
        task["dependencies"] = {"signing": dep.label}
        task.setdefault("attributes", {}).update(dep.attributes.copy())
        task["name"] = _get_dependent_job_name_without_its_kind(dep)

        yield task


def _get_dependent_job_name_without_its_kind(dependent_job):
    return dependent_job.label[len(dependent_job.kind) + 1:]


@transforms.add
def build_treeherder_definition(config, tasks):
    for task in tasks:
        dep = task["primary-dependency"]
        task["treeherder"] = inherit_treeherder_from_dep(task, dep)
        treeherder_group = dep.task["extra"]["treeherder"]["groupSymbol"]
        treeherder_symbol = join_symbol(treeherder_group, 'gp')
        task["treeherder"]["symbol"] = treeherder_symbol

        yield task


@transforms.add
def build_worker_definition(config, tasks):
    for task in tasks:
        dep = task.pop("primary-dependency")
        task_name = task["name"]

        worker_definition = {}
        worker_definition["upstream-artifacts"] = [{
            "taskId": {"task-reference": "<signing>"},
            "taskType": "signing",
            "paths": dep.attributes["apks"].values(),
        }]
        worker_definition["dep"] = config.params["level"] != "3"
        worker_definition["channel"] = _CHANNEL_PER_TASK_NAME[task_name]

        # Fenix production doesn't follow the rule {product}-{channel}
        worker_definition["certificate-alias"] = 'fenix' if task_name == 'production' else \
            "{}-{}".format(task["worker"]["product"], worker_definition["channel"])

        if _GOOGLE_PLAY_TRACK_PER_TASK_NAME.get(task_name):
            worker_definition["google-play-track"] = _GOOGLE_PLAY_TRACK_PER_TASK_NAME[task_name]

        task["worker"].update(worker_definition)
        yield task
