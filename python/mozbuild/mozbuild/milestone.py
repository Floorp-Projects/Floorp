# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import re
import sys


def get_milestone_ab_with_num(milestone):
    """
    Returns the alpha and beta tag with its number (a1, a2, b3, ...).
    """

    match = re.search(r"([ab]\d+)", milestone)
    if match:
        return match.group(1)

    return ""


def get_official_milestone(path):
    """
    Returns the contents of the first line in `path` that starts with a digit.
    """

    with open(path) as fp:
        for line in fp:
            line = line.strip()
            if line[:1].isdigit():
                return line

    raise Exception("Didn't find a line that starts with a digit.")


def get_milestone_major(milestone):
    """
    Returns the major (first) part of the milestone.
    """

    return milestone.split('.')[0]


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--uaversion', default=False, action='store_true')
    parser.add_argument('--symbolversion', default=False, action='store_true')
    parser.add_argument('--topsrcdir', metavar='TOPSRCDIR', required=True)
    options = parser.parse_args(args)

    milestone_file = os.path.join(options.topsrcdir, 'config', 'milestone.txt')

    milestone = get_official_milestone(milestone_file)

    if options.uaversion:
        # Only expose the major milestone in the UA string, hide the patch
        # level (bugs 572659 and 870868).
        uaversion = "%s.0" % (get_milestone_major(milestone),)
        print(uaversion)

    elif options.symbolversion:
        # Only expose major milestone and alpha version. Used for symbol
        # versioning on Linux.
        symbolversion = "%s%s" % (get_milestone_major(milestone),
                                  get_milestone_ab_with_num(milestone))
        print(symbolversion)
    else:
        print(milestone)


if __name__ == '__main__':
    main(sys.argv[1:])
