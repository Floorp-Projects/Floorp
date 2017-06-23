# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import SessionNotCreatedException

from marionette_harness import MarionetteTestCase

import os.path


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
                current_profile = os.path.normcase(self.marionette.instance.runner.profile.profile)
            self.assertEqual(os.path.normcase(str(self.caps["moz:profile"])), current_profile)
            self.assertEqual(os.path.normcase(str(self.marionette.profile)), current_profile)

        self.assertIn("moz:accessibilityChecks", self.caps)
        self.assertFalse(self.caps["moz:accessibilityChecks"])
        self.assertIn("specificationLevel", self.caps)
        self.assertEqual(self.caps["specificationLevel"], 0)

    def test_set_specification_level(self):
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"specificationLevel": 2}})
        caps = self.marionette.session_capabilities
        self.assertEqual(2, caps["specificationLevel"])

        self.marionette.delete_session()
        self.marionette.start_session({"requiredCapabilities": {"specificationLevel": 3}})
        caps = self.marionette.session_capabilities
        self.assertEqual(3, caps["specificationLevel"])

    def test_we_can_pass_in_required_capabilities_on_session_start(self):
        self.marionette.delete_session()
        capabilities = {"requiredCapabilities": {"browserName": self.appinfo["name"].lower()}}
        self.marionette.start_session(capabilities)
        caps = self.marionette.session_capabilities
        self.assertIn("browserName", caps)

        # Start a new session just to make sure we leave the browser in the
        # same state it was before it started the test
        self.marionette.start_session()

    def test_capability_types(self):
        for value in ["", "invalid", True, 42, []]:
            print("testing value {}".format(value))
            with self.assertRaises(SessionNotCreatedException):
                print("  with desiredCapabilities")
                self.marionette.delete_session()
                self.marionette.start_session({"desiredCapabilities": value})
            with self.assertRaises(SessionNotCreatedException):
                print("  with requiredCapabilities")
                self.marionette.delete_session()
                self.marionette.start_session({"requiredCapabilities": value})

    def test_we_get_valid_uuid4_when_creating_a_session(self):
        self.assertNotIn("{", self.marionette.session_id,
                         "Session ID has {{}} in it: {}".format(
                             self.marionette.session_id))


