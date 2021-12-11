# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from collections import deque
import taskgraph
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.cached_tasks import add_optimization

transforms = TransformSequence()


def order_tasks(config, tasks):
    """Iterate image tasks in an order where parent tasks come first."""
    if config.kind == "docker-image":
        kind_prefix = "build-docker-image-"
    else:
        kind_prefix = config.kind + "-"

    pending = deque(tasks)
    task_labels = {task["label"] for task in pending}
    emitted = set()
    while True:
        try:
            task = pending.popleft()
        except IndexError:
            break
        parents = {
            task
            for task in task.get("dependencies", {}).values()
            if task.startswith(kind_prefix)
        }
        if parents and not emitted.issuperset(parents & task_labels):
            pending.append(task)
            continue
        emitted.add(task["label"])
        yield task


def format_task_digest(cached_task):
    return "/".join(
        [
            cached_task["type"],
            cached_task["name"],
            cached_task["digest"],
        ]
    )


@transforms.add
def cache_task(config, tasks):
    if taskgraph.fast:
        for task in tasks:
            yield task
        return

    digests = {}
    for task in config.kind_dependencies_tasks:
        if "cached_task" in task.attributes:
            digests[task.label] = format_task_digest(task.attributes["cached_task"])

    for task in order_tasks(config, tasks):
        cache = task.pop("cache", None)
        if cache is None:
            yield task
            continue

        dependency_digests = []
        for p in task.get("dependencies", {}).values():
            if p in digests:
                dependency_digests.append(digests[p])
            else:
                raise Exception(
                    "Cached task {} has uncached parent task: {}".format(
                        task["label"], p
                    )
                )
        digest_data = cache["digest-data"] + sorted(dependency_digests)
        add_optimization(
            config,
            task,
            cache_type=cache["type"],
            cache_name=cache["name"],
            digest_data=digest_data,
        )
        digests[task["label"]] = format_task_digest(task["attributes"]["cached_task"])

        yield task
