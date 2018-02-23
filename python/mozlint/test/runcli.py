# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys

here = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(here), 'mozlint'))

from mozlint import cli
cli.SEARCH_PATHS.append(os.path.join(here, 'linters'))

if __name__ == '__main__':
    parser = cli.MozlintParser()
    args = vars(parser.parse_args(sys.argv[1:]))
    args['root'] = here
    sys.exit(cli.run(**args))