class TestCapabilityMatching(MarionetteTestCase):
    allowed = [None, "*"]
    disallowed = ["", 42, True, {}, []]

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.browser_name = self.marionette.session_capabilities["browserName"]
        self.platform_name = self.marionette.session_capabilities["platformName"]
        self.delete_session()

    def delete_session(self):
        if self.marionette.session is not None:
            self.marionette.delete_session()

    def test_browser_name_desired(self):
        self.marionette.start_session({"desiredCapabilities": {"browserName": self.browser_name}})
        self.assertEqual(self.marionette.session_capabilities["browserName"], self.browser_name)

    def test_browser_name_required(self):
        self.marionette.start_session({"requiredCapabilities": {"browserName": self.browser_name}})
        self.assertEqual(self.marionette.session_capabilities["browserName"], self.browser_name)

    def test_browser_name_desired_allowed_types(self):
        for typ in self.allowed:
            self.delete_session()
            self.marionette.start_session({"desiredCapabilities": {"browserName": typ}})
            self.assertEqual(self.marionette.session_capabilities["browserName"], self.browser_name)

    def test_browser_name_desired_disallowed_types(self):
        for typ in self.disallowed:
            with self.assertRaises(SessionNotCreatedException):
                self.marionette.start_session({"desiredCapabilities": {"browserName": typ}})

    def test_browser_name_required_allowed_types(self):
        for typ in self.allowed:
            self.delete_session()
            self.marionette.start_session({"requiredCapabilities": {"browserName": typ}})
            self.assertEqual(self.marionette.session_capabilities["browserName"], self.browser_name)

    def test_browser_name_requried_disallowed_types(self):
        for typ in self.disallowed:
            with self.assertRaises(SessionNotCreatedException):
                self.marionette.start_session({"requiredCapabilities": {"browserName": typ}})

    def test_browser_name_prefers_required(self):
        caps = {"desiredCapabilities": {"browserName": "invalid"},
                    "requiredCapabilities": {"browserName": "*"}}
        self.marionette.start_session(caps)

    def test_browser_name_error_on_invalid_required(self):
        with self.assertRaises(SessionNotCreatedException):
            caps = {"desiredCapabilities": {"browserName": "*"},
                        "requiredCapabilities": {"browserName": "invalid"}}
            self.marionette.start_session(caps)

    # TODO(ato): browser version comparison not implemented yet

    def test_platform_name_desired(self):
        self.marionette.start_session({"desiredCapabilities": {"platformName": self.platform_name}})
        self.assertEqual(self.marionette.session_capabilities["platformName"], self.platform_name)

    def test_platform_name_required(self):
        self.marionette.start_session({"requiredCapabilities": {"platformName": self.platform_name}})
        self.assertEqual(self.marionette.session_capabilities["platformName"], self.platform_name)

    def test_platform_name_desired_allowed_types(self):
        for typ in self.allowed:
            self.delete_session()
            self.marionette.start_session({"desiredCapabilities": {"platformName": typ}})
            self.assertEqual(self.marionette.session_capabilities["platformName"], self.platform_name)

    def test_platform_name_desired_disallowed_types(self):
        for typ in self.disallowed:
            with self.assertRaises(SessionNotCreatedException):
                self.marionette.start_session({"desiredCapabilities": {"platformName": typ}})

    def test_platform_name_required_allowed_types(self):
        for typ in self.allowed:
            self.delete_session()
            self.marionette.start_session({"requiredCapabilities": {"platformName": typ}})
            self.assertEqual(self.marionette.session_capabilities["platformName"], self.platform_name)

    def test_platform_name_requried_disallowed_types(self):
        for typ in self.disallowed:
            with self.assertRaises(SessionNotCreatedException):
                self.marionette.start_session({"requiredCapabilities": {"platformName": typ}})

    def test_platform_name_prefers_required(self):
        caps = {"desiredCapabilities": {"platformName": "invalid"},
                    "requiredCapabilities": {"platformName": "*"}}
        self.marionette.start_session(caps)

    def test_platform_name_error_on_invalid_required(self):
        with self.assertRaises(SessionNotCreatedException):
            caps = {"desiredCapabilities": {"platformName": "*"},
                        "requiredCapabilities": {"platformName": "invalid"}}
            self.marionette.start_session(caps)

    # TODO(ato): platform version comparison not imlpemented yet

    def test_accept_insecure_certs(self):
        for capability_type in ["desiredCapabilities", "requiredCapabilities"]:
            print("testing {}".format(capability_type))
            for value in ["", 42, {}, []]:
                print("  type {}".format(type(value)))
                with self.assertRaises(SessionNotCreatedException):
                    self.marionette.start_session({capability_type: {"acceptInsecureCerts": value}})

        self.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"acceptInsecureCerts": True}})
        self.assertTrue(self.marionette.session_capabilities["acceptInsecureCerts"])
        self.delete_session()
        self.marionette.start_session({"requiredCapabilities": {"acceptInsecureCerts": True}})

        self.assertTrue(self.marionette.session_capabilities["acceptInsecureCerts"])

    def test_page_load_strategy(self):
        for strategy in ["none", "eager", "normal"]:
            print("valid strategy {}".format(strategy))
            self.delete_session()
            self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": strategy}})
            self.assertEqual(self.marionette.session_capabilities["pageLoadStrategy"], strategy)

        # A null value should be treatend as "normal"
        self.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": None}})
        self.assertEqual(self.marionette.session_capabilities["pageLoadStrategy"], "normal")

        for value in ["", "EAGER", True, 42, {}, []]:
            print("invalid strategy {}".format(value))
            with self.assertRaisesRegexp(SessionNotCreatedException, "InvalidArgumentError"):
                self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": value}})

    def test_proxy_default(self):
        self.marionette.start_session()
        self.assertNotIn("proxy", self.marionette.session_capabilities)

    def test_proxy_desired(self):
        self.marionette.start_session({"desiredCapabilities": {"proxy": {"proxyType": "manual"}}})
        self.assertIn("proxy", self.marionette.session_capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"]["proxyType"], "manual")
        self.assertEqual(self.marionette.get_pref("network.proxy.type"), 1)

    def test_proxy_required(self):
        self.marionette.start_session({"requiredCapabilities": {"proxy": {"proxyType": "manual"}}})
        self.assertIn("proxy", self.marionette.session_capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"]["proxyType"], "manual")
        self.assertEqual(self.marionette.get_pref("network.proxy.type"), 1)

    def test_timeouts(self):
        timeouts = {u"implicit": 123, u"pageLoad": 456, u"script": 789}
        caps = {"desiredCapabilities": {"timeouts": timeouts}}
        self.marionette.start_session(caps)
        self.assertIn("timeouts", self.marionette.session_capabilities)
        self.assertDictEqual(self.marionette.session_capabilities["timeouts"], timeouts)
        self.assertDictEqual(self.marionette._send_message("getTimeouts"), timeouts)
