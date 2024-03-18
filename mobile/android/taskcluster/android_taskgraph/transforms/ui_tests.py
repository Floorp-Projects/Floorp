# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


_ANDROID_TASK_NAME_PREFIX = "android-"


@transforms.add
def set_component_attribute(config, tasks):
    for task in tasks:
        component_name = task.pop("component", None)
        if not component_name:
            task_name = task["name"]
            if task_name.startswith(_ANDROID_TASK_NAME_PREFIX):
                component_name = task_name[len(_ANDROID_TASK_NAME_PREFIX) :]
            else:
                raise NotImplementedError(
                    f"Cannot determine component name from task {task_name}"
                )

        attributes = task.setdefault("attributes", {})
        attributes["component"] = component_name

        yield task


@transforms.add
def define_ui_test_command_line(config, tasks):
    for task in tasks:
        run = task.setdefault("run", {})
        post_gradlew = run.setdefault("post-gradlew", [])
        post_gradlew.append(
            [
                "automation/taskcluster/androidTest/ui-test.sh",
                task["attributes"]["component"],
                "arm",
                "1",
            ]
        )

        yield task


@transforms.add
def define_treeherder_symbol(config, tasks):
    for task in tasks:
        treeherder = task.setdefault("treeherder")
        treeherder.setdefault("symbol", f"{task['attributes']['component']}(unit)")

        yield task


@transforms.add
def define_description(config, tasks):
    for task in tasks:
        task.setdefault(
            "description",
            f"Run unit/ui tests on device for {task['attributes']['component']}",
        )
        yield task
