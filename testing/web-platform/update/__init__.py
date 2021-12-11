#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
import sys

from mozlog import structuredlog

here = os.path.split(__file__)[0]

imp.load_source(
    "localpaths", os.path.join(here, os.pardir, "tests", "tools", "localpaths.py")
)

from wptrunner.update import setup_logging, WPTUpdate
from wptrunner.update.base import exit_unclean

from . import updatecommandline
from .update import UpdateRunner


def run_update(logger, **kwargs):
    updater = WPTUpdate(logger, runner_cls=UpdateRunner, **kwargs)
    return updater.run()


if __name__ == "__main__":
    args = updatecommandline.parse_args()
    logger = setup_logging(args, {"mach": sys.stdout})
    assert structuredlog.get_default_logger() is not None

    rv = run_update(logger, **args)
    if rv is exit_unclean:
        sys.exit(1)
    else:
        sys.exit(0)
