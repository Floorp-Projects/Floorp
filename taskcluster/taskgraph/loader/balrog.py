# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals


def loader(kind, path, config, params, loaded_tasks):
    """
    Load tasks implementing balrog submission jobs.  These depend on beetmover
    jobs and submit the update to balrog as available after the files are moved
    into place
    """
    if config.get('kind-dependencies', []) != ["beetmover", "beetmover-l10n"]:
        raise Exception("Balrog kinds must depend on beetmover kinds")
    for task in loaded_tasks:
        if not task.attributes.get('nightly'):
            continue
        if task.kind not in config.get('kind-dependencies', []):
            continue
        beetmover_task = {}
        beetmover_task['dependent-task'] = task

        yield beetmover_task
