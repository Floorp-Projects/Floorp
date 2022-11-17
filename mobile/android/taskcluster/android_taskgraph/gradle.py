import logging
import subprocess

from collections import defaultdict
from taskgraph.util.memoize import memoize

from .build_config import get_components

logger = logging.getLogger(__name__)

CONFIGURATIONS_WITH_DEPENDENCIES = (
    "api",
    "compileOnly",
    "implementation",
    "testImplementation"
)


@memoize
def get_upstream_deps_for_gradle_projects(gradle_root):
    """Return the full list of local upstream dependencies of a component."""
    project_dependencies = defaultdict(set)
    gradle_projects = _get_gradle_projects(gradle_root)

    for configuration in CONFIGURATIONS_WITH_DEPENDENCIES:
        logger.info(f"Looking for dependencies in {configuration} configuration in {gradle_root}")

        cmd = ["./gradlew", "--console=plain", "--parallel"]
        # This is eventually going to fail if there's ever enough projects to make the command line
        # too long. If that happens, we'll need to split this list up and run gradle more than once.
        for gradle_project in sorted(gradle_projects):
            cmd.extend([f"{gradle_project}:dependencies", "--configuration", configuration])

        # Parsing output like this is not ideal but bhearsum couldn't find a way
        # to get the dependencies printed in a better format. If we could convince
        # gradle to spit out JSON that would be much better.
        # This is filed as https://bugzilla.mozilla.org/show_bug.cgi?id=1795152
        current_project_name = None
        for line in subprocess.check_output(cmd, universal_newlines=True, cwd=gradle_root).splitlines():
            # If we find the start of a new component section, update our tracking variable
            if line.startswith("Project"):
                current_project_name = line.split(":")[1].strip("'")

            # If we find a new local dependency, add it.
            if line.startswith("+--- project") or line.startswith(r"\--- project"):
                project_dependencies[current_project_name].add(line.split(" ")[2])

    return {
        project_name: sorted(project_dependencies[project_name]) for project_name in gradle_projects
    }


def _get_gradle_projects(gradle_root):
    if gradle_root.endswith("android-components"):
        return [c["name"] for c in get_components()]
    # TODO: Support focus and fenix
    raise NotImplementedError(f"Cannot find gradle projects for {gradle_root}")
