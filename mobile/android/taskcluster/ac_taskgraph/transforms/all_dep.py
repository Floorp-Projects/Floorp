# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def build_name_and_attributes(config, tasks):
    for task in tasks:
        all_dependent_tasks = list(task.pop("dependent-tasks").values())
        task["dependencies"] = {
            dep.label: dep.label
            for dep in all_dependent_tasks
        }
        first_dep = all_dependent_tasks[0]
        copy_of_attributes = first_dep.attributes.copy()
        task.setdefault("attributes", copy_of_attributes)
        task["attributes"]["artifacts"] = {}
        task["attributes"]["component"] = "all"

        # run_on_tasks_for is set as an attribute later in the pipeline
        task.setdefault("run-on-tasks-for", copy_of_attributes['run_on_tasks_for'])
        task["name"] = task["attributes"][config.config['group-by']]

        yield task
