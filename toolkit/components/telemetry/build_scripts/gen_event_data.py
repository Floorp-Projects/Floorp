# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out event information for C++. The events are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from collections import OrderedDict
from mozparsers.shared_telemetry_utils import (
    StringTable,
    static_assert,
    ParserError
)
from mozparsers import parse_events

import json
import sys
import itertools

# The banner/text at the top of the generated file.
banner = """/* This file is auto-generated, only for internal use in TelemetryEvent.h,
   see gen_event_data.py. */
"""

file_header = """\
#ifndef mozilla_TelemetryEventData_h
#define mozilla_TelemetryEventData_h
#include "core/EventInfo.h"
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

    print("#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const uint32_t %s[] = {" % table_name, file=output)
    print("#else", file=output)
    print("constexpr uint32_t %s[] = {" % table_name, file=output)
    print("#endif", file=output)

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

    print("#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const CommonEventInfo %s[] = {" % table_name, file=output)
    print("#else", file=output)
    print("constexpr CommonEventInfo %s[] = {" % table_name, file=output)
    print("#endif", file=output)

    for e, extras in zip(events, extra_table):
        # Write a comment to make the file human-readable.
        print("  // category: %s" % e.category, file=output)
        print("  // methods: [%s]" % ", ".join(e.methods), file=output)
        print("  // objects: [%s]" % ", ".join(e.objects), file=output)

        # Write the common info structure
        print("  {%d, %d, %d, %d, %s, %s, %s }," %
              (string_table.stringIndex(e.category),
               string_table.stringIndex(e.expiry_version),
               extras[0],  # extra keys index
               extras[1],  # extra keys count
               e.dataset,
               " | ".join(e.record_in_processes_enum),
               " | ".join(e.products_enum)),
              file=output)

    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % table_name,
                  "index overflow")


def write_event_table(events, output, string_table):
    table_name = "gEventInfo"

    print("#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const EventInfo %s[] = {" % table_name, file=output)
    print("#else", file=output)
    print("constexpr EventInfo %s[] = {" % table_name, file=output)
    print("#endif", file=output)

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


def generate_JSON_definitions(output, *filenames):
    """ Write the event definitions to a JSON file.

    :param output: the file to write the content to.
    :param filenames: a list of filenames provided by the build system.
           We only support a single file.
    """
    # Load the event data.
    events = []
    for filename in filenames:
        try:
            batch = parse_events.load_events(filename, True)
            events.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
            sys.exit(1)

    event_definitions = OrderedDict()
    for event in events:
        category = event.category

        if category not in event_definitions:
            event_definitions[category] = OrderedDict()

        event_definitions[category][event.name] = OrderedDict({
            'methods': event.methods,
            'objects': event.objects,
            'extra_keys': event.extra_keys,
            'record_on_release': True if event.dataset_short == 'opt-out' else False,
            # We don't expire dynamic-builtin scalars: they're only meant for
            # use in local developer builds anyway. They will expire when rebuilding.
            'expires': event.expiry_version,
            'expired': False,
            'products': event.products,
        })

    json.dump(event_definitions, output, sort_keys=True)


def main(output, *filenames):
    # Load the event data.
    events = []
    for filename in filenames:
        try:
            batch = parse_events.load_events(filename, True)
            events.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
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
