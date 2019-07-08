# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from ..cli import BaseTryParser
from ..push import push_to_try, generate_try_task_config


class EmptyParser(BaseTryParser):
    name = 'empty'
    common_groups = ['push']


def run(message='{msg}', push=True, closed_tree=False):
    msg = 'No try selector specified, use "Add New Jobs" to select tasks.'
    return push_to_try('empty', message.format(msg=msg),
                       try_task_config=generate_try_task_config('empty', []),
                       push=push, closed_tree=closed_tree)
