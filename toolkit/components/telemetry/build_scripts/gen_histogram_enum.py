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
from mozparsers.shared_telemetry_utils import ParserError
from mozparsers import parse_histograms

import itertools
import sys
import buildconfig


banner = """/* This file is auto-generated, see gen_histogram_enum.py.  */
"""

header = """
#ifndef mozilla_TelemetryHistogramEnums_h
#define mozilla_TelemetryHistogramEnums_h

#include "mozilla/TemplateLib.h"

namespace mozilla {
namespace Telemetry {
"""

footer = """
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryHistogramEnums_h"""


def get_histogram_typename(histogram):
    name = histogram.name()
    if name.startswith("USE_COUNTER2_"):
        return "UseCounterWorker" if name.endswith("_WORKER") else "UseCounter"
    return None


def main(output, *filenames):
    # Print header.
    print(banner, file=output)
    print(header, file=output)

    # Load the histograms.
    try:
        all_histograms = list(parse_histograms.from_files(filenames))
    except ParserError as ex:
        print("\nError processing histograms:\n" + str(ex) + "\n")
        sys.exit(1)

    groups = itertools.groupby(all_histograms, get_histogram_typename)

    # Print the histogram enums.
    # Note that parse_histograms.py guarantees that all of the
    # USE_COUNTER2_*_WORKER and USE_COUNTER2_* histograms are both defined in a
    # contiguous block.
    print("enum HistogramID : uint32_t {", file=output)
    seen_group_types = {"UseCounter": False, "UseCounterWorker": False}
    for (group_type, histograms) in groups:
        if group_type is not None:
            assert isinstance(group_type, str)
            assert group_type in seen_group_types.keys()
            assert not seen_group_types[group_type]
            seen_group_types[group_type] = True
            # The Histogram*DUMMY enum variables are used to make the computation
            # of Histogram{First,Last}* easier.  Otherwise, we'd have to special
            # case the first and last histogram in the group.
            print("  HistogramFirst%s," % group_type, file=output)
            print("  Histogram{0}DUMMY1 = HistogramFirst{0} - 1,".format(group_type), file=output)

        for histogram in histograms:
            if histogram.record_on_os(buildconfig.substs["OS_TARGET"]):
                print("  %s," % histogram.name(), file=output)

        if group_type is not None:
            assert isinstance(group_type, str)
            print("  Histogram%sDUMMY2," % group_type, file=output)
            print("  HistogramLast{0} = Histogram{0}DUMMY2 - 1,".format(group_type), file=output)

    print("  HistogramCount,", file=output)

    for (key, value) in sorted(seen_group_types.items()):
        if value:
            print("  Histogram{0}Count = HistogramLast{0} - HistogramFirst{0} + 1,"
                  .format(key), file=output)
        else:
            print("  HistogramFirst%s = 0," % key, file=output)
            print("  HistogramLast%s = 0," % key, file=output)
            print("  Histogram%sCount = 0," % key, file=output)

    print("};", file=output)

    # Write categorical label enums.
    categorical = filter(lambda h: h.kind() == "categorical", all_histograms)
    categorical = filter(lambda h: h.record_on_os(buildconfig.substs["OS_TARGET"]), categorical)
    enums = [("LABELS_" + h.name(), h.labels(), h.name()) for h in categorical]
    for name, labels, _ in enums:
        print("\nenum class %s : uint32_t {" % name, file=output)
        print("  %s" % ",\n  ".join(labels), file=output)
        print("};", file=output)

    print("\ntemplate<class T> struct IsCategoricalLabelEnum : std::false_type {};", file=output)
    for name, _, _ in enums:
        print("template<> struct IsCategoricalLabelEnum<%s> : std::true_type {};" % name,
              file=output)

    print("\ntemplate<class T> struct CategoricalLabelId {};", file=output)
    for name, _, id in enums:
        print("template<> struct CategoricalLabelId<%s> : "
              "std::integral_constant<uint32_t, %s> {};" % (name, id), file=output)

    # Footer.
    print(footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
