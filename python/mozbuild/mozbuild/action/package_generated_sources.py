# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import os.path
import sys

import buildconfig
from mozpack.archive import create_tar_gz_from_files
from mozbuild.generated_sources import get_generated_sources


def main(argv):
    parser = argparse.ArgumentParser(
        description='Produce archive of generated sources')
    parser.add_argument('outputfile', help='File to write output to')
    args = parser.parse_args(argv)


    files = dict(get_generated_sources())
    with open(args.outputfile, 'wb') as fh:
        create_tar_gz_from_files(fh, files, compresslevel=5)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
