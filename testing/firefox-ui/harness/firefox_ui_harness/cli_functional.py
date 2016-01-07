#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette.runtests import cli as mn_cli

from firefox_ui_harness.arguments import FirefoxUIArguments
from firefox_ui_harness.runners import FirefoxUITestRunner


def cli():
    mn_cli(runner_class=FirefoxUITestRunner,
           parser_class=FirefoxUIArguments,
           )


if __name__ == '__main__':
    cli()
