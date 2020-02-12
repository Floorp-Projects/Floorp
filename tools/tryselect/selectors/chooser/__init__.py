# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import webbrowser
from threading import Timer

from tryselect.cli import BaseTryParser
from tryselect.tasks import generate_tasks
from tryselect.push import check_working_directory, push_to_try, generate_try_task_config

here = os.path.abspath(os.path.dirname(__file__))


class ChooserParser(BaseTryParser):
    name = 'chooser'
    arguments = []
    common_groups = ['push', 'task']
    task_configs = [
        'artifact',
        'browsertime',
        'chemspill-prio',
        'disable-pgo',
        'env',
        'gecko-profile',
        'path',
        'pernosco',
        'rebuild',
        'worker-overrides',
    ]


def run(update=False, query=None, try_config=None, full=False, parameters=None,
        save=False, preset=None, mod_presets=False, push=True, message='{msg}',
        closed_tree=False):
    from .app import create_application
    check_working_directory(push)

    tg = generate_tasks(parameters, full)
    app = create_application(tg)

    if os.environ.get('WERKZEUG_RUN_MAIN') == 'true':
        # we are in the reloader process, don't open the browser or do any try stuff
        app.run()
        return

    # give app a second to start before opening the browser
    url = 'http://127.0.0.1:5000'
    Timer(1, lambda: webbrowser.open(url)).start()
    print("Starting trychooser on {}".format(url))
    app.run()

    selected = app.tasks
    if not selected:
        print("no tasks selected")
        return

    msg = "Try Chooser Enhanced ({} tasks selected)".format(len(selected))
    return push_to_try('chooser', message.format(msg=msg),
                       try_task_config=generate_try_task_config('chooser', selected, try_config),
                       push=push, closed_tree=closed_tree)
