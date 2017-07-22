# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from marionette_harness import BaseMarionetteTestRunner
from testcase import TelemetryTestCase


class TelemetryTestRunner(BaseMarionetteTestRunner):

    def __init__(self, **kwargs):
        super(TelemetryTestRunner, self).__init__(**kwargs)
        self.server_root = kwargs.pop('server_root')
        self.testvars['server_root'] = self.server_root

        # select the appropriate GeckoInstance
        self.app = 'fxdesktop'

        self.test_handlers = [TelemetryTestCase]
