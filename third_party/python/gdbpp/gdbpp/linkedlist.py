# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb
import itertools
from gdbpp import GeckoPrettyPrinter

# mfbt's LinkedList<T> is a doubly-linked list where the items in the list store
# the next/prev pointers as part of themselves rather than the list structure be
# its own independent data structure.  This means:
# - Every item may belong to at most one LinkedList instance.
# - For our pretty printer, we only want to pretty-print the LinkedList object
#   itself.  We do not want to start printing every item in the list whenever
#   we run into a LinkedListElement<T>.
@GeckoPrettyPrinter('mozilla::LinkedList', '^mozilla::LinkedList<.*>$')
class linkedlist_printer(object):
    def __init__(self, value):
        self.value = value
        # mfbt's LinkedList has the elements of the linked list subclass from
        # LinkedListElement<T>.  We want its pointer type for casting purposes.
        #
        # (We want to list pointers since we expect all of these objects to be
        # complex enough that we don't want to automatically expand them.  The
        # LinkedListElement type itself isn't small.)
        self.t_ptr_type = value.type.template_argument(0).pointer()

    def children(self):
        # Walk mNext until we loop back around to the sentinel.  The sentinel
        # item always exists and in the zero-length base-case mNext == sentinel,
        # so extract that immediately and update it throughout the loop.
        sentinel = self.value['sentinel']
        pSentinel = sentinel.address
        pNext = sentinel['mNext']
        i = 0
        while pSentinel != pNext:
            list_elem = pNext.dereference()
            list_value = pNext.cast(self.t_ptr_type)
            yield ('%d' % i, list_value)
            pNext = list_elem['mNext']
            i += 1

    def to_string(self):
        return str(self.value.type)

    def display_hint(self):
        return 'array'
