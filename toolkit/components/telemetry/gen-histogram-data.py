# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out histogram information for C++.  The histograms are defined
# in a file provided as a command-line argument.

from __future__ import with_statement

import sys
import histogram_tools
import itertools

banner = """/* This file is auto-generated, see gen-histogram-data.py.  */
"""

# Write out the gHistograms array.

class StringTable:
    def __init__(self):
        self.current_index = 0;
        self.table = {}

    def c_strlen(self, string):
        return len(string) + 1

    def stringIndex(self, string):
        if string in self.table:
            return self.table[string]
        else:
            result = self.current_index
            self.table[string] = result
            self.current_index += self.c_strlen(string)
            return result

    def writeDefinition(self, f, name):
        entries = self.table.items()
        entries.sort(key=lambda x:x[1])
        # Avoid null-in-string warnings with GCC and potentially
        # overlong string constants; write everything out the long way.
        def explodeToCharArray(string):
            def toCChar(s):
                if s == "'":
                    return "'\\''"
                else:
                    return "'%s'" % s
            return ", ".join(map(toCChar, string))
        f.write("const char %s[] = {\n" % name)
        for (string, offset) in entries[:-1]:
            e = explodeToCharArray(string)
            if e:
                f.write("  /* %5d */ %s, '\\0',\n"
                        % (offset, explodeToCharArray(string)))
            else:
                f.write("  /* %5d */ '\\0',\n" % offset)
        f.write("  /* %5d */ %s, '\\0' };\n\n"
                % (entries[-1][1], explodeToCharArray(entries[-1][0])))

def print_array_entry(histogram, name_index, exp_index):
    cpp_guard = histogram.cpp_guard()
    if cpp_guard:
        print "#if defined(%s)" % cpp_guard
    print "  { %s, %s, %s, %s, %d, %d, %s, %s, %s }," \
        % (histogram.low(), histogram.high(),
           histogram.n_buckets(), histogram.nsITelemetry_kind(),
           name_index, exp_index, histogram.dataset(),
           "true" if histogram.extended_statistics_ok() else "false",
           "true" if histogram.keyed() else "false")
    if cpp_guard:
        print "#endif"

def write_histogram_table(histograms):
    table = StringTable()

    print "const TelemetryHistogram gHistograms[] = {"
    for histogram in histograms:
        name_index = table.stringIndex(histogram.name())
        exp_index = table.stringIndex(histogram.expiration())
        print_array_entry(histogram, name_index, exp_index)
    print "};"

    strtab_name = "gHistogramStringTable"
    table.writeDefinition(sys.stdout, strtab_name)
    static_assert("sizeof(%s) <= UINT32_MAX" % strtab_name,
                  "index overflow")

# Write out static asserts for histogram data.  We'd prefer to perform
# these checks in this script itself, but since several histograms
# (generally enumerated histograms) use compile-time constants for
# their upper bounds, we have to let the compiler do the checking.

def static_assert(expression, message):
    print "static_assert(%s, \"%s\");" % (expression, message)

def static_asserts_for_boolean(histogram):
    pass

def static_asserts_for_flag(histogram):
    pass

def static_asserts_for_count(histogram):
    pass

def static_asserts_for_enumerated(histogram):
    n_values = histogram.high()
    static_assert("%s > 2" % n_values,
                  "Not enough values for %s" % histogram.name())

def shared_static_asserts(histogram):
    name = histogram.name()
    low = histogram.low()
    high = histogram.high()
    n_buckets = histogram.n_buckets()
    static_assert("%s < %s" % (low, high), "low >= high for %s" % name)
    static_assert("%s > 2" % n_buckets, "Not enough values for %s" % name)
    static_assert("%s >= 1" % low, "Incorrect low value for %s" % name)
    static_assert("%s > %s" % (high, n_buckets),
                  "high must be > number of buckets for %s; you may want an enumerated histogram" % name)

def static_asserts_for_linear(histogram):
    shared_static_asserts(histogram)

def static_asserts_for_exponential(histogram):
    shared_static_asserts(histogram)

def write_histogram_static_asserts(histograms):
    print """
// Perform the checks at the beginning of HistogramGet at
// compile time, so that incorrect histogram definitions
// give compile-time errors, not runtime errors."""

    table = {
        'boolean' : static_asserts_for_boolean,
        'flag' : static_asserts_for_flag,
        'count': static_asserts_for_count,
        'enumerated' : static_asserts_for_enumerated,
        'linear' : static_asserts_for_linear,
        'exponential' : static_asserts_for_exponential,
        }

    for histogram in histograms:
        histogram_tools.table_dispatch(histogram.kind(), table,
                                       lambda f: f(histogram))

def write_debug_histogram_ranges(histograms):
    ranges_lengths = []

    # Collect all the range information from individual histograms.
    # Write that information out as well.
    print "#ifdef DEBUG"
    print "const int gBucketLowerBounds[] = {"
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
            print ','.join(map(str, ranges)), ','
        else:
            print '/* Skipping %s */' % histogram.name()
    print "};"

    # Write the offsets into gBucketLowerBounds.
    print "struct bounds { int offset; int length; };"
    print "const struct bounds gBucketLowerBoundIndex[] = {"
    offset = 0
    for (histogram, range_length) in itertools.izip(histograms, ranges_lengths):
        cpp_guard = histogram.cpp_guard()
        # We do test cpp_guard here, so that histogram IDs are valid
        # indexes into this array.
        if cpp_guard:
            print "#if defined(%s)" % cpp_guard
        print "{ %d, %d }," % (offset, range_length)
        if cpp_guard:
            print "#endif"
        offset += range_length
    print "};"
    print "#endif"

def main(argv):
    filename = argv[0]

    histograms = list(histogram_tools.from_file(filename))

    print banner
    write_histogram_table(histograms)
    write_histogram_static_asserts(histograms)
    write_debug_histogram_ranges(histograms)

main(sys.argv[1:])
