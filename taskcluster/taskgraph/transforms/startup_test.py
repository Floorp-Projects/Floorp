# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_command(config, jobs):
    for job in jobs:
        extra_config = job.pop("extra-config")
        upstream_kind = extra_config["upstream_kind"]
        upstream_artifact = extra_config["upstream_artifact"]
        binary = extra_config["binary"]
        package_to_test = "<{}/public/build/{}>".format(
            upstream_kind, upstream_artifact
        )

        if job["attributes"]["build_platform"].startswith("linux"):
            job["run"]["command"] = {
                "artifact-reference": ". $HOME/scripts/xvfb.sh && start_xvfb '1600x1200x24' 0 && "
                + "python3 ./mach python testing/mozharness/scripts/does_it_crash.py "
                + "--run-for 30 --thing-url "
                + package_to_test
                + " --thing-to-run "
                + binary
            }
        else:
            job["run"]["mach"] = {
                "artifact-reference": "python testing/mozharness/scripts/does_it_crash.py "
                + "--run-for 30 --thing-url "
                + package_to_test
                + " --thing-to-run "
                + binary
            }

        yield job
