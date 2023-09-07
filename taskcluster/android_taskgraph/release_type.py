# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def does_task_match_release_type(task, release_type):
    return (
        # TODO: only use a single attribute to compare to `release_type`
        task.attributes.get("build-type") == release_type
        or task.attributes.get("release-type") == release_type
    )
