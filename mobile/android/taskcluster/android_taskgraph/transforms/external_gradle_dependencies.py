# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence

from ..build_config import get_path, get_upstream_deps_for_all_gradle_projects

transforms = TransformSequence()


@transforms.add
def set_treeherder_config(config, tasks):
    for task in tasks:
        gradle_project = _get_gradle_project(task)
        treeherder = task.setdefault("treeherder", {})
        treeherder_group = task.get("attributes", {}).get("treeherder-group", gradle_project)
        treeherder["symbol"] = f"{treeherder_group}(egd)"

        yield task


def _get_gradle_project(task):
    gradle_project = task.get("attributes", {}).get("component")
    if not gradle_project:
        gradle_project = "focus" # TODO: Support Fenix
    return gradle_project


@transforms.add
def extend_resources(config, tasks):
    deps_per_component = get_upstream_deps_for_all_gradle_projects()

    for task in tasks:
        run = task.setdefault("run", {})
        resources = run.setdefault("resources", [])

        component = task.get("attributes", {}).get("component")
        if not component:
            component = "app" # == Focus. TODO: Support Fenix

        dependencies = deps_per_component[component]
        component_and_deps = [component] + dependencies

        resources.extend([
            path
            for gradle_project in component_and_deps
            for path in _get_build_gradle_paths(gradle_project)
        ])
        run["resources"] = sorted(list(set(resources)))

        yield task


def _get_build_gradle_paths(gradle_project):
    build_gradle_paths = [
        "android-components/plugins/publicsuffixlist/build.gradle"
    ]

    if gradle_project in ("app", "service-telemetry"):
        build_gradle_paths.extend([
            "focus-android/build.gradle",
            "focus-android/buildSrc/build.gradle",
        ])

        if gradle_project == "app":
            build_gradle_paths.append("focus-android/app/build.gradle")

        if gradle_project == "service-telemetry":
            build_gradle_paths.append("focus-android/service-telemetry/build.gradle")
    else:
        build_gradle_paths.extend([
            "android-components/build.gradle",
            "android-components/buildSrc/build.gradle",
            "android-components/plugins/dependencies/build.gradle",
            f"android-components/{get_path(gradle_project)}/build.gradle",
        ])

    return build_gradle_paths


@transforms.add
def set_command_arguments(config, tasks):
    for task in tasks:
        gradle_project = task.get("attributes", {}).get("component")
        work_dir = "android-components"
        if not gradle_project:
            gradle_project = "app" # == Focus. TODO: Support Fenix
            work_dir = "focus-android"

        run = task.setdefault("run", {})
        arguments = run.setdefault("arguments", [])
        if not arguments:
            arguments.append(work_dir)

            if work_dir == "android-components":
                arguments.append("-Pcoverage")
            arguments.extend([
                f"{gradle_project}:{gradle_task_name}"
                for gradle_task_name in _get_gradle_task_names(gradle_project)
            ])

        yield task


def _get_gradle_task_names(gradle_project):
    gradle_tasks_name = []
    # TODO Support Fenix
    if gradle_project == "app":
        gradle_tasks_name.extend(["assembleFocusDebug", "assembleAndroidTest", "testFocusDebugUnitTest", "lint"])
    else:
        lint_task_name = "lint" if gradle_project in ("tooling-lint", "samples-browser") else "lintRelease"
        gradle_tasks_name.extend(["assemble", "assembleAndroidTest", "test", lint_task_name])
    return tuple(gradle_tasks_name)
