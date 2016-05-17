# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out a C++ enum definition whose members are the names of
# histograms as well as the following other members:
#
#   - HistogramCount
#   - HistogramFirstUseCounter
#   - HistogramLastUseCounter
#   - HistogramUseCounterCount
#
# The histograms are defined in files provided as command-line arguments.

from __future__ import print_function

import histogram_tools
import itertools
import sys

banner = """/* This file is auto-generated, see gen-histogram-enum.py.  */
"""

def main(output, *filenames):
    print(banner, file=output)
    print("#ifndef mozilla_TelemetryHistogramEnums_h", file=output);
    print("#define mozilla_TelemetryHistogramEnums_h", file=output);
    print("namespace mozilla {", file=output)
    print("namespace Telemetry {", file=output)
    print("enum ID : uint32_t {", file=output)

    groups = itertools.groupby(histogram_tools.from_files(filenames),
                               lambda h: h.name().startswith("USE_COUNTER2_"))
    seen_use_counters = False

    # Note that histogram_tools.py guarantees that all of the USE_COUNTER2_*
    # histograms are defined in a contiguous block.  We therefore assume
    # that there's at most one group for which use_counter_group is true.
    for (use_counter_group, histograms) in groups:
        if use_counter_group:
            seen_use_counters = True

        # The HistogramDUMMY* enum variables are used to make the computation
        # of Histogram{First,Last}UseCounter easier.  Otherwise, we'd have to
        # special case the first and last histogram in the group.
        if use_counter_group:
            print("  HistogramFirstUseCounter,", file=output)
            print("  HistogramDUMMY1 = HistogramFirstUseCounter - 1,", file=output)

        for histogram in histograms:
            cpp_guard = histogram.cpp_guard()
            if cpp_guard:
                print("#if defined(%s)" % cpp_guard, file=output)
            print("  %s," % histogram.name(), file=output)
            if cpp_guard:
                print("#endif", file=output)

        if use_counter_group:
            print("  HistogramDUMMY2,", file=output)
            print("  HistogramLastUseCounter = HistogramDUMMY2 - 1,", file=output)

    print("  HistogramCount,", file=output)
    if seen_use_counters:
        print("  HistogramUseCounterCount = HistogramLastUseCounter - HistogramFirstUseCounter + 1", file=output)
    else:
        print("  HistogramFirstUseCounter = 0,", file=output)
        print("  HistogramLastUseCounter = 0,", file=output)
        print("  HistogramUseCounterCount = 0", file=output)
    print("};", file=output)
    print("} // namespace mozilla", file=output)
    print("} // namespace Telemetry", file=output)
    print("#endif // mozilla_TelemetryHistogramEnums_h", file=output);

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
