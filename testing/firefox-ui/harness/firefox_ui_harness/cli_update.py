#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette.runtests import cli as mn_cli

from firefox_ui_harness.arguments import UpdateArguments
from firefox_ui_harness.runners import UpdateTestRunner


def cli():
    mn_cli(runner_class=UpdateTestRunner,
           parser_class=UpdateArguments,
           )


if __name__ == '__main__':
    cli()
