# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Transforms for release tasks
"""


def run_on_releases(config, jobs):
    """
    Filter out jobs with `run-on-releases` set, and that don't match the
    `release_type` paramater.
    """
    for job in jobs:
        release_type = config.params["release_type"]
        run_on_release_types = job.pop("run-on-releases", None)

        if run_on_release_types is None or release_type in run_on_release_types:
            yield job
