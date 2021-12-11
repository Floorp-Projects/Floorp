# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from mozlint import cli

here = os.path.abspath(os.path.dirname(__file__))

if __name__ == "__main__":
    parser = cli.MozlintParser()
    args = vars(parser.parse_args(sys.argv[1:]))
    args["root"] = here
    args["config_paths"] = [os.path.join(here, "linters")]
    sys.exit(cli.run(**args))
