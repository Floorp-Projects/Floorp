# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Utilities for generating a decision task from :file:`.taskcluster.yml`.
"""


import jsone
import yaml
import os
import slugid

from .vcs import find_hg_revision_push_info
from .templates import merge
from .time import current_json_time


def make_decision_task(params, root, context, head_rev=None):
    """Generate a basic decision task, based on the root .taskcluster.yml"""
    with open(os.path.join(root, ".taskcluster.yml"), "rb") as f:
        taskcluster_yml = yaml.safe_load(f)

    if not head_rev:
        head_rev = params["head_rev"]

    if params["repository_type"] == "hg":
        pushlog = find_hg_revision_push_info(params["repository_url"], head_rev)

        hg_push_context = {
            "pushlog_id": pushlog["pushid"],
            "pushdate": pushlog["pushdate"],
            "owner": pushlog["user"],
        }
    else:
        hg_push_context = {}

    slugids = {}

    def as_slugid(name):
        # https://github.com/taskcluster/json-e/issues/164
        name = name[0]
        if name not in slugids:
            slugids[name] = slugid.nice()
        return slugids[name]

    # provide a similar JSON-e context to what mozilla-taskcluster provides:
    # https://docs.taskcluster.net/reference/integrations/mozilla-taskcluster/docs/taskcluster-yml
    # but with a different tasks_for and an extra `cron` section
    context = merge(
        {
            "repository": {
                "url": params["repository_url"],
                "project": params["project"],
                "level": params["level"],
            },
            "push": merge(
                {
                    "revision": params["head_rev"],
                    # remainder are fake values, but the decision task expects them anyway
                    "comment": " ",
                },
                hg_push_context,
            ),
            "now": current_json_time(),
            "as_slugid": as_slugid,
        },
        context,
    )

    rendered = jsone.render(taskcluster_yml, context)
    if len(rendered["tasks"]) != 1:
        raise Exception("Expected .taskcluster.yml to only produce one cron task")
    task = rendered["tasks"][0]

    task_id = task.pop("taskId")
    return (task_id, task)
