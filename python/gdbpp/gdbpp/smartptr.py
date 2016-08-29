# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
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

@GeckoPrettyPrinter('mozilla::StaticAutoPtr', '^mozilla::StaticAutoPtr<.*>$')
@GeckoPrettyPrinter('mozilla::StaticRefPtr', '^mozilla::StaticRefPtr<.*>$')
@GeckoPrettyPrinter('nsAutoPtr', '^nsAutoPtr<.*>$')
@GeckoPrettyPrinter('nsCOMPtr', '^nsCOMPtr<.*>$')
@GeckoPrettyPrinter('RefPtr', '^RefPtr<.*>$')
class smartptr_printer(object):
    def __init__(self, value):
        self.value = value['mRawPtr']

    def to_string(self):
        if not self.value:
            type_name = str(self.value.type)
        else:
            type_name = str(self.value.dereference().dynamic_type.pointer())

        return '[(%s) %s]' % (type_name, str(self.value))

@GeckoPrettyPrinter('mozilla::StyleSheetHandle::RefPtr', '^mozilla::HandleRefPtr<mozilla::StyleSheetHandle>$')
class sheetptr_printer(object):
    def __init__(self, value):
        self.value = value['mHandle']
        if self.value and self.value['mPtr'] and self.value['mPtr']['mValue']:
            self.value = self.value['mPtr']['mValue']

    def to_string(self):
        if not self.value:
            type_name = str(self.value.type)
            value = 0
        else:
            value = int(self.value)
            if value & 0x1:
                value = value & ~0x1
                type_name = 'mozilla::ServoStyleSheet *'
            else:
                type_name = 'mozilla::CSSStyleSheet *'

        return '[(%s) %s]' % (type_name, hex(value))
