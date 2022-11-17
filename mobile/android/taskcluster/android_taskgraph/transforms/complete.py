# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("notifications",):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    "geckoview-bump": _get_geckoview_bump(config, task),
                },
            )
        yield task


def _get_geckoview_bump(config, task):
    if all(
        (
            task["name"] == "pr",  # Filter out pr-1 and pr-2
            config.params["owner"] == "github-actions[bot]@users.noreply.github.com",
            config.params["head_repository"] == config.params["base_repository"],
            config.params["tasks_for"] == "github-pull-request-untrusted",
            config.params["head_ref"] == "relbot/upgrade-geckoview-ac-main",
        )
    ):
        # We have to return strings because of the way `by-*` works in taskgraph
        return "nightly"

    return ""


# Based on https://searchfox.org/mozilla-central/rev/e0eb861a187f0bb6d994228f2e0e49b2c9ee455e/taskcluster/taskgraph/transforms/release_notifications.py#31
@transforms.add
def add_notifications(config, tasks):
    for task in tasks:
        notifications = task.pop("notifications", None)
        if notifications:
            emails = notifications["emails"]
            subject = notifications["subject"].format(
                pull_request_number=config.params["pull_request_number"]
            )
            message = notifications["message"].format(
                repository=config.params["base_repository"],
                pull_request_number=config.params["pull_request_number"],
            )

            status_types = notifications.get("status-types", ["on-failed"])
            for s in status_types:
                task.setdefault("routes", []).extend(
                    [f"notify.email.{email}.{s}" for email in emails]
                )

            task.setdefault("extra", {}).update(
                {
                    "notify": {
                        "email": {
                            "subject": subject,
                        }
                    }
                }
            )
            if message:
                task["extra"]["notify"]["email"]["content"] = message

        yield task
