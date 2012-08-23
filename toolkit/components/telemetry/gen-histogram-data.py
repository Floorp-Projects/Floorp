# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out histogram information for C++.  The histograms are defined
# in a file provided as a command-line argument.

from __future__ import with_statement

import sys
import histogram_tools

banner = """/* This file is auto-generated, see gen-histogram-data.py.  */
"""

# Write out the gHistograms array.

def print_array_entry(histogram):
    cpp_guard = histogram.cpp_guard()
    if cpp_guard:
        print "#if defined(%s)" % cpp_guard
    print "  { \"%s\", %s, %s, %s, %s, \"%s\" }," \
        % (histogram.name(), histogram.low(), histogram.high(),
           histogram.n_buckets(), histogram.nsITelemetry_kind(),
           histogram.description())
    if cpp_guard:
        print "#endif"

def write_histogram_table(histograms):
    print "const TelemetryHistogram gHistograms[] = {"
    for histogram in histograms:
        print_array_entry(histogram)
    print "};"

# Write out static asserts for histogram data.  We'd prefer to perform
# these checks in this script itself, but since several histograms
# (generally enumerated histograms) use compile-time constants for
# their upper bounds, we have to let the compiler do the checking.

def static_assert(expression, message):
    print "MOZ_STATIC_ASSERT(%s, \"%s\");" % (expression, message)

def static_asserts_for_boolean(histogram):
    pass

def static_asserts_for_flag(histogram):
    pass

def static_asserts_for_enumerated(histogram):
    n_values = histogram.high()
    static_assert("%s > 2" % n_values,
                  "Not enough values for %s" % histogram.name)

def shared_static_asserts(histogram):
    name = histogram.name()
    low = histogram.low()
    high = histogram.high()
    n_buckets = histogram.n_buckets()
    static_assert("%s < %s" % (low, high), "low >= high for %s" % name)
    static_assert("%s > 2" % n_buckets, "Not enough values for %s" % name)
    static_assert("%s >= 1" % low, "Incorrect low value for %s" % name)

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
        'enumerated' : static_asserts_for_enumerated,
        'linear' : static_asserts_for_linear,
        'exponential' : static_asserts_for_exponential,
        }

    for histogram in histograms:
        histogram_tools.table_dispatch(histogram.kind(), table,
                                       lambda f: f(histogram))

def main(argv):
    filename = argv[0]

    histograms = list(histogram_tools.from_file(filename))

    print banner
    write_histogram_table(histograms)
    write_histogram_static_asserts(histograms)

main(sys.argv[1:])
