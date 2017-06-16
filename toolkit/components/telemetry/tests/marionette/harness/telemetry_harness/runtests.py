# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness.runtests import cli as mn_cli
from testcase import TelemetryTestCase


def cli():
    mn_cli(testcase_class=TelemetryTestCase)


if __name__ == '__main__':
    cli()
