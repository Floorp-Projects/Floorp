# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys


def main(output, filename):
    file = open(filename, "r")
    output.write('R"(')  # insert literal start
    for line in file:
        output.write(line)
    output.write(')"')  # insert literal end


if __name__ == "__main__":
    main(sys.stdout, sys.argv[1])
