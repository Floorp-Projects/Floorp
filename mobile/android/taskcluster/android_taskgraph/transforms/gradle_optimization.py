from taskgraph.transforms.base import TransformSequence

from ..build_config import get_path, ANDROID_COMPONENTS_DIR
from ..gradle import get_upstream_deps_for_gradle_projects


transforms = TransformSequence()


@transforms.add
def extend_optimization_if_one_already_exists(config, tasks):
    deps_per_component = get_upstream_deps_for_gradle_projects(ANDROID_COMPONENTS_DIR)

    for task in tasks:
        optimization = task.get("optimization")
        if optimization:
            skip_unless_changed = optimization["skip-unless-changed"]

            component = task["attributes"]["component"]
            # TODO Remove this special case when ui-test.sh is able to accept "browser-engine-gecko"
            if component == "browser":
                component = "browser-engine-gecko"

            dependencies = deps_per_component[component]
            component_and_deps = [component] + dependencies

            skip_unless_changed.extend([
                f"android-components/{get_path(component)}/**"
                for component in component_and_deps
            ])

        yield task
