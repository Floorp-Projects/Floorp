# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import gdb
import gdb.printing


class GeckoPrettyPrinter(object):
    pp = gdb.printing.RegexpCollectionPrettyPrinter('GeckoPrettyPrinters')

    def __init__(self, name, regexp):
        self.name = name
        self.regexp = regexp

    def __call__(self, wrapped):
        GeckoPrettyPrinter.pp.add_printer(self.name, self.regexp, wrapped)
        return wrapped


import gdbpp.enumset  # NOQA: F401
import gdbpp.linkedlist  # NOQA: F401
import gdbpp.owningthread  # NOQA: F401
import gdbpp.smartptr  # NOQA: F401
import gdbpp.string  # NOQA: F401
import gdbpp.tarray  # NOQA: F401
import gdbpp.thashtable  # NOQA: F401

gdb.printing.register_pretty_printer(None, GeckoPrettyPrinter.pp)
