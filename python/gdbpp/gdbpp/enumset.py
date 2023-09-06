# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb

from gdbpp import GeckoPrettyPrinter


@GeckoPrettyPrinter("mozilla::EnumSet", "^mozilla::EnumSet<.*>$")
class enumset_printer(object):
    def __init__(self, value):
        self.value = value
        self.enum_type = value.type.template_argument(0)

    def children(self):
        bitfield = self.value["mBitField"]
        max_bit = (self.enum_type.sizeof * 8) - 1
        return (
            ("flag", gdb.Value(i).cast(self.enum_type))
            for i in range(0, max_bit)
            if ((bitfield & (2**i)) != 0)
        )

    def to_string(self):
        return str(self.value.type)

    def display_hint(self):
        return "array"
