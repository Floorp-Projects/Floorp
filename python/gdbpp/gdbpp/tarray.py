# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb
import itertools
from gdbpp import GeckoPrettyPrinter

@GeckoPrettyPrinter('InfallibleTArray', '^InfallibleTArray<.*>$')
@GeckoPrettyPrinter('FallibleTArray', '^FallibleTArray<.*>$')
@GeckoPrettyPrinter('AutoTArray', '^AutoTArray<.*>$')
@GeckoPrettyPrinter('nsTArray', '^nsTArray<.*>$')
class tarray_printer(object):
    def __init__(self, value):
        self.value = value
        self.elem_type = value.type.template_argument(0)

    def children(self):
        length = self.value['mHdr'].dereference()['mLength']
        data = self.value['mHdr'] + 1
        elements = data.cast(self.elem_type.pointer())
        return (('%d' % i, (elements + i).dereference()) for i in range(0, int(length)))

    def to_string(self):
        return str(self.value.type)

    def display_hint(self):
        return 'array'
