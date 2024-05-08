# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

from gecko_taskgraph.util.platforms import platform_family

transforms = TransformSequence()


@transforms.add
def set_build_schedules_optimization(config, tasks):
    """Set the `build` optimization based on the build platform."""
    for task in tasks:
        # don't add an optimization if there's already one defined
        if "when" in task or "optimization" in task:
            yield task
            continue

        schedules = []
        if config.kind in (
            "build-components",
            "build-samples-browser",
            "test-components",
        ):
            # These are Android components builds and can only impact Fenix or Focus.
            schedules = ["android", "fenix", "focus-android"]

        elif config.kind in ("build-apk", "build-bundle", "test-apk", "ui-test-apk"):
            # These are APK builds for Fenix or Focus
            schedules = ["android"]

            if "fenix" in task["name"]:
                schedules.append("fenix")
            elif "focus" in task["name"] or "klar" in task["name"]:
                schedules.append("focus-android")

        else:
            family = platform_family(task["attributes"]["build_platform"])
            schedules = [family]

            if "android" not in family:
                # These are not GeckoView builds, so are associated with Firefox.
                schedules.append("firefox")

        task["optimization"] = {"build": schedules}
        yield task
