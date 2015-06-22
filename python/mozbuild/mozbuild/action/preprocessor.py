# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys

from mozbuild.preprocessor import Preprocessor


def main(args):
  pp = Preprocessor()
  pp.handleCommandLine(args, True)


if __name__ == "__main__":
  main(sys.argv[1:])
