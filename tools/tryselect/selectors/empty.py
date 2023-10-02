# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from ..cli import BaseTryParser
from ..push import generate_try_task_config, push_to_try


class EmptyParser(BaseTryParser):
    name = "empty"
    common_groups = ["push"]
    task_configs = [
        "artifact",
        "browsertime",
        "chemspill-prio",
        "disable-pgo",
        "env",
        "gecko-profile",
        "pernosco",
        "routes",
        "worker-overrides",
    ]


def run(
    message="{msg}",
    try_config=None,
    stage_changes=False,
    dry_run=False,
    closed_tree=False,
    push_to_lando=False,
):
    msg = 'No try selector specified, use "Add New Jobs" to select tasks.'
    return push_to_try(
        "empty",
        message.format(msg=msg),
        try_task_config=generate_try_task_config("empty", [], try_config),
        stage_changes=stage_changes,
        dry_run=dry_run,
        closed_tree=closed_tree,
        push_to_lando=push_to_lando,
    )
