# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb
from gdbpp import GeckoPrettyPrinter

@GeckoPrettyPrinter('nsWeakPtr', '^nsCOMPtr<nsIWeakReference>$')
class weak_ptr_printer(object):
    def __init__(self, value):
        self.value = value

    def to_string(self):
        proxy = self.value['mRawPtr']
        if not proxy:
            return '[(%s) 0x0]' % proxy.type

        ref_type = proxy.dynamic_type
        weak_ptr = proxy.cast(ref_type).dereference()['mReferent']
        if not weak_ptr:
            return '[(%s) %s]' % (weak_ptr.type, weak_ptr)

        return '[(%s) %s]' % (weak_ptr.dynamic_type, weak_ptr)

@GeckoPrettyPrinter('nsAutoPtr', 'nsAutoPtr<.*>')
@GeckoPrettyPrinter('nsCOMPtr', 'nsCOMPtr<.*>')
@GeckoPrettyPrinter('RefPtr', 'RefPtr<.*>')
class smartptr_printer(object):
    def __init__(self, value):
        self.value = value['mRawPtr']

    def to_string(self):
        if not self.value:
            type_name = str(self.value.type)
        else:
            type_name = str(self.value.dereference().dynamic_type.pointer())

        return '[(%s) %s]' % (type_name, str(self.value))
