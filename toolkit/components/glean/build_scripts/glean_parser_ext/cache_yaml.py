# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pickle
import sys

import buildconfig
from run_glean_parser import parse


def main(out_fd, *args):
    interesting_yamls = buildconfig.substs.get("MOZ_GLEAN_INTERESTING_METRICS_FILES")
    parse_result = parse(args, interesting_yamls)
    pickle.dump(parse_result, out_fd)


if __name__ == "__main__":
    main(*sys.argv[1:])
