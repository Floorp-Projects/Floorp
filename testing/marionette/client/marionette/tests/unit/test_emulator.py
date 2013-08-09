# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import JavascriptException, MarionetteException


class TestEmulatorContent(MarionetteTestCase):

    def test_emulator_cmd(self):
        self.marionette.set_script_timeout(10000)
        expected = ["<build>",
                    "OK"]
        result = self.marionette.execute_async_script("""
        runEmulatorCmd("avd name", marionetteScriptFinished)
        """);
        self.assertEqual(result, expected)

    def test_emulator_order(self):
        self.marionette.set_script_timeout(10000)
        self.assertRaises(MarionetteException,
                          self.marionette.execute_async_script,
        """runEmulatorCmd("gsm status", function(result) {});
           marionetteScriptFinished(true);
        """);


class TestEmulatorChrome(TestEmulatorContent):

    def setUp(self):
        super(TestEmulatorChrome, self).setUp()
        self.marionette.set_context("chrome")


class TestEmulatorScreen(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.screen = self.marionette.emulator.screen
        self.screen.initialize()

    def test_emulator_orientation(self):
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_PRIMARY,
                         'Orientation has been correctly initialized.')

        self.screen.orientation = self.screen.SO_PORTRAIT_SECONDARY
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_SECONDARY,
                         'Orientation has been set to portrait-secondary')

        self.screen.orientation = self.screen.SO_LANDSCAPE_PRIMARY
        self.assertEqual(self.screen.orientation, self.screen.SO_LANDSCAPE_PRIMARY,
                         'Orientation has been set to landscape-primary')

        self.screen.orientation = self.screen.SO_LANDSCAPE_SECONDARY
        self.assertEqual(self.screen.orientation, self.screen.SO_LANDSCAPE_SECONDARY,
                         'Orientation has been set to landscape-secondary')

        self.screen.orientation = self.screen.SO_PORTRAIT_PRIMARY
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_PRIMARY,
                         'Orientation has been set to portrait-primary')
