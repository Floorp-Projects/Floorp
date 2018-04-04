#!/usr/bin/env python
# perfecthash.py - Helper for generating perfect hash functions for xptcodegen.py
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# A perfect hash function (PHF) is a function which maps distinct elements from
# a source set to a set of integers with no collisions. Perfect hash functions
# created by perfecthash.py take in a mapping from a key bytearray to an output
# value. The generated PHF uses a 32-bit FNV hash to index into an intermediate
# table. The value from that table is then used to feed into a second 32-bit FNV
# hash to get the index into the final table.
#
# This is done by starting with the largest set of conflicts, and guessing
# intermediate table values such that we generate no conflicts. This then allows
# us to do a constant-time lookup at runtime into these tables.

from collections import namedtuple

# 32-bit FNV offset basis and prime value.
FNV_OFFSET_BASIS = 0x811C9DC5
FNV_PRIME = 16777619

# We use uint32_ts for our arrays in PerfectHash. 0x80000000 is the high bit
# which we sometimes use as a flag.
U32_HIGH_BIT = 0x80000000

# A basic FNV-based hash function. bytes is the bytearray to hash. 32-bit FNV is
# used for indexing into the first table, and the value stored in that table is
# used as the offset basis for indexing into the values table.
#
# NOTE: C++ implementation is in xptinfo.cpp
def hash(bytes, h=FNV_OFFSET_BASIS):
    for byte in bytes:
        h ^= byte       # xor-in the byte
        h *= FNV_PRIME  # Multiply by the FNV prime
        h &= 0xffffffff # clamp to 32-bits
    return h

IntermediateBucket = namedtuple('IntermediateBucket', ['index', 'entries'])
HashEntry = namedtuple('HashEntry', ['key', 'value'])

class PerfectHash(object):
    """An object representing a perfect hash function"""

    def __init__(self, intermediate_table_size, data):
        # data should be a list of (bytearray, value) pairs

        self.intermediate = [0] * intermediate_table_size
        self.values = [None] * len(data)

        assert len(self.values) < U32_HIGH_BIT, \
            "Not enough space in uint32_t to index %d values" % len(self.values)

        # Buckets contains a set of IntermediateBucket values. Each bucket
        # contains the index into the intermediate table, and the set of entries
        # which map into that table.
        buckets = [IntermediateBucket(index=idx, entries=[])
                   for idx in range(len(self.intermediate))]

        # Determine which input strings map to which buckets in the intermediate
        # array.
        for key, value in data:
            assert isinstance(key, bytearray), \
                "data should be a list of (bytearray, value) pairs"
            assert value is not None, "cannot handle a None value"

            buckets[hash(key) % len(self.intermediate)].entries \
                .append(HashEntry(key=key, value=value))

        # Sort the buckets such that the largest one first.
        buckets.sort(key=lambda b: len(b.entries), reverse=True)

        freecursor = 0
        for bucket in buckets:
            # If we've reached buckets with no conflicts, we can just start
            # storing direct indices into the final array. Once we reach an
            # empty bucket, we're done. The high bit is set to identify direct
            # indices into the final array.
            if len(bucket.entries) == 0:
                break
            elif len(bucket.entries) == 1:
                while freecursor < len(self.values):
                    if self.values[freecursor] is None:
                        self.intermediate[bucket.index] = freecursor | U32_HIGH_BIT
                        self.values[freecursor] = bucket.entries[0].value
                        break
                    freecursor += 1
                continue

            # Try values for the basis until we find one with no conflicts.
            idx = 0
            basis = 1
            slots = []
            while idx < len(bucket.entries):
                slot = hash(bucket.entries[idx].key, basis) % len(self.values)
                if self.values[slot] is not None or slot in slots:
                    # There was a conflict, try the next basis.
                    basis += 1
                    idx = 0
                    del slots[:]
                else:
                    slots.append(slot)
                    idx += 1

            assert basis < U32_HIGH_BIT, \
                "not enough space in uint32_t to store basis %d" % basis

            # We've found a basis which doesn't conflict
            self.intermediate[bucket.index] = basis
            for slot, entry in zip(slots, bucket.entries):
                self.values[slot] = entry.value

    def lookup(self, key):
        mid = self.intermediate[hash(key) % len(self.intermediate)]
        if mid & U32_HIGH_BIT:  # direct index
            return self.values[mid & ~U32_HIGH_BIT]
        else:
            return self.values[hash(key, mid) % len(self.values)]
