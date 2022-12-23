# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out UserInteraction information for C++. The UserInteractions are
# defined in a file provided as a command-line argument.

import sys
from os import path

from mozparsers import parse_user_interactions
from mozparsers.shared_telemetry_utils import ParserError, static_assert

COMPONENTS_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(
    path.join(COMPONENTS_PATH, "glean", "build_scripts", "glean_parser_ext")
)
import sys

from string_table import StringTable

# The banner/text at the top of the generated file.
banner = """/* This file is auto-generated, only for internal use in
   TelemetryUserInteraction.h, see gen_userinteraction_data.py. */
"""

file_header = """\
#ifndef mozilla_TelemetryUserInteractionData_h
#define mozilla_TelemetryUserInteractionData_h
#include "core/UserInteractionInfo.h"
"""

file_footer = """\
#endif // mozilla_TelemetryUserInteractionData_h
"""


def write_user_interaction_table(user_interactions, output, string_table):
    head = """
      namespace mozilla {
      namespace Telemetry {
      namespace UserInteractionID {
        const static uint32_t UserInteractionCount = %d;
      }  // namespace UserInteractionID
      }  // namespace Telemetry
      }  // namespace mozilla
    """

    print(head % len(user_interactions), file=output)

    print("namespace {", file=output)

    table_name = "gUserInteractions"
    print("constexpr UserInteractionInfo %s[] = {" % table_name, file=output)

    for u in user_interactions:
        name_index = string_table.stringIndex(u.label)
        print("  UserInteractionInfo({}),".format(name_index), file=output)
    print("};", file=output)

    static_assert(
        output,
        "sizeof(%s) <= UINT32_MAX" % table_name,
        "index overflow of UserInteractionInfo table %s" % table_name,
    )

    print("}  // namespace", file=output)


def main(output, *filenames):
    # Load the UserInteraction data.
    user_interactions = []
    for filename in filenames:
        try:
            batch = parse_user_interactions.load_user_interactions(filename)
            user_interactions.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
            sys.exit(1)

    # Write the scalar data file.
    print(banner, file=output)
    print(file_header, file=output)

    string_table = StringTable()

    # Write the data for individual UserInteractions.
    write_user_interaction_table(user_interactions, output, string_table)
    print("", file=output)

    # Write the string table.
    string_table_name = "gUserInteractionsStringTable"
    string_table.writeDefinition(output, string_table_name)
    static_assert(
        output, "sizeof(%s) <= UINT32_MAX" % string_table_name, "index overflow"
    )
    print("", file=output)

    print(file_footer, file=output)


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
