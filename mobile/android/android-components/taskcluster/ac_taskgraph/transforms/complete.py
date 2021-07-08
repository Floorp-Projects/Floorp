# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from copy import deepcopy
import re

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import add_suffix
from taskgraph import MAX_DEPENDENCIES


transforms = TransformSequence()


@transforms.add
def fill_dependencies(config, tasks):
    for task in tasks:
        dependencies = (f'<{dep}>' for dep in task['dependencies'].keys())
        task['run']['command']['task-reference'] = task['run']['command']['task-reference'].format(
            dependencies=' '.join(dependencies)
        )

        yield task


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("notifications",):
            resolve_keyed_by(
                task, key,
                item_name=task["name"],
                **{
                    "geckoview-bump": _get_geckoview_bump(config, task),
                }
            )
        yield task


_GECKOVIEW_NIGHTLY_BUMP_BRANCH_PATTERN = re.compile(r"^GV-nightly-(?P<date>\d{8})-(?P<time>\d{6})$")

def _get_geckoview_bump(config, task):
    if all((
        task["name"] == "pr",  # Filter out pr-1 and pr-2
        config.params["owner"] == "MickeyMoz@users.noreply.github.com",
        config.params["head_repository"] == "https://github.com/MickeyMoz/android-components",
        config.params["tasks_for"] == "github-pull-request",
        _GECKOVIEW_NIGHTLY_BUMP_BRANCH_PATTERN.search(config.params["head_ref"]),
    )):
        # We have to return strings because of the way `by-*` works in taskgraph
        return "nightly"

    return ""


def _extract_time_stamp(config):
    matches = _GECKOVIEW_NIGHTLY_BUMP_BRANCH_PATTERN.search(config.params["head_ref"])
    if matches:
        return "{}-{}".format(matches.group("date"), matches.group("time"))

    return ""


# Based on https://searchfox.org/mozilla-central/rev/e0eb861a187f0bb6d994228f2e0e49b2c9ee455e/taskcluster/taskgraph/transforms/release_notifications.py#31
@transforms.add
def add_notifications(config, jobs):
    geckoview_timestamp = _extract_time_stamp(config)

    for job in jobs:
        notifications = job.pop("notifications", None)
        if notifications:
            emails = notifications["emails"]
            subject = notifications["subject"].format(geckoview_timestamp=geckoview_timestamp)
            message = notifications["message"].format(pull_request_number=config.params["pull_request_number"])

            status_types = notifications.get('status-types', ['on-failed'])
            for s in status_types:
                job.setdefault('routes', []).extend(
                    [f'notify.email.{email}.{s}' for email in emails]
                )

            job.setdefault('extra', {}).update(
                {
                   'notify': {
                       'email': {
                            'subject': subject,
                        }
                    }
                }
            )
            if message:
                job['extra']['notify']['email']['content'] = message

        yield job
