# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from perfdocs import perfdocs
from mozlint.util import pip

here = os.path.abspath(os.path.dirname(__file__))
PERFDOCS_REQUIREMENTS_PATH = os.path.join(here, 'requirements.txt')


def setup(root, **lintargs):
    if not pip.reinstall_program(PERFDOCS_REQUIREMENTS_PATH):
        print("Cannot install requirements.")
        return 1


def lint(paths, config, logger, fix=None, **lintargs):
    return perfdocs.run_perfdocs(
        config, logger=logger, paths=paths, verify=True
    )
