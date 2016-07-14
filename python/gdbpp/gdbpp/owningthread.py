# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb
from gdbpp import GeckoPrettyPrinter

@GeckoPrettyPrinter('nsAutoOwningThread', '^nsAutoOwningThread$')
class owning_thread_printer(object):
    def __init__(self, value):
        self.value = value

    def to_string(self):
        prthread_type = gdb.lookup_type('PRThread').pointer()
        prthread = self.value['mThread'].cast(prthread_type)
        name = prthread['name']

        # if the thread doesn't have a name try to get its thread id (might not
        # work on !linux)
        name = prthread['tid']

        return name if name else '(PRThread *) %s' % prthread
