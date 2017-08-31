# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import errors

from marionette_harness import MarionetteTestCase


class TestProxyCapabilities(MarionetteTestCase):

    def setUp(self):
        super(TestProxyCapabilities, self).setUp()

        self.marionette.delete_session()

    def tearDown(self):
        if not self.marionette.session:
            self.marionette.start_session()

        with self.marionette.using_context("chrome"):
            self.marionette.execute_script("""
                Cu.import("resource://gre/modules/Preferences.jsm");
                Preferences.resetBranch("network.proxy");
            """)

        super(TestProxyCapabilities, self).tearDown()

    def test_proxy_object_none_by_default(self):
        self.marionette.start_session()
        self.assertNotIn("proxy", self.marionette.session_capabilities)

    def test_proxy_object_in_returned_capabilities(self):
        capabilities = {"proxy": {"proxyType": "system"}}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_proxy_type_autodetect(self):
        capabilities = {"proxy": {"proxyType": "autodetect"}}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_proxy_type_direct(self):
        capabilities = {"proxy": {"proxyType": "direct"}}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_proxy_type_manual_without_port(self):
        proxy_hostname = "marionette.test"
        capabilities = {"proxy": {
            "proxyType": "manual",
            "ftpProxy": "{}:21".format(proxy_hostname),
            "httpProxy": "{}:80".format(proxy_hostname),
            "sslProxy": "{}:443".format(proxy_hostname),
            "socksProxy": proxy_hostname,
            "socksVersion": 4,
        }}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_proxy_type_manual_socks_requires_version(self):
        proxy_port = 4444
        proxy_hostname = "marionette.test"
        proxy_host = "{}:{}".format(proxy_hostname, proxy_port)
        capabilities = {"proxy": {
            "proxyType": "manual",
            "socksProxy": proxy_host,
        }}

        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session(capabilities)

    def test_proxy_type_pac(self):
        pac_url = "http://marionette.test"
        capabilities = {"proxy": {"proxyType": "pac", "proxyAutoconfigUrl": pac_url}}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_proxy_type_system(self):
        capabilities = {"proxy": {"proxyType": "system"}}

        self.marionette.start_session(capabilities)
        self.assertEqual(self.marionette.session_capabilities["proxy"],
                         capabilities["proxy"])

    def test_invalid_proxy_object(self):
        capabilities = {"proxy": "I really should be a dictionary"}

        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session(capabilities)

    def test_missing_proxy_type(self):
        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session({"proxy": {"proxyAutoconfigUrl": "foobar"}})

    def test_invalid_proxy_type(self):
        capabilities = {"proxy": {"proxyType": "NOPROXY"}}

        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session(capabilities)

    def test_invalid_autoconfig_url_for_pac(self):
        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session({"proxy": {"proxyType": "pac"}})

        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session({"proxy": {"proxyType": "pac",
                                                     "proxyAutoconfigUrl": None}})

    def test_missing_socks_version_for_manual(self):
        capabilities = {"proxy": {"proxyType": "manual", "socksProxy": "marionette.test"}}

        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette.start_session(capabilities)
