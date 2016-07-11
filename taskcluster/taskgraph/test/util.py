# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from ..task import base


class TestTask(base.Task):

    def __init__(self, kind=None, label=None, attributes=None, task=None):
        super(TestTask, self).__init__(
                kind or 'test',
                label or 'test-label',
                attributes or {},
                task or {})

    @classmethod
    def load_tasks(cls, kind, path, config, parameters):
        return []

    def get_dependencies(self, taskgraph):
        return []
