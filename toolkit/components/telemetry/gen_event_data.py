# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out event information for C++. The events are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from shared_telemetry_utils import StringTable, static_assert, ParserError

import parse_events
import sys
import itertools

# The banner/text at the top of the generated file.
banner = """/* This file is auto-generated, only for internal use in TelemetryEvent.h,
   see gen_event_data.py. */
"""

file_header = """\
#ifndef mozilla_TelemetryEventData_h
#define mozilla_TelemetryEventData_h
#include "EventInfo.h"
namespace {
"""

file_footer = """\
} // namespace
#endif // mozilla_TelemetryEventData_h
"""


def write_extra_table(events, output, string_table):
    table_name = "gExtraKeysTable"
    extra_table = []
    extra_count = 0

    print("const uint32_t %s[] = {" % table_name, file=output)

    for e in events:
        extra_index = 0
        extra_keys = e.extra_keys
        if len(extra_keys) > 0:
            extra_index = extra_count
            extra_count += len(extra_keys)
            indexes = string_table.stringIndexes(extra_keys)

            print("  // %s, [%s], [%s]" % (
                  e.category,
                  ", ".join(e.methods),
                  ", ".join(e.objects)),
                  file=output)
            print("  // extra_keys: %s" % ", ".join(extra_keys), file=output)
            print("  %s," % ", ".join(map(str, indexes)),
                  file=output)

        extra_table.append((extra_index, len(extra_keys)))

    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % table_name,
                  "index overflow")

    return extra_table


def write_common_event_table(events, output, string_table, extra_table):
    table_name = "gCommonEventInfo"

    print("const CommonEventInfo %s[] = {" % table_name, file=output)
    for e, extras in zip(events, extra_table):
        # Write a comment to make the file human-readable.
        print("  // category: %s" % e.category, file=output)
        print("  // methods: [%s]" % ", ".join(e.methods), file=output)
        print("  // objects: [%s]" % ", ".join(e.objects), file=output)

        # Write the common info structure
        print("  {%d, %d, %d, %d, %s, %s}," %
              (string_table.stringIndex(e.category),
               string_table.stringIndex(e.expiry_version),
               extras[0],  # extra keys index
               extras[1],  # extra keys count
               e.dataset,
               " | ".join(e.record_in_processes_enum)),
              file=output)

    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % table_name,
                  "index overflow")


def write_event_table(events, output, string_table):
    table_name = "gEventInfo"
    print("const EventInfo %s[] = {" % table_name, file=output)

    for common_info_index, e in enumerate(events):
        for method_name, object_name in itertools.product(e.methods, e.objects):
            print("  // category: %s, method: %s, object: %s" %
                  (e.category, method_name, object_name),
                  file=output)

            print("  {gCommonEventInfo[%d], %d, %d}," %
                  (common_info_index,
                   string_table.stringIndex(method_name),
                   string_table.stringIndex(object_name)),
                  file=output)

    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % table_name,
                  "index overflow")


def main(output, *filenames):
    # Load the event data.
    if len(filenames) > 1:
        raise Exception('We don\'t support loading from more than one file.')
    try:
        events = parse_events.load_events(filenames[0])
    except ParserError as ex:
        print("\nError processing events:\n" + str(ex) + "\n")
        sys.exit(1)

    # Write the scalar data file.
    print(banner, file=output)
    print(file_header, file=output)

    # Write the extra keys table.
    string_table = StringTable()
    extra_table = write_extra_table(events, output, string_table)
    print("", file=output)

    # Write a table with the common event data.
    write_common_event_table(events, output, string_table, extra_table)
    print("", file=output)

    # Write the data for individual events.
    write_event_table(events, output, string_table)
    print("", file=output)

    # Write the string table.
    string_table_name = "gEventsStringTable"
    string_table.writeDefinition(output, string_table_name)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % string_table_name,
                  "index overflow")
    print("", file=output)

    print(file_footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
