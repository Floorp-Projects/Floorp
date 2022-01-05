# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.parameters import Parameters

from gecko_taskgraph.actions.registry import register_callback_action
from gecko_taskgraph.decision import taskgraph_decision


@register_callback_action(
    title="Push scriptworker canaries.",
    name="scriptworker-canary",
    symbol="scriptworker-canary",
    description="Trigger scriptworker-canary pushes for the given scriptworkers.",
    schema={
        "type": "object",
        "properties": {
            "scriptworkers": {
                "type": "array",
                "description": "List of scriptworker types to run canaries for.",
                "items": {"type": "string"},
            },
        },
    },
    order=1000,
    permission="scriptworker-canary",
    context=[],
)
def scriptworker_canary(parameters, graph_config, input, task_group_id, task_id):
    scriptworkers = input["scriptworkers"]

    # make parameters read-write
    parameters = dict(parameters)

    parameters["target_tasks_method"] = "scriptworker_canary"
    parameters["try_task_config"] = {
        "scriptworker-canary-workers": scriptworkers,
    }
    parameters["tasks_for"] = "action"

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({"root": graph_config.root_dir}, parameters=parameters)
