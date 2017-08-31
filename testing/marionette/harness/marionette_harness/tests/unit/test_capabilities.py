# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import SessionNotCreatedException

from marionette_harness import MarionetteTestCase

# Unlike python 3, python 2 doesn't have a proper implementation of realpath or
# samefile for Windows. However this function, which does exactly what we want,
# was added to python 2 to fix an issue with tcl installations and symlinks.
from FixTk import convert_path


class TestCapabilities(MarionetteTestCase):

    def setUp(self):
        super(TestCapabilities, self).setUp()
        self.caps = self.marionette.session_capabilities
        with self.marionette.using_context("chrome"):
            self.appinfo = self.marionette.execute_script("""
                return {
                  name: Services.appinfo.name,
                  version: Services.appinfo.version,
                  processID: Services.appinfo.processID,
                }
                """)
            self.os_name = self.marionette.execute_script(
                "return Services.sysinfo.getProperty('name')").lower()
            self.os_version = self.marionette.execute_script(
                "return Services.sysinfo.getProperty('version')")

    def test_mandated_capabilities(self):
        self.assertIn("browserName", self.caps)
        self.assertIn("browserVersion", self.caps)
        self.assertIn("platformName", self.caps)
        self.assertIn("platformVersion", self.caps)
        self.assertIn("acceptInsecureCerts", self.caps)
        self.assertIn("timeouts", self.caps)

        self.assertEqual(self.caps["browserName"], self.appinfo["name"].lower())
        self.assertEqual(self.caps["browserVersion"], self.appinfo["version"])
        self.assertEqual(self.caps["platformName"], self.os_name)
        self.assertEqual(self.caps["platformVersion"], self.os_version)
        self.assertFalse(self.caps["acceptInsecureCerts"])
        self.assertDictEqual(self.caps["timeouts"],
                             {"implicit": 0,
                              "pageLoad": 300000,
                              "script": 30000})

    def test_supported_features(self):
        self.assertIn("rotatable", self.caps)

    def test_additional_capabilities(self):
        self.assertIn("moz:processID", self.caps)
        self.assertEqual(self.caps["moz:processID"], self.appinfo["processID"])
        self.assertEqual(self.marionette.process_id, self.appinfo["processID"])

        self.assertIn("moz:profile", self.caps)
        if self.marionette.instance is not None:
            if self.caps["browserName"] == "fennec":
                current_profile = self.marionette.instance.runner.device.app_ctx.remote_profile
            else:
                current_profile = convert_path(self.marionette.instance.runner.profile.profile)
            self.assertEqual(convert_path(str(self.caps["moz:profile"])), current_profile)
            self.assertEqual(convert_path(str(self.marionette.profile)), current_profile)

        self.assertIn("moz:accessibilityChecks", self.caps)
        self.assertFalse(self.caps["moz:accessibilityChecks"])
        self.assertIn("specificationLevel", self.caps)
        self.assertEqual(self.caps["specificationLevel"], 0)

    def test_set_specification_level(self):
        self.marionette.delete_session()
        self.marionette.start_session({"specificationLevel": 2})
        caps = self.marionette.session_capabilities
        self.assertEqual(2, caps["specificationLevel"])

    def test_we_get_valid_uuid4_when_creating_a_session(self):
        self.assertNotIn("{", self.marionette.session_id,
                         "Session ID has {{}} in it: {}".format(
                             self.marionette.session_id))


class TestCapabilityMatching(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.delete_session()

    def delete_session(self):
        if self.marionette.session is not None:
            self.marionette.delete_session()

    def test_accept_insecure_certs(self):
        for value in ["", 42, {}, []]:
            print("  type {}".format(type(value)))
            with self.assertRaises(SessionNotCreatedException):
                self.marionette.start_session({"acceptInsecureCerts": value})

        self.delete_session()
        self.marionette.start_session({"acceptInsecureCerts": True})
        self.assertTrue(self.marionette.session_capabilities["acceptInsecureCerts"])

    def test_page_load_strategy(self):
        for strategy in ["none", "eager", "normal"]:
            print("valid strategy {}".format(strategy))
            self.delete_session()
            self.marionette.start_session({"pageLoadStrategy": strategy})
            self.assertEqual(self.marionette.session_capabilities["pageLoadStrategy"], strategy)

        # A null value should be treatend as "normal"
        self.delete_session()
        self.marionette.start_session({"pageLoadStrategy": None})
        self.assertEqual(self.marionette.session_capabilities["pageLoadStrategy"], "normal")

        for value in ["", "EAGER", True, 42, {}, []]:
            print("invalid strategy {}".format(value))
            with self.assertRaisesRegexp(SessionNotCreatedException, "InvalidArgumentError"):
                self.marionette.start_session({"pageLoadStrategy": value})

    def test_timeouts(self):
        timeouts = {u"implicit": 123, u"pageLoad": 456, u"script": 789}
        caps = {"timeouts": timeouts}
        self.marionette.start_session(caps)
        self.assertIn("timeouts", self.marionette.session_capabilities)
        self.assertDictEqual(self.marionette.session_capabilities["timeouts"], timeouts)
        self.assertDictEqual(self.marionette._send_message("getTimeouts"), timeouts)
