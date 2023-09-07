# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import time

from gecko_taskgraph.transforms.task import index_builder

SIGNING_ROUTE_TEMPLATES = [
    "index.{trust_domain}.v3.{project}.{artifact_type}.{variant}.latest.{artifact_name}",
    "index.{trust_domain}.v3.{project}.{artifact_type}.{variant}.{build_date}.revision.{head_rev}.{artifact_name}",
    "index.{trust_domain}.v3.{project}.{artifact_type}.{variant}.{build_date}.latest.{artifact_name}",
    "index.{trust_domain}.v3.{project}.{artifact_type}.{variant}.revision.{head_rev}.{artifact_name}",
]


@index_builder("signing")
def add_signing_indexes(config, task):
    routes = task.setdefault("routes", [])

    if config.params["level"] != "3":
        return task

    subs = config.params.copy()
    subs["build_date"] = time.strftime(
        "%Y.%m.%d", time.gmtime(config.params["build_date"])
    )
    subs["trust_domain"] = config.graph_config["trust-domain"]
    subs["variant"] = task["attributes"]["build-type"]

    if task["attributes"].get("component"):
        subs["artifact_type"] = "components"
        subs["artifact_name"] = task["attributes"]["component"]
        new_signing_routes = [
            template.format(**subs) for template in SIGNING_ROUTE_TEMPLATES
        ]

    elif task["attributes"].get("apks"):
        subs["artifact_type"] = "apks"
        new_signing_routes = []
        for abi in task["attributes"]["apks"].keys():
            subs["artifact_name"] = abi
            new_signing_routes.extend(
                [template.format(**subs) for template in SIGNING_ROUTE_TEMPLATES]
            )

    elif task["attributes"].get("aab"):
        # no indexes for AAB signing
        return task

    else:
        raise NotImplementedError(f"Unsupported artifact_type. Task: {task}")

    routes.extend(new_signing_routes)
    return task
