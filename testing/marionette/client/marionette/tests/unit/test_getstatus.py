# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class TestGetStatus(MarionetteTestCase):
    def test_getStatus(self):
        status = self.marionette.status()
        self.assertIn("os", status)
        status_os = status['os']
        self.assertIn("version", status_os)
        self.assertIn("name", status_os)
        self.assertIn("arch", status_os)
        self.assertIn("build", status)
        status_build = status['build']
        self.assertIn("revision", status_build)
        self.assertIn("time", status_build)
        self.assertIn("version", status_build)
