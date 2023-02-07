# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from requests import HTTPError
from taskgraph.util.taskcluster import find_task_id, get_artifact

from gecko_taskgraph.util.attributes import INTEGRATION_PROJECTS, TRY_PROJECTS
from gecko_taskgraph.util.taskcluster import state_task

BACKSTOP_PUSH_INTERVAL = 20
BACKSTOP_TIME_INTERVAL = 60 * 4  # minutes
BACKSTOP_INDEX = "{trust-domain}.v2.{project}.latest.taskgraph.backstop"


def is_backstop(
    params,
    push_interval=BACKSTOP_PUSH_INTERVAL,
    time_interval=BACKSTOP_TIME_INTERVAL,
    trust_domain="gecko",
    integration_projects=INTEGRATION_PROJECTS,
):
    """Determines whether the given parameters represent a backstop push.

    Args:
        push_interval (int): Number of pushes
        time_interval (int): Minutes between forced schedules.
                             Use 0 to disable.
        trust_domain (str): "gecko" for Firefox, "comm" for Thunderbird
        integration_projects (set): project that uses backstop optimization
    Returns:
        bool: True if this is a backstop, otherwise False.
    """
    # In case this is being faked on try.
    if params.get("backstop", False):
        return True

    project = params["project"]
    pushid = int(params["pushlog_id"])
    pushdate = int(params["pushdate"])

    if project in TRY_PROJECTS:
        return False
    if project not in integration_projects:
        return True

    # On every Nth push, want to run all tasks.
    if pushid % push_interval == 0:
        return True

    if time_interval <= 0:
        return False

    # We also want to ensure we run all tasks at least once per N minutes.
    subs = {"trust-domain": trust_domain, "project": project}
    index = BACKSTOP_INDEX.format(**subs)

    try:
        last_backstop_id = find_task_id(index)
    except KeyError:
        # Index wasn't found, implying there hasn't been a backstop push yet.
        return True

    if state_task(last_backstop_id) in ("failed", "exception"):
        # If the last backstop failed its decision task, make this a backstop.
        return True

    try:
        last_pushdate = get_artifact(last_backstop_id, "public/parameters.yml")[
            "pushdate"
        ]
    except HTTPError as e:
        # If the last backstop decision task exists in the index, but
        # parameters.yml isn't available yet, it means the decision task is
        # still running. If that's the case, we can be pretty sure the time
        # component will not cause a backstop, so just return False.
        if e.response.status_code == 404:
            return False
        raise

    if (pushdate - last_pushdate) / 60 >= time_interval:
        return True
    return False
