# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import time

from taskgraph.transforms.task import index_builder

# Please ping the performance testing team (whawkins at the moment) if these routes change.
# In the future, notifying consumers may be easier (https://bugzilla.mozilla.org/show_bug.cgi?id=1548810), but
# we need to remember to tell users for the time being
SIGNING_ROUTE_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.{variant}.latest.{abi}",
    "index.{trust-domain}.v2.{project}.{variant}.{build_date}.revision.{head_rev}.{abi}",
    "index.{trust-domain}.v2.{project}.{variant}.{build_date}.latest.{abi}",
    "index.{trust-domain}.v2.{project}.{variant}.revision.{head_rev}.{abi}",
]


@index_builder("signing")
def add_signing_indexes(config, task):
    if config.params["level"] != "3":
        return task

    subs = config.params.copy()
    subs["build_date"] = time.strftime(
        "%Y.%m.%d", time.gmtime(config.params["build_date"])
    )
    subs["trust-domain"] = config.graph_config["trust-domain"]
    subs["variant"] = task["attributes"]["build-type"]

    unique_routes = set()
    for tpl in SIGNING_ROUTE_TEMPLATES:
        for abi in task["attributes"]["apks"].keys():
            subs["abi"] = abi
            unique_routes.add(tpl.format(**subs))

    task.setdefault("routes", sorted(list(unique_routes)))
    return task
