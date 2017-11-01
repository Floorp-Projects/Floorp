# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from ..cli import BaseTryParser
from ..vcs import VCSHelper


class EmptyParser(BaseTryParser):
    name = 'empty'
    common_groups = ['push']


def run_empty_try(message='{msg}', push=True, **kwargs):
    vcs = VCSHelper.create()
    msg = 'No try selector specified, use "Add New Jobs" to select tasks.'
    return vcs.push_to_try('empty', message.format(msg=msg), [], push=push,
                           closed_tree=kwargs["closed_tree"])
