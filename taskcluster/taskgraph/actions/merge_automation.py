# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action

from taskgraph.decision import taskgraph_decision
from taskgraph.parameters import Parameters
from taskgraph.util.attributes import RELEASE_PROMOTION_PROJECTS


def is_release_promotion_available(parameters):
    return parameters["project"] in RELEASE_PROMOTION_PROJECTS


@register_callback_action(
    name="merge-automation",
    title="Merge Day Automation",
    symbol="${input.behavior}",
    description="Merge repository branches.",
    generic=False,
    order=500,
    context=[],
    available=is_release_promotion_available,
    schema=lambda graph_config: {
        "type": "object",
        "properties": {
            "force-dry-run": {
                "type": "boolean",
                "description": "Override other options and do not push changes",
                "default": True,
            },
            "push": {
                "type": "boolean",
                "description": "Push changes using to_repo and to_branch",
                "default": False,
            },
            "behavior": {
                "type": "string",
                "description": "The type of release promotion to perform.",
                "enum": sorted(graph_config["merge-automation"]["behaviors"].keys()),
            },
            "from-repo": {
                "type": "string",
                "description": "The URI of the source repository",
            },
            "to-repo": {
                "type": "string",
                "description": "The push URI of the target repository",
            },
            "from-branch": {
                "type": "string",
                "description": "The fx head of the source, such as central",
            },
            "to-branch": {
                "type": "string",
                "description": "The fx head of the target, such as beta",
            },
            "ssh-user-alias": {
                "type": "string",
                "description": "The alias of an ssh account to use when pushing changes.",
            },
        },
        "required": [
            "behavior"
        ],
    },
)
def merge_automation_action(parameters, graph_config, input, task_group_id, task_id):

    # make parameters read-write
    parameters = dict(parameters)

    parameters["target_tasks_method"] = "merge_automation"
    parameters["merge_config"] = {
        "force-dry-run": input.get("force-dry-run", False),
        "behavior": input["behavior"],
    }

    for field in ["from-repo", "from-branch", "to-repo", "to-branch", "ssh-user-alias", "push"]:
        if input.get(field):
            parameters["merge_config"][field] = input[field]

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({"root": graph_config.root_dir}, parameters=parameters)
