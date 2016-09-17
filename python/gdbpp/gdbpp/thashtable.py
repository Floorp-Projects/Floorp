# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb
import itertools
from gdbpp import GeckoPrettyPrinter

def walk_template_to_given_base(value, desired_tag_prefix):
    '''Given a value of some template subclass, walk up its ancestry until we
    hit the desired type, then return the appropriate value (which will then
    have that type).
    '''
    # Base case
    t = value.type
    # It's possible that we're dealing with an alias template that looks like:
    #   template<typename Protocol>
    #   using ManagedContainer = nsTHashtable<nsPtrHashKey<Protocol>>;
    # In which case we want to strip the indirection, and strip_typedefs()
    # accomplishes this.  (Disclaimer: I tried it and it worked and it didn't
    # break my other use cases, if things start exploding, do reconsider.)
    t = t.strip_typedefs()
    if t.tag.startswith(desired_tag_prefix):
        return value
    for f in t.fields():
        # we only care about the inheritance hierarchy
        if not f.is_base_class:
            continue
        # This is the answer or something we're going to need to recurse into.
        fv = value[f]
        ft = fv.type
        # slightly optimize by checking the tag rather than in the recursion
        if ft.tag.startswith(desired_tag_prefix):
            # found it!
            return fv
        return walk_template_to_given_base(fv, desired_tag_prefix)
    return None

# The templates and their inheritance hierarchy form an onion of types around
# the nsTHashtable core at the center.  All we care about is that nsTHashtable,
# but we register for the descendant types in order to avoid the default pretty
# printers having to unwrap those onion layers, wasting precious lines.
@GeckoPrettyPrinter('nsClassHashtable', '^nsClassHashtable<.*>$')
@GeckoPrettyPrinter('nsDataHashtable', '^nsDataHashtable<.*>$')
@GeckoPrettyPrinter('nsInterfaceHashtable', '^nsInterfaceHashtable<.*>$')
@GeckoPrettyPrinter('nsRefPtrHashtable', '^nsRefPtrHashtable<.*>$')
@GeckoPrettyPrinter('nsBaseHashtable', '^nsBaseHashtable<.*>$')
@GeckoPrettyPrinter('nsTHashtable', '^nsTHashtable<.*>$')
class thashtable_printer(object):
    def __init__(self, outer_value):
        self.outermost_type = outer_value.type

        value = walk_template_to_given_base(outer_value, 'nsTHashtable<')
        self.value = value

        self.entry_type = value.type.template_argument(0)

        # -- Determine whether we're a hashTABLE or a hashSET
        # If we're a table, the entry type will be a nsBaseHashtableET template.
        # If we're a set, it will be something like nsPtrHashKey.
        #
        # So, assume we're a set if we're not nsBaseHashtableET<
        # (It should ideally also be true that the type ends with HashKey, but
        # since nsBaseHashtableET causes us to assume "mData" exists, let's
        # pivot based on that.)
        self.is_table = self.entry_type.tag.startswith('nsBaseHashtableET<')

        # While we know that it has a field `mKeyHash` for the hash-code and
        # book-keeping, and a DataType field mData for the value (if we're a
        # table), the key field frustratingly varies by key type.
        #
        # So we want to walk its key type to figure out the field name.  And we
        # do mean field name.  The field object is no good for subscripting the
        # value unless the field was directly owned by that value's type.  But
        # by using a string name, we save ourselves all that fanciness.

        if self.is_table:
            # For nsBaseHashtableET<KeyClass, DataType>, we want the KeyClass
            key_type = self.entry_type.template_argument(0)
        else:
            # If we're a set, our entry type is the key class already!
            key_type = self.entry_type
        self.key_field_name = None
        for f in key_type.fields():
            # No need to traverse up the type hierarchy...
            if f.is_base_class:
                continue
            # ...just to skip the fields we know exist...
            if f.name == 'mKeyHash' or f.name == 'mData':
                continue
            # ...and assume the first one we find is the key.
            self.key_field_name = f.name
            break

    def children(self):
        table = self.value['mTable']

        # mEntryCount is the number of occupied slots/entries in the table.
        # We can use this to avoid doing wasted memory reads.
        entryCount = table['mEntryCount']
        if entryCount == 0:
            return

        # The table capacity is tracked "cleverly" in terms of how many bits
        # the hash needs to be shifted.  CapacityFromHashShift calculates the
        # actual entry capacity via ((uint32_t)1 << (kHashBits - mHashShift));
        capacity = 1 << (table['kHashBits'] - table['mHashShift'])

        # Pierce generation-tracking EntryStore class to get at buffer.  The
        # class instance always exists, but this char* may be null.
        store = table['mEntryStore']['mEntryStore']

        key_field_name = self.key_field_name

        seenCount = 0
        pEntry = store.cast(self.entry_type.pointer())
        for i in range(0, int(capacity)):
            entry = (pEntry + i).dereference()
            # An mKeyHash of 0 means empty, 1 means deleted sentinel, so skip
            # if that's the case.
            if entry['mKeyHash'] <= 1:
                continue

            yield ('%d' % i, entry[key_field_name])
            if self.is_table:
                yield ('%d' % i, entry['mData'])

            # Stop iterating if we know there are no more occupied slots.
            seenCount += 1
            if seenCount >= entryCount:
                break

    def to_string(self):
        # The most specific template type is the most interesting.
        return str(self.outermost_type)

    def display_hint(self):
        if self.is_table:
            return 'map'
        else:
            return 'array'
