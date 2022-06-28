# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add notifications via taskcluster-notify for release tasks
"""
from pipes import quote as shell_quote

from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def add_notifications(config, jobs):
    for job in jobs:
        label = "{}-{}".format(config.kind, job["name"])

        resolve_keyed_by(job, "emails", label, project=config.params["project"])
        emails = [email.format(config=config.__dict__) for email in job.pop("emails")]

        command = [
            "release",
            "send-buglist-email",
            "--version",
            config.params["version"],
            "--product",
            job["shipping-product"],
            "--revision",
            config.params["head_rev"],
            "--build-number",
            str(config.params["build_number"]),
            "--repo",
            config.params["head_repository"],
        ]
        for address in emails:
            command += ["--address", address]
        command += [
            # We wrap this in `{'task-reference': ...}` below
            "--task-group-id",
            "<decision>",
        ]

        job["scopes"] = [f"notify:email:{address}" for address in emails]
        job["run"] = {
            "using": "mach",
            "sparse-profile": "mach",
            "mach": {"task-reference": " ".join(map(shell_quote, command))},
        }

        yield job
