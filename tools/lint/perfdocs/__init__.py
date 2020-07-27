# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from perfdocs import perfdocs

here = os.path.abspath(os.path.dirname(__file__))
PERFDOCS_REQUIREMENTS_PATH = os.path.join(here, "requirements.txt")


def lint(paths, config, logger, fix=False, **lintargs):
    return perfdocs.run_perfdocs(config, logger=logger, paths=paths, generate=fix)
