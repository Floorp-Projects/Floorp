# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_dependencies

from android_taskgraph.util.scriptworker import generate_beetmover_upstream_artifacts

transforms = TransformSequence()


def _get_task_type(dep_kind):
    if dep_kind.startswith("build-"):
        return "build"
    elif dep_kind.startswith("signing-"):
        return "signing"
    return dep_kind


@transforms.add
def build_upstream_artifacts(config, tasks):
    for task in tasks:
        worker_definition = {
            "upstream-artifacts": [],
        }
        if "artifact_map" in task["attributes"]:
            # Beetmover-apk tasks use declarative artifacts.
            locale = task["attributes"].get("locale")
            build_type = task["attributes"]["build-type"]
            worker_definition[
                "upstream-artifacts"
            ] = generate_beetmover_upstream_artifacts(config, task, build_type, locale)
        else:
            for dep in get_dependencies(config, task):
                paths = list(dep.attributes.get("artifacts", {}).values())
                paths.extend(
                    [
                        apk_metadata["name"]
                        for apk_metadata in dep.attributes.get("apks", {}).values()
                    ]
                )
                if dep.attributes.get("aab"):
                    paths.extend([dep.attributes.get("aab")])
                if paths:
                    worker_definition["upstream-artifacts"].append(
                        {
                            "taskId": {"task-reference": f"<{dep.kind}>"},
                            "taskType": _get_task_type(dep.kind),
                            "paths": sorted(paths),
                        }
                    )

        task.setdefault("worker", {}).update(worker_definition)
        yield task
