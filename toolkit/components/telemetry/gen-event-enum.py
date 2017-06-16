# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out C++ enum definitions that represent the different event types.
#
# The events are defined in files provided as command-line arguments.

from __future__ import print_function
from shared_telemetry_utils import ParserError

import sys
import parse_events

banner = """/* This file is auto-generated, see gen-event-enum.py.  */
"""

file_header = """\
#ifndef mozilla_TelemetryEventEnums_h
#define mozilla_TelemetryEventEnums_h
namespace mozilla {
namespace Telemetry {
namespace EventID {
"""

file_footer = """\
} // namespace EventID
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryEventEnums_h
"""


def main(output, *filenames):
    # Load the events first.
    if len(filenames) > 1:
        raise Exception('We don\'t support loading from more than one file.')

    try:
        events = parse_events.load_events(filenames[0])
    except ParserError as ex:
        print("\nError processing events:\n" + str(ex) + "\n")
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

        print("// category: %s" % category, file=output)
        print("enum class %s : uint32_t {" % category_cpp, file=output)

        for event_index, e in indexed:
            cpp_guard = e.cpp_guard
            if cpp_guard:
                print("#if defined(%s)" % cpp_guard, file=output)
            for offset, label in enumerate(e.enum_labels):
                print("  %s = %d," % (label, event_index + offset), file=output)
            if cpp_guard:
                print("#endif", file=output)

        print("};\n", file=output)

    print("const uint32_t EventCount = %d;\n" % index, file=output)

    print(file_footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
