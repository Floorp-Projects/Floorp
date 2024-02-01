# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out a C++ enum definition whose members are the names of
# histograms as well as the following other members:
#
#   - HistogramCount
#
# The histograms are defined in files provided as command-line arguments.

import sys

import buildconfig
from mozparsers import parse_histograms
from mozparsers.shared_telemetry_utils import ParserError

banner = """/* This file is auto-generated, see gen_histogram_enum.py.  */
"""

header = """
#ifndef mozilla_TelemetryHistogramEnums_h
#define mozilla_TelemetryHistogramEnums_h

#include <cstdint>
#include <type_traits>

namespace mozilla {
namespace Telemetry {
"""

footer = """
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryHistogramEnums_h"""


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

    # Print the histogram enums.
    print("enum HistogramID : uint32_t {", file=output)
    for histogram in all_histograms:
        if histogram.record_on_os(buildconfig.substs["OS_TARGET"]):
            print("  %s," % histogram.name(), file=output)

    print("  HistogramCount,", file=output)

    print("};", file=output)

    # Write categorical label enums.
    categorical = filter(lambda h: h.kind() == "categorical", all_histograms)
    categorical = filter(
        lambda h: h.record_on_os(buildconfig.substs["OS_TARGET"]), categorical
    )
    enums = [("LABELS_" + h.name(), h.labels(), h.name()) for h in categorical]
    for name, labels, _ in enums:
        print("\nenum class %s : uint32_t {" % name, file=output)
        print("  %s" % ",\n  ".join(labels), file=output)
        print("};", file=output)

    print(
        "\ntemplate<class T> struct IsCategoricalLabelEnum : std::false_type {};",
        file=output,
    )
    for name, _, _ in enums:
        print(
            "template<> struct IsCategoricalLabelEnum<%s> : std::true_type {};" % name,
            file=output,
        )

    print("\ntemplate<class T> struct CategoricalLabelId {};", file=output)
    for name, _, id in enums:
        print(
            "template<> struct CategoricalLabelId<%s> : "
            "std::integral_constant<uint32_t, %s> {};" % (name, id),
            file=output,
        )

    # Footer.
    print(footer, file=output)


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
