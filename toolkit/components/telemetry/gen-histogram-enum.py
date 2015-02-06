# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out a C++ enum definition whose members are the names of
# histograms.  The histograms are defined in a file provided as a
# command-line argument.

import sys
import histogram_tools

banner = """/* This file is auto-generated, see gen-histogram-enum.py.  */
"""

def main(argv):
    filename = argv[0]

    print banner
    print "enum ID : uint32_t {"
    for histogram in histogram_tools.from_file(filename):
        cpp_guard = histogram.cpp_guard()
        if cpp_guard:
            print "#if defined(%s)" % cpp_guard
        print "  %s," % histogram.name()
        if cpp_guard:
            print "#endif"
    print "  HistogramCount"
    print "};"

main(sys.argv[1:])
