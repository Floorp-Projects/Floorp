# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from gecko_taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


def is_fission_enabled(task):
    options = task["mozharness"]["extra-options"]
    if "--setpref=fission.autostart=true" in options or "--enable-fission" in options:
        return True
    return False


def is_fission_disabled(task):
    options = task["mozharness"]["extra-options"]
    if "--setpref=fission.autostart=false" in options or "--disable-fission" in options:
        return True
    return False


@transforms.add
def reset_fission_prefs(config, tasks):
    """Verify and simplify fission --setpref use."""
    for task in tasks:
        variant = task["attributes"].get("unittest_variant", "")
        is_fission_variant = "fission" in variant and "no-fission" not in variant
        if task["suite"] not in ["gtest", "cppunittest", "jittest"]:
            if is_fission_variant:
                if is_fission_disabled(task):
                    raise Exception("Fission task disabled fission: " + str(task))
                if not is_fission_enabled(task):
                    # Ensure fission enabled for fission tasks
                    task["mozharness"]["extra-options"].append(
                        "--setpref=fission.autostart=true"
                    )
            else:
                if is_fission_enabled(task):
                    raise Exception("Non-fission task enabled fission: " + str(task))
                if not is_fission_disabled(task):
                    # Ensure fission disabled for non-fission tasks
                    task["mozharness"]["extra-options"].append(
                        "--setpref=fission.autostart=false"
                    )

        yield task
