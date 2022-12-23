# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozparsers.shared_telemetry_utils import ParserError
from perfecthash import PerfectHash

PHFSIZE = 1024

import sys

from mozparsers import parse_user_interactions

banner = """/* This file is auto-generated, see gen_userinteraction_phf.py.  */
"""

header = """
#ifndef mozilla_TelemetryUserInteractionNameMap_h
#define mozilla_TelemetryUserInteractionNameMap_h

#include "mozilla/PerfectHash.h"

namespace mozilla {
namespace Telemetry {
"""

footer = """
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryUserInteractionNameMap_h
"""


def main(output, *filenames):
    """
    Generate a Perfect Hash Table for the UserInteraction name -> UserInteraction ID lookup.
    The table is immutable once generated and we can avoid any dynamic memory allocation.
    """

    output.write(banner)
    output.write(header)

    try:
        user_interactions = list(parse_user_interactions.from_files(filenames))
    except ParserError as ex:
        print("\nError processing UserInteractions:\n" + str(ex) + "\n")
        sys.exit(1)

    user_interactions = [
        (bytearray(ui.label, "ascii"), idx)
        for (idx, ui) in enumerate(user_interactions)
    ]
    name_phf = PerfectHash(user_interactions, PHFSIZE)

    output.write(
        name_phf.cxx_codegen(
            name="UserInteractionIDByNameLookup",
            entry_type="uint32_t",
            lower_entry=lambda x: str(x[1]),
            key_type="const nsACString&",
            key_bytes="aKey.BeginReading()",
            key_length="aKey.Length()",
        )
    )

    output.write(footer)


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
