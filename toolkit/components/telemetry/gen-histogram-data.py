# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out histogram information for C++.  The histograms are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from shared_telemetry_utils import StringTable, static_assert

import sys
import histogram_tools
import itertools

banner = """/* This file is auto-generated, see gen-histogram-data.py.  */
"""

def print_array_entry(output, histogram, name_index, exp_index, label_index, label_count):
    cpp_guard = histogram.cpp_guard()
    if cpp_guard:
        print("#if defined(%s)" % cpp_guard, file=output)
    print("  { %s, %s, %s, %s, %d, %d, %s, %d, %d, %s }," \
        % (histogram.low(),
           histogram.high(),
           histogram.n_buckets(),
           histogram.nsITelemetry_kind(),
           name_index,
           exp_index,
           histogram.dataset(),
           label_index,
           label_count,
           "true" if histogram.keyed() else "false"), file=output)
    if cpp_guard:
        print("#endif", file=output)

def write_histogram_table(output, histograms):
    string_table = StringTable()
    label_table = []
    label_count = 0

    print("const HistogramInfo gHistograms[] = {", file=output)
    for histogram in histograms:
        name_index = string_table.stringIndex(histogram.name())
        exp_index = string_table.stringIndex(histogram.expiration())

        labels = histogram.labels()
        label_index = 0
        if len(labels) > 0:
            label_index = label_count
            label_table.append((histogram.name(), string_table.stringIndexes(labels)))
            label_count += len(labels)

        print_array_entry(output, histogram,
                          name_index, exp_index,
                          label_index, len(labels))
    print("};\n", file=output)

    strtab_name = "gHistogramStringTable"
    string_table.writeDefinition(output, strtab_name)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % strtab_name,
                  "index overflow")

    print("\nconst uint32_t gHistogramLabelTable[] = {", file=output)
    for name,indexes in label_table:
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
    static_assert(output, "%s > %s" % (high, n_buckets),
                  "high must be > number of buckets for %s; you may want an enumerated histogram" % name)

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
        'boolean' : static_asserts_for_boolean,
        'flag' : static_asserts_for_flag,
        'count': static_asserts_for_count,
        'enumerated' : static_asserts_for_enumerated,
        'categorical' : static_asserts_for_enumerated,
        'linear' : static_asserts_for_linear,
        'exponential' : static_asserts_for_exponential,
        }

    for histogram in histograms:
        histogram_tools.table_dispatch(histogram.kind(), table,
                                       lambda f: f(output, histogram))

def write_debug_histogram_ranges(output, histograms):
    ranges_lengths = []

    # Collect all the range information from individual histograms.
    # Write that information out as well.
    print("#ifdef DEBUG", file=output)
    print("const int gBucketLowerBounds[] = {", file=output)
    for histogram in histograms:
        ranges = []
        try:
            ranges = histogram.ranges()
        except histogram_tools.DefinitionException:
            pass
        ranges_lengths.append(len(ranges))
        # Note that we do not test cpp_guard here.  We do this so we
        # will have complete information about all the histograms in
        # this array.  Just having information about the ranges of
        # histograms is not platform-specific; if there are histograms
        # that have platform-specific constants in their definitions,
        # those histograms will fail in the .ranges() call above and
        # we'll have a zero-length array to deal with here.
        if len(ranges) > 0:
            print(','.join(map(str, ranges)), ',', file=output)
        else:
            print('/* Skipping %s */' % histogram.name(), file=output)
    print("};", file=output)

    # Write the offsets into gBucketLowerBounds.
    print("struct bounds { int offset; int length; };", file=output)
    print("const struct bounds gBucketLowerBoundIndex[] = {", file=output)
    offset = 0
    for (histogram, range_length) in itertools.izip(histograms, ranges_lengths):
        cpp_guard = histogram.cpp_guard()
        # We do test cpp_guard here, so that histogram IDs are valid
        # indexes into this array.
        if cpp_guard:
            print("#if defined(%s)" % cpp_guard, file=output)
        print("{ %d, %d }," % (offset, range_length), file=output)
        if cpp_guard:
            print("#endif", file=output)
        offset += range_length
    print("};", file=output)
    print("#endif", file=output)

def main(output, *filenames):
    histograms = list(histogram_tools.from_files(filenames))

    print(banner, file=output)
    write_histogram_table(output, histograms)
    write_histogram_static_asserts(output, histograms)
    write_debug_histogram_ranges(output, histograms)

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
