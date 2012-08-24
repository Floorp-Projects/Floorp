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

# Write out the gHistograms array.  Try and ensure the user didn't
# provide bogus keys in the histogram definition.

def check_keys(name, definition, allowed_keys):
    for key in definition.iterkeys():
        if key not in allowed_keys:
            raise KeyError, '%s not permitted for %s' % (key, name)

def print_array_entry(name, low, high, n_buckets, kind, definition):
    cpp_guard = definition.get('cpp_guard')
    if cpp_guard:
        print "#if defined(%s)" % cpp_guard
    print "  { \"%s\", %s, %s, %s, nsITelemetry::HISTOGRAM_%s, \"%s\" }," \
        % (name, low, high, n_buckets, kind, definition['description'])
    if cpp_guard:
        print "#endif"

def process_boolean(name, definition):
    print_array_entry(name, 0, 1, 2, 'BOOLEAN', definition)

def process_flag(name, definition):
    print_array_entry(name, 0, 1, 2, 'FLAG', definition)

def process_enumerated_values(name, definition):
    n_values = definition['n_values']
    print_array_entry(name, 1, n_values, "%s+1" % n_values, 'LINEAR',
                      definition)

def process_general_histogram(name, definition):
    low = definition.get('low', 1)
    high = definition['high']
    n_buckets = definition['n_buckets']
    print_array_entry(name, low, high, n_buckets,
                      definition['kind'].upper(), definition)

always_allowed_keys = ['kind', 'description', 'cpp_guard']

def write_histogram_table(histograms):
    general_keys = ['low', 'high', 'n_buckets'] + always_allowed_keys

    table = {
        'boolean' : (process_boolean, always_allowed_keys),
        'flag' : (process_flag, always_allowed_keys),
        'enumerated' : (process_enumerated_values, ['n_values'] + always_allowed_keys),
        'linear' : (process_general_histogram, general_keys),
        'exponential' : (process_general_histogram, general_keys),
        }

    print "const TelemetryHistogram gHistograms[] = {"
    for (name, definition) in histograms:
        kind = definition['kind']
        if kind in table:
            (processor, allowed_keys) = table[kind]
            check_keys(name, definition, allowed_keys)
            processor(name, definition)
        else:
            print "Don't know how to handle a histogram of kind ", kind
            sys.exit(1)
    print "};"

# Write out static asserts for histogram data.  We'd prefer to perform
# these checks in this script itself, but since several histograms
# (generally enumerated histograms) use compile-time constants for
# their upper bounds, we have to let the compiler do the checking.

def static_assert(expression, message):
    print "MOZ_STATIC_ASSERT(%s, \"%s\");" % (expression, message)

def static_asserts_for_boolean(name, definition):
    pass

def static_asserts_for_flag(name, definition):
    pass

def static_asserts_for_enumerated(name, definition):
    n_values = definition['n_values']
    static_assert("%s > 2" % n_values, "Not enough values for %s" % name)

def shared_static_asserts(name, definition):
    low = definition.get('low', 1)
    high = definition['high']
    n_buckets = definition['n_buckets']
    static_assert("%s < %s" % (low, high), "low >= high for %s" % name)
    static_assert("%s > 2" % n_buckets, "Not enough values for %s" % name)
    static_assert("%s >= 1" % low, "Incorrect low value for %s" % name)

def static_asserts_for_linear(name, definition):
    shared_static_asserts(name, definition)

def static_asserts_for_exponential(name, definition):
    shared_static_asserts(name, definition)

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

    for (name, definition) in histograms:
        kind = definition['kind']
        if kind in table:
            cpp_guard = definition.get('cpp_guard')
            if cpp_guard:
                print "#if defined(%s)" % cpp_guard
            table[kind](name, definition)
            if cpp_guard:
                print "#endif"
        else:
            print "Don't know how to handle a histogram of kind", kind
            sys.exit(1)

def main(argv):
    filename = argv[0]

    histograms = list(histogram_tools.from_file(filename))

    print banner
    write_histogram_table(histograms)
    write_histogram_static_asserts(histograms)

main(sys.argv[1:])
