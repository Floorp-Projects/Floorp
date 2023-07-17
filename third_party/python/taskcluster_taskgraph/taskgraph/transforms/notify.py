# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add notifications to tasks via Taskcluster's notify service.

See https://docs.taskcluster.net/docs/reference/core/notify/usage for
more information.
"""
from voluptuous import ALLOW_EXTRA, Any, Exclusive, Optional, Required

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by

_status_type = Any(
    "on-completed",
    "on-defined",
    "on-exception",
    "on-failed",
    "on-pending",
    "on-resolved",
    "on-running",
)

_recipients = [
    {
        Required("type"): "email",
        Required("address"): optionally_keyed_by("project", "level", str),
        Optional("status-type"): _status_type,
    },
    {
        Required("type"): "matrix-room",
        Required("room-id"): str,
        Optional("status-type"): _status_type,
    },
    {
        Required("type"): "pulse",
        Required("routing-key"): str,
        Optional("status-type"): _status_type,
    },
    {
        Required("type"): "slack-channel",
        Required("channel-id"): str,
        Optional("status-type"): _status_type,
    },
]

_route_keys = {
    "email": "address",
    "matrix-room": "room-id",
    "pulse": "routing-key",
    "slack-channel": "channel-id",
}
"""Map each type to its primary key that will be used in the route."""

NOTIFY_SCHEMA = Schema(
    {
        Exclusive("notify", "config"): {
            Required("recipients"): [Any(*_recipients)],
            Optional("content"): {
                Optional("email"): {
                    Optional("subject"): str,
                    Optional("content"): str,
                    Optional("link"): {
                        Required("text"): str,
                        Required("href"): str,
                    },
                },
                Optional("matrix"): {
                    Optional("body"): str,
                    Optional("formatted-body"): str,
                    Optional("format"): str,
                    Optional("msg-type"): str,
                },
                Optional("slack"): {
                    Optional("text"): str,
                    Optional("blocks"): list,
                    Optional("attachments"): list,
                },
            },
        },
        # Continue supporting the legacy schema for backwards compat.
        Exclusive("notifications", "config"): {
            Required("emails"): optionally_keyed_by("project", "level", [str]),
            Required("subject"): str,
            Optional("message"): str,
            Optional("status-types"): [_status_type],
        },
    },
    extra=ALLOW_EXTRA,
)
"""Notify schema."""

transforms = TransformSequence()
transforms.add_validate(NOTIFY_SCHEMA)


def _convert_legacy(config, legacy, label):
    """Convert the legacy format to the new one."""
    notify = {
        "recipients": [],
        "content": {"email": {"subject": legacy["subject"]}},
    }
    resolve_keyed_by(
        legacy,
        "emails",
        label,
        **{
            "level": config.params["level"],
            "project": config.params["project"],
        },
    )

    status_types = legacy.get("status-types", ["on-completed"])
    for email in legacy["emails"]:
        for status_type in status_types:
            notify["recipients"].append(
                {"type": "email", "address": email, "status-type": status_type}
            )

    notify["content"]["email"]["content"] = legacy.get("message", legacy["subject"])
    return notify


def _convert_content(content):
    """Convert the notify content to Taskcluster's format.

    The Taskcluster notification format is described here:
    https://docs.taskcluster.net/docs/reference/core/notify/usage
    """
    tc = {}
    if "email" in content:
        tc["email"] = content.pop("email")

    for key, obj in content.items():
        for name in obj.keys():
            tc_name = "".join(part.capitalize() for part in name.split("-"))
            tc[f"{key}{tc_name}"] = obj[name]
    return tc


@transforms.add
def add_notifications(config, tasks):
    for task in tasks:
        label = "{}-{}".format(config.kind, task["name"])
        if "notifications" in task:
            notify = _convert_legacy(config, task.pop("notifications"), label)
        else:
            notify = task.pop("notify", None)

        if not notify:
            yield task
            continue

        format_kwargs = dict(
            task=task,
            config=config.__dict__,
        )

        def substitute(ctx):
            """Recursively find all strings in a simple nested dict (no lists),
            and format them in-place using `format_kwargs`."""
            for key, val in ctx.items():
                if isinstance(val, str):
                    ctx[key] = val.format(**format_kwargs)
                elif isinstance(val, dict):
                    ctx[key] = substitute(val)
            return ctx

        task.setdefault("routes", [])
        for recipient in notify["recipients"]:
            type = recipient["type"]
            recipient.setdefault("status-type", "on-completed")
            substitute(recipient)

            if type == "email":
                resolve_keyed_by(
                    recipient,
                    "address",
                    label,
                    **{
                        "level": config.params["level"],
                        "project": config.params["project"],
                    },
                )

            task["routes"].append(
                f"notify.{type}.{recipient[_route_keys[type]]}.{recipient['status-type']}"
            )

        if "content" in notify:
            task.setdefault("extra", {}).update(
                {"notify": _convert_content(substitute(notify["content"]))}
            )
        yield task
