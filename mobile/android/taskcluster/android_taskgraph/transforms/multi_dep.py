# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol
from android_taskgraph.util.scriptworker import generate_beetmover_upstream_artifacts

transforms = TransformSequence()


@transforms.add
def build_name_and_attributes(config, tasks):
    for task in tasks:
        task["dependencies"] = {
            dep.kind: dep.label
            for dep in _get_all_deps(task)
        }
        primary_dep = task["primary-dependency"]
        attributes = primary_dep.attributes.copy()
        attributes.update(task.get("attributes", {}))
        task["attributes"] = attributes
        # run_on_tasks_for is set as an attribute later in the pipeline
        task.setdefault("run-on-tasks-for", attributes['run_on_tasks_for'])
        task["name"] = _get_dependent_job_name_without_its_kind(primary_dep)

        yield task


def _get_dependent_job_name_without_its_kind(dependent_job):
    return dependent_job.label[len(dependent_job.kind) + 1:]


def _get_all_deps(task):
    if task.get("dependent-tasks"):
        return task["dependent-tasks"].values()

    return [task["primary-dependency"]]


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        if not task["attributes"].get("build-type"):
            task["attributes"]["build-type"] = task["name"]

        resolve_keyed_by(
            task,
            "treeherder.job-symbol",
            item_name=task["name"],
            **{
                'build-type': task["attributes"]["build-type"],
                'level': config.params["level"],
            }
        )
        yield task


@transforms.add
def build_upstream_artifacts(config, tasks):
    for task in tasks:
        worker_definition = {
            "upstream-artifacts": [],
        }
        if "artifact_map" in task["attributes"]:
            # Beetmover-fenix tasks use declarative artifacts.
            locale = task["attributes"].get("locale")
            build_type = task["attributes"]["build-type"]
            worker_definition[
                "upstream-artifacts"
            ] = generate_beetmover_upstream_artifacts(config, task, build_type, locale)
        else:
            for dep in _get_all_deps(task):
                paths = list(dep.attributes.get("artifacts", {}).values())
                paths.extend([
                    apk_metadata["name"]
                    for apk_metadata in dep.attributes.get("apks", {}).values()
                ])
                if paths:
                    worker_definition["upstream-artifacts"].append({
                        "taskId": {"task-reference": f"<{dep.kind}>"},
                        "taskType": _get_task_type(dep.kind),
                        "paths": sorted(paths),
                    })

        task.setdefault("worker", {}).update(worker_definition)
        yield task


def _get_task_type(dep_kind):
    if dep_kind.startswith("build-"):
        return "build"
    elif dep_kind.startswith("signing-"):
        return "signing"
    return dep_kind
