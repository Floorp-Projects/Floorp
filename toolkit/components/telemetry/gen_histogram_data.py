# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out histogram information for C++.  The histograms are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from shared_telemetry_utils import StringTable, static_assert, ParserError

import sys
import parse_histograms

banner = """/* This file is auto-generated, see gen_histogram_data.py.  */
"""


def print_array_entry(output, histogram, name_index, exp_index, label_index,
                      label_count, key_index, key_count):
    cpp_guard = histogram.cpp_guard()
    if cpp_guard:
        print("#if defined(%s)" % cpp_guard, file=output)
    print("  { %s, %s, %s, %s, %d, %d, %s, %d, %d, %d, %d, %s, %s },"
          % (histogram.low(),
             histogram.high(),
             histogram.n_buckets(),
             histogram.nsITelemetry_kind(),
             name_index,
             exp_index,
             histogram.dataset(),
             label_index,
             label_count,
             key_index,
             key_count,
             " | ".join(histogram.record_in_processes_enum()),
             "true" if histogram.keyed() else "false"), file=output)
    if cpp_guard:
        print("#endif", file=output)


def write_histogram_table(output, histograms):
    string_table = StringTable()
    label_table = []
    label_count = 0
    keys_table = []
    keys_count = 0

    print("constexpr HistogramInfo gHistogramInfos[] = {", file=output)
    for histogram in histograms:
        name_index = string_table.stringIndex(histogram.name())
        exp_index = string_table.stringIndex(histogram.expiration())

        labels = histogram.labels()
        label_index = 0
        if len(labels) > 0:
            label_index = label_count
            label_table.append((histogram.name(), string_table.stringIndexes(labels)))
            label_count += len(labels)

        keys = histogram.keys()
        key_index = 0
        if len(keys) > 0:
            key_index = keys_count
            keys_table.append((histogram.name(), string_table.stringIndexes(keys)))
            keys_count += len(keys)

        print_array_entry(output, histogram, name_index, exp_index,
                          label_index, len(labels), key_index, len(keys))
    print("};\n", file=output)

    strtab_name = "gHistogramStringTable"
    string_table.writeDefinition(output, strtab_name)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % strtab_name,
                  "index overflow")

    print("\nconst uint32_t gHistogramLabelTable[] = {", file=output)
    for name, indexes in label_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)

    print("\nconst uint32_t gHistogramKeyTable[] = {", file=output)
    for name, indexes in keys_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)


# Write out static asserts for histogram data.  We'd prefer to perform
# these checks in this script itself, but since several histograms
# (generally enumerated histograms) use compile-time constants for
# their upper bounds, we have to let the compiler do the checking.

def static_asserts_for_boolean(output, histogram):
    pass


def static_asserts_for_flag(output, histogram):
    pass


def static_asserts_for_count(output, histogram):
    pass


def static_asserts_for_enumerated(output, histogram):
    n_values = histogram.high()
    static_assert(output, "%s > 2" % n_values,
                  "Not enough values for %s" % histogram.name())


def shared_static_asserts(output, histogram):
    name = histogram.name()
    low = histogram.low()
    high = histogram.high()
    n_buckets = histogram.n_buckets()
    static_assert(output, "%s < %s" % (low, high), "low >= high for %s" % name)
    static_assert(output, "%s > 2" % n_buckets, "Not enough values for %s" % name)
    static_assert(output, "%s >= 1" % low, "Incorrect low value for %s" % name)
    static_assert(output, "%s > %s" % (high, n_buckets), "high must be > number of buckets for %s;"
                  " you may want an enumerated histogram" % name)


def static_asserts_for_linear(output, histogram):
    shared_static_asserts(output, histogram)


def static_asserts_for_exponential(output, histogram):
    shared_static_asserts(output, histogram)


def write_histogram_static_asserts(output, histograms):
    print("""
// Perform the checks at the beginning of HistogramGet at
// compile time, so that incorrect histogram definitions
// give compile-time errors, not runtime errors.""", file=output)

    table = {
        'boolean': static_asserts_for_boolean,
        'flag': static_asserts_for_flag,
        'count': static_asserts_for_count,
        'enumerated': static_asserts_for_enumerated,
        'categorical': static_asserts_for_enumerated,
        'linear': static_asserts_for_linear,
        'exponential': static_asserts_for_exponential,
    }

    for histogram in histograms:
        kind = histogram.kind()
        if kind not in table:
            raise Exception('Unknown kind "%s" for histogram "%s".' % (kind, histogram.name()))
        fn = table[kind]
        fn(output, histogram)


def write_histogram_ranges(output, histograms):
    # This generates static data to avoid costly initialization of histograms
    # (especially exponential ones which require log and exp calls) at runtime.
    # The format must exactly match that required in histogram.cc, which is
    # 0, buckets..., INT_MAX. Additionally, the list ends in a 0 to aid asserts
    # that validate that the length of the ranges list is correct.U cache miss.
    print("const int gHistogramBucketLowerBounds[] = {", file=output)

    # Print the dummy buckets for expired histograms, and set the offset to match.
    print("0,1,2,INT_MAX,", file=output)
    offset = 4
    ranges_offsets = {}

    for histogram in histograms:
        ranges = tuple(histogram.ranges())
        if ranges not in ranges_offsets:
            ranges_offsets[ranges] = offset
            # Suffix each ranges listing with INT_MAX, to match histogram.cc's
            # expected format.
            offset += len(ranges) + 1
            print(','.join(map(str, ranges)), ',INT_MAX,', file=output)
    print("0};", file=output)

    if offset > 32767:
        raise Exception('Histogram offsets exceeded maximum value for an int16_t.')

    print("const int16_t gHistogramBucketLowerBoundIndex[] = {", file=output)
    for histogram in histograms:
        cpp_guard = histogram.cpp_guard()
        if cpp_guard:
            print("#if defined(%s)" % cpp_guard, file=output)

        our_offset = ranges_offsets[tuple(histogram.ranges())]
        print("%d," % our_offset, file=output)

        if cpp_guard:
            print("#endif", file=output)
    print("};", file=output)


def main(output, *filenames):
    try:
        histograms = list(parse_histograms.from_files(filenames))
    except ParserError as ex:
        print("\nError processing histograms:\n" + str(ex) + "\n")
        sys.exit(1)

    print(banner, file=output)
    write_histogram_table(output, histograms)
    write_histogram_ranges(output, histograms)
    write_histogram_static_asserts(output, histograms)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
