from taskgraph.transforms.base import TransformSequence

from ..build_config import get_path
from ..gradle import get_upstream_deps_for_all_gradle_projects


transforms = TransformSequence()


@transforms.add
def extend_optimization_if_one_already_exists(config, tasks):
    deps_per_component = get_upstream_deps_for_all_gradle_projects()

    for task in tasks:
        optimization = task.get("optimization")
        if optimization:
            skip_unless_changed = optimization["skip-unless-changed"]

            component = task["attributes"].get("component")
            if not component:
                component = "app" # == Focus. TODO: Support Fenix
            # TODO Remove this special case when ui-test.sh is able to accept "browser-engine-gecko"
            if component == "browser":
                component = "browser-engine-gecko"

            dependencies = deps_per_component[component]
            component_and_deps = [component] + dependencies

            skip_unless_changed.extend(sorted([
                _get_path(component)
                for component in component_and_deps
            ]))

        yield task


def _get_path(component):
    if component == "app":
        return "focus-android/**"
    elif component == "service-telemetry":
        return "focus-android/service-telemetry/**"
    else:
        return f"android-components/{get_path(component)}/**"
