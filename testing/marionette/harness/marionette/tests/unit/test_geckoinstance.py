# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette.marionette_test import MarionetteTestCase
from marionette_driver.geckoinstance import apps, GeckoInstance


class TestGeckoInstance(MarionetteTestCase):

    def test_create(self):
        """Test that the correct gecko instance is determined."""
        for app in apps:
            # If app has been specified we directly return the appropriate instance class
            self.assertEqual(type(GeckoInstance.create(app=app, bin="n/a")),
                             apps[app])

        # When using mozversion to detect the instance class, it should never return the
        # base GeckoInstance class.
        self.assertNotEqual(GeckoInstance.create(bin=self.marionette.bin),
                            GeckoInstance)

        # Unknown applications and binaries should fail
        self.assertRaises(NotImplementedError, GeckoInstance.create,
                          app="n/a", binary=self.marionette.bin)
        self.assertRaises(IOError, GeckoInstance.create,
                          bin="n/a")
