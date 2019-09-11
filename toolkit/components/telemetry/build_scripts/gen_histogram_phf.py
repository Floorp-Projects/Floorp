# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
from mozparsers.shared_telemetry_utils import ParserError
from perfecthash import PerfectHash

PHFSIZE = 512

from mozparsers import parse_histograms
import sys
import buildconfig


banner = """/* This file is auto-generated, see gen_histogram_phf.py.  */
"""

header = """
#ifndef mozilla_TelemetryHistogramNameMap_h
#define mozilla_TelemetryHistogramNameMap_h

#include "mozilla/PerfectHash.h"

namespace mozilla {
namespace Telemetry {
"""

footer = """
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryHistogramNameMap_h
"""


def main(output, *filenames):
    """
    Generate a Perfect Hash Table for the Histogram name -> Histogram ID lookup.
    The table is immutable once generated and we can avoid any dynamic memory allocation.
    """

    output.write(banner)
    output.write(header)

    try:
        histograms = list(parse_histograms.from_files(filenames))
        histograms = [h for h in histograms if h.record_on_os(buildconfig.substs["OS_TARGET"])]
    except ParserError as ex:
        print("\nError processing histograms:\n" + str(ex) + "\n")
        sys.exit(1)

    histograms = [(bytearray(hist.name(), 'ascii'), idx) for (idx, hist) in enumerate(histograms)]
    name_phf = PerfectHash(histograms, PHFSIZE)

    output.write(name_phf.cxx_codegen(
        name='HistogramIDByNameLookup',
        entry_type="uint32_t",
        lower_entry=lambda x: str(x[1]),
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()"))

    output.write(footer)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
