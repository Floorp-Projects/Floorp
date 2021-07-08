# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def build_name_and_attributes(config, tasks):
    for task in tasks:
        task["dependencies"] = {
            dep.kind: dep.label
            for dep in _get_all_deps(task)
        }
        primary_dep = task["primary-dependency"]
        copy_of_attributes = primary_dep.attributes.copy()
        task.setdefault("attributes", copy_of_attributes)
        # run_on_tasks_for is set as an attribute later in the pipeline
        task.setdefault("run-on-tasks-for", copy_of_attributes['run_on_tasks_for'])
        task["name"] = _get_dependent_job_name_without_its_kind(primary_dep)

        yield task


def _get_dependent_job_name_without_its_kind(dependent_job):
    return dependent_job.label[len(dependent_job.kind) + 1:]


def _get_all_deps(task):
    if task.get("dependent-tasks"):
        return task["dependent-tasks"].values()

    return [task["primary-dependency"]]


@transforms.add
def build_upstream_artifacts(config, tasks):
    for task in tasks:
        worker_definition = {
            "upstream-artifacts": [],
        }

        for dep in _get_all_deps(task):
            paths = sorted(dep.attributes["artifacts"].values())
            if paths:
                worker_definition["upstream-artifacts"].append({
                    "taskId": {"task-reference": f"<{dep.kind}>"},
                    "taskType": dep.kind,
                    "paths": paths,
                })

        task["worker"].update(worker_definition)
        yield task
