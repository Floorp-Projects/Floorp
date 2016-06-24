# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out a C++ enum definition whose members are the names of
# scalar types.
#
# The scalars are defined in files provided as command-line arguments.

from __future__ import print_function

import sys
import parse_scalars

banner = """/* This file is auto-generated, see gen-scalar-enum.py.  */
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
    if len(filenames) > 1:
        raise Exception('We don\'t support loading from more than one file.')
    scalars = parse_scalars.load_scalars(filenames[0])

    # Write the enum file.
    print(banner, file=output)
    print(file_header, file=output);

    for s in scalars:
        cpp_guard = s.cpp_guard
        if cpp_guard:
            print("#if defined(%s)" % cpp_guard, file=output)
        print("  %s," % s.enum_label, file=output)
        if cpp_guard:
            print("#endif", file=output)

    print("  ScalarCount,", file=output)

    print(file_footer, file=output)

if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
