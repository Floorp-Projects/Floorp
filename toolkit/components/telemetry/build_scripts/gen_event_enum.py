# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out C++ enum definitions that represent the different event types.
#
# The events are defined in files provided as command-line arguments.

from __future__ import print_function
from mozparsers.shared_telemetry_utils import ParserError
from mozparsers import parse_events

import sys
import buildconfig

banner = """/* This file is auto-generated, see gen_event_enum.py.  */
"""

file_header = """\
#ifndef mozilla_TelemetryEventEnums_h
#define mozilla_TelemetryEventEnums_h
namespace mozilla {
namespace Telemetry {
enum class EventID : uint32_t {\
"""

file_footer = """\
};
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryEventEnums_h
"""


def main(output, *filenames):
    # Load the events first.
    events = []
    for filename in filenames:
        try:
            batch = parse_events.load_events(filename, True)
            events.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
            sys.exit(1)

    grouped = dict()
    index = 0
    for e in events:
        category = e.category
        if category not in grouped:
            grouped[category] = []
        grouped[category].append((index, e))
        index += len(e.enum_labels)

    # Write the enum file.
    print(banner, file=output)
    print(file_header, file=output)

    for category, indexed in grouped.iteritems():
        category_cpp = indexed[0][1].category_cpp

        print("  // category: %s" % category, file=output)

        for event_index, e in indexed:
            if e.record_on_os(buildconfig.substs["OS_TARGET"]):
                for offset, label in enumerate(e.enum_labels):
                    print(" %s_%s = %d,"
                          % (category_cpp, label, event_index + offset), file=output)

    print("  // meta", file=output)
    print("  EventCount = %d," % index, file=output)

    print(file_footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
