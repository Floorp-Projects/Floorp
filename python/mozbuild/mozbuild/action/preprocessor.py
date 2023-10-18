# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from mozbuild.preprocessor import Preprocessor


def generate(output, *args):
    pp = Preprocessor()
    pp.out = output
    pp.handleCommandLine(list(args), True)
    return set(pp.includes)


def main(args):
    pp = Preprocessor()
    pp.handleCommandLine(args, True)


if __name__ == "__main__":
    main(sys.argv[1:])
