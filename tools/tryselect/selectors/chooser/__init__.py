# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing
import os
import time
import webbrowser
from threading import Timer

from gecko_taskgraph.target_tasks import filter_by_uncommon_try_tasks

from tryselect.cli import BaseTryParser
from tryselect.push import (
    check_working_directory,
    generate_try_task_config,
    push_to_try,
)
from tryselect.tasks import generate_tasks

here = os.path.abspath(os.path.dirname(__file__))


class ChooserParser(BaseTryParser):
    name = "chooser"
    arguments = []
    common_groups = ["push", "task"]
    task_configs = [
        "artifact",
        "browsertime",
        "chemspill-prio",
        "disable-pgo",
        "env",
        "gecko-profile",
        "path",
        "pernosco",
        "rebuild",
        "worker-overrides",
    ]


def run(
    update=False,
    query=None,
    try_config=None,
    full=False,
    parameters=None,
    save=False,
    preset=None,
    mod_presets=False,
    stage_changes=False,
    dry_run=False,
    message="{msg}",
    closed_tree=False,
    push_to_lando=False,
):
    from .app import create_application

    push = not stage_changes and not dry_run
    check_working_directory(push)

    tg = generate_tasks(parameters, full)

    # Remove tasks that are not to be shown unless `--full` is specified.
    if not full:
        excluded_tasks = [
            label
            for label in tg.tasks.keys()
            if not filter_by_uncommon_try_tasks(label)
        ]
        for task in excluded_tasks:
            tg.tasks.pop(task)

    queue = multiprocessing.Queue()

    if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        # we are in the reloader process, don't open the browser or do any try stuff
        app = create_application(tg, queue)
        app.run()
        return

    # give app a second to start before opening the browser
    url = "http://127.0.0.1:5000"
    Timer(1, lambda: webbrowser.open(url)).start()
    print("Starting trychooser on {}".format(url))
    process = multiprocessing.Process(
        target=create_and_run_application, args=(tg, queue)
    )
    process.start()

    selected = queue.get()

    # Allow the close page to render before terminating the process.
    time.sleep(1)
    process.terminate()
    if not selected:
        print("no tasks selected")
        return

    msg = "Try Chooser Enhanced ({} tasks selected)".format(len(selected))
    return push_to_try(
        "chooser",
        message.format(msg=msg),
        try_task_config=generate_try_task_config("chooser", selected, try_config),
        stage_changes=stage_changes,
        dry_run=dry_run,
        closed_tree=closed_tree,
        push_to_lando=push_to_lando,
    )


def create_and_run_application(tg, queue: multiprocessing.Queue):
    from .app import create_application

    app = create_application(tg, queue)

    app.run()
