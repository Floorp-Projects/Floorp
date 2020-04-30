# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os

from ..cli import BaseTryParser
from ..push import push_to_try, history_path


class AgainParser(BaseTryParser):
    name = 'again'
    arguments = [
        [['--index'],
         {'default': 0,
          'type': int,
          'help': "Index of entry in the history to re-push, "
                  "where '0' is the most recent (default 0). "
                  "Use --list to display indices.",
          }],
        [['--list'],
         {'default': False,
          'action': 'store_true',
          'dest': 'list_configs',
          'help': "Display history and exit",
          }],
        [['--list-tasks'],
         {'default': 0,
          'action': 'count',
          'dest': 'list_tasks',
          'help': "Like --list, but display selected tasks  "
                  "for each history entry, up to 10. Repeat "
                  "to display all selected tasks.",
          }],
        [['--purge'],
         {'default': False,
          'action': 'store_true',
          'help': "Remove all history and exit",
          }],
    ]
    common_groups = ['push']


def run(index=0, purge=False, list_configs=False, list_tasks=0, message='{msg}', **pushargs):
    if purge:
        os.remove(history_path)
        return

    if not os.path.isfile(history_path):
        print("error: history file not found: {}".format(history_path))
        return 1

    with open(history_path, 'r') as fh:
        history = fh.readlines()

    if list_configs or list_tasks > 0:
        for i, data in enumerate(history):
            msg, config = json.loads(data)
            version = config.get('version', '1')
            if version == 1:
                tasks = config['tasks']
            elif version == 2:
                try_config = config.get('parameters', {}).get('try_task_config', {})
                tasks = try_config.get('tasks')
            else:
                tasks = None

            if tasks is not None:
                n = len(tasks)

                print('{index}. ({n} task{s}) {msg}'.format(
                    index=i,
                    msg=msg,
                    n=n,
                    s='' if n == 1 else 's'))

                if list_tasks > 0:
                    indent = ' ' * 4
                    if list_tasks > 1:
                        shown_tasks = tasks
                    else:
                        shown_tasks = tasks[:10]
                    print(indent + ('\n' + indent).join(shown_tasks))

                    num_hidden_tasks = len(tasks) - len(shown_tasks)
                    if num_hidden_tasks > 0:
                        print('{}... and {} more'.format(indent, num_hidden_tasks))
            else:
                print('{index}. {msg}'.format(
                    index=i,
                    msg=msg,
                ))

        return

    msg, try_task_config = json.loads(history[index])
    return push_to_try('again', message.format(msg=msg),
                       try_task_config=try_task_config, **pushargs)
