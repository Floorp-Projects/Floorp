# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out processes data for C++. The processes are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from mozparsers.shared_telemetry_utils import ParserError, load_yaml_file

import sys
import collections

# The banner/text at the top of the generated file.
banner = """/* This file is auto-generated from Telemetry build scripts,
   see gen_process_enum.py. */
"""

file_header = """\
#ifndef mozilla_TelemetryProcessEnums_h
#define mozilla_TelemetryProcessEnums_h

namespace mozilla {
namespace Telemetry {
"""

file_footer = """
} // namespace Telemetry
} // namespace mozilla
#endif // mozilla_TelemetryProcessEnums_h"""


def to_enum_label(name):
    return name.title().replace('_', '')


def write_processes_enum(processes, output):
    def p(line):
        print(line, file=output)
    processes = collections.OrderedDict(processes)

    p("enum class ProcessID : uint32_t {")
    for i, (name, _) in enumerate(sorted(processes.items())):
        p("  %s = %d," % (to_enum_label(name), i))
    p("  Count = %d" % len(processes))
    p("};")


def main(output, *filenames):
    if len(filenames) > 1:
        raise Exception('We don\'t support loading from more than one file.')

    try:
        processes = load_yaml_file(filenames[0])

        # Write the process data file.
        print(banner, file=output)
        print(file_header, file=output)
        write_processes_enum(processes, output)
        print(file_footer, file=output)
    except ParserError as ex:
        print("\nError generating processes enums:\n" + str(ex) + "\n")
        sys.exit(1)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
