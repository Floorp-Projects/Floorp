# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out a C++ enum definition whose members are the names of
# scalar types.
#
# The scalars are defined in files provided as command-line arguments.

from __future__ import print_function
from mozparsers.shared_telemetry_utils import ParserError
from mozparsers import parse_scalars

import sys
import buildconfig

banner = """/* This file is auto-generated, see gen_scalar_enum.py.  */
"""

file_header = """\
#ifndef mozilla_TelemetryScalarEnums_h
#define mozilla_TelemetryScalarEnums_h
namespace mozilla {
namespace Telemetry {
enum class ScalarID : uint32_t {\
"""

file_footer = """\
};
} // namespace mozilla
} // namespace Telemetry
#endif // mozilla_TelemetryScalarEnums_h
"""


def main(output, *filenames):
    # Load the scalars first.
    scalars = []
    for filename in filenames:
        try:
            batch = parse_scalars.load_scalars(filename)
            scalars.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
            sys.exit(1)

    # Write the enum file.
    print(banner, file=output)
    print(file_header, file=output)

    for s in scalars:
        if s.record_on_os(buildconfig.substs["OS_TARGET"]):
            print("  %s," % s.enum_label, file=output)

    print("  ScalarCount,", file=output)

    print(file_footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
