# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.geckoinstance import apps, GeckoInstance

from marionette_harness import MarionetteTestCase


class TestGeckoInstance(MarionetteTestCase):

    def test_create(self):
        """Test that the correct gecko instance is determined."""
        for app in apps:
            # If app has been specified we directly return the appropriate instance class
            self.assertEqual(type(GeckoInstance.create(app=app, bin="n/a")),
                             apps[app])

        # Unknown applications and binaries should fail
        self.assertRaises(NotImplementedError, GeckoInstance.create,
                          app="n/a", bin=self.marionette.bin)
        self.assertRaises(NotImplementedError, GeckoInstance.create,
                          bin="n/a")
        self.assertRaises(NotImplementedError, GeckoInstance.create,
                          bin=None)
