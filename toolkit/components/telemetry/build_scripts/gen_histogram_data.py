# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out histogram information for C++.  The histograms are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from mozparsers.shared_telemetry_utils import (
    StringTable,
    static_assert,
    ParserError
)
from mozparsers import parse_histograms

import sys
import buildconfig

banner = """/* This file is auto-generated, see gen_histogram_data.py.  */
"""


def print_array_entry(output, histogram, name_index, exp_index,
                      label_index, label_count,
                      key_index, key_count,
                      store_index, store_count):
    if histogram.record_on_os(buildconfig.substs["OS_TARGET"]):
        print("  { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, %s, %s, %s, %s },"
              % (histogram.low(),
                 histogram.high(),
                 histogram.n_buckets(),
                 name_index,
                 exp_index,
                 label_count,
                 key_count,
                 store_count,
                 label_index,
                 key_index,
                 store_index,
                 "true" if histogram.keyed() else "false",
                 histogram.nsITelemetry_kind(),
                 histogram.dataset(),
                 " | ".join(histogram.record_in_processes_enum()),
                 " | ".join(histogram.products_enum())), file=output)


def write_histogram_table(output, histograms):
    string_table = StringTable()

    label_table = []
    label_count = 0
    keys_table = []
    keys_count = 0
    store_table = []
    total_store_count = 0

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

        stores = histogram.record_into_store()
        store_index = 0
        if stores == ["main"]:
            # if count == 1 && offset == UINT16_MAX -> only main store
            store_index = 'UINT16_MAX'
        else:
            store_index = total_store_count
            store_table.append((histogram.name(), string_table.stringIndexes(stores)))
            total_store_count += len(stores)

        print_array_entry(output, histogram, name_index, exp_index,
                          label_index, len(labels), key_index, len(keys),
                          store_index, len(stores))
    print("};\n", file=output)

    strtab_name = "gHistogramStringTable"
    string_table.writeDefinition(output, strtab_name)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % strtab_name,
                  "index overflow")

    print("\n#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const uint32_t gHistogramLabelTable[] = {", file=output)
    print("#else", file=output)
    print("constexpr uint32_t gHistogramLabelTable[] = {", file=output)
    print("#endif", file=output)
    for name, indexes in label_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)
    static_assert(output, "sizeof(gHistogramLabelTable) <= UINT16_MAX", "index overflow")

    print("\n#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const uint32_t gHistogramKeyTable[] = {", file=output)
    print("#else", file=output)
    print("constexpr uint32_t gHistogramKeyTable[] = {", file=output)
    print("#endif", file=output)
    for name, indexes in keys_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)
    static_assert(output, "sizeof(gHistogramKeyTable) <= UINT16_MAX", "index overflow")

    store_table_name = "gHistogramStoresTable"
    print("\n#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const uint32_t {}[] = {{".format(store_table_name), file=output)
    print("#else", file=output)
    print("constexpr uint32_t {}[] = {{".format(store_table_name), file=output)
    print("#endif", file=output)
    for name, indexes in store_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT16_MAX" % store_table_name,
                  "index overflow")


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

    target_os = buildconfig.substs["OS_TARGET"]
    for histogram in histograms:
        kind = histogram.kind()
        if not histogram.record_on_os(target_os):
            continue

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
    print("#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const int gHistogramBucketLowerBounds[] = {", file=output)
    print("#else", file=output)
    print("constexpr int gHistogramBucketLowerBounds[] = {", file=output)
    print("#endif", file=output)

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

    target_os = buildconfig.substs["OS_TARGET"]
    print("#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const int16_t gHistogramBucketLowerBoundIndex[] = {", file=output)
    print("#else", file=output)
    print("constexpr int16_t gHistogramBucketLowerBoundIndex[] = {", file=output)
    print("#endif", file=output)
    for histogram in histograms:
        if histogram.record_on_os(target_os):
            our_offset = ranges_offsets[tuple(histogram.ranges())]
            print("%d," % our_offset, file=output)

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
