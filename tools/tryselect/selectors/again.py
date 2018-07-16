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
        [['--purge'],
         {'default': False,
          'action': 'store_true',
          'help': "Remove all history and exit",
          }],
    ]
    common_groups = ['push']


def run_try_again(index=0, purge=False, list_configs=False, message='{msg}', **pushargs):
    if purge:
        os.remove(history_path)
        return

    if not os.path.isfile(history_path):
        print("error: history file not found: {}".format(history_path))
        return 1

    with open(history_path, 'r') as fh:
        history = fh.readlines()

    if list_configs:
        for i, data in enumerate(history):
            msg, config = json.loads(data)
            print('{}. {}'.format(i, msg))
        return

    msg, try_task_config = json.loads(history[index])
    return push_to_try('again', message.format(msg=msg),
                       try_task_config=try_task_config, **pushargs)
