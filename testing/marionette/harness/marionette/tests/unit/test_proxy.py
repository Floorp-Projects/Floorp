# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import InvalidArgumentException

class TestProxy(MarionetteTestCase):

    def setUp(self):
        super(TestProxy, self).setUp()
        self.marionette.delete_session()

    def test_that_we_can_set_a_autodetect_proxy(self):
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "autodetect",
                                }
                            }
                        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                }
            """)

        self.assertEqual(result["proxyType"], 4)

    def test_that_capabilities_returned_have_proxy_details(self):
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "autodetect",
                                    }
                            }
                        }
        self.marionette.start_session(capabilities)
        result = self.marionette.session_capabilities

        self.assertEqual(result["proxy"]["proxyType"], "autodetect")

    def test_that_we_can_set_a_system_proxy(self):
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "system",
                                    }
                            }
                       }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                }
            """)

        self.assertEqual(result["proxyType"], 5)

    def test_we_can_set_a_pac_proxy(self):
        url = "http://marionette.test"
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "pac",
                                    "proxyAutoconfigUrl": url,
                                }
                            }
                        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                "proxyAutoconfigUrl" : Services.prefs.getCharPref('network.proxy.autoconfig_url'),
                }
            """)

        self.assertEqual(result["proxyType"], 2)
        self.assertEqual(result["proxyAutoconfigUrl"], url, 'proxyAutoconfigUrl was not set')

    def test_that_we_can_set_a_manual_proxy(self):
        port = 4444
        url = "http://marionette.test"
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "manual",
                                    "ftpProxy": url,
                                    "ftpProxyPort": port,
                                    "httpProxy": url,
                                    "httpProxyPort": port,
                                    "sslProxy": url,
                                    "sslProxyPort": port,
                                }
                            }
                        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                "httpProxy" : Services.prefs.getCharPref('network.proxy.http'),
                "httpProxyPort": Services.prefs.getIntPref('network.proxy.http_port'),
                "sslProxy": Services.prefs.getCharPref('network.proxy.ssl'),
                "sslProxyPort": Services.prefs.getIntPref('network.proxy.ssl_port'),
                "ftpProxy": Services.prefs.getCharPref('network.proxy.ftp'),
                "ftpProxyPort": Services.prefs.getIntPref('network.proxy.ftp_port'),
                }
            """)

        self.assertEqual(result["proxyType"], 1)
        self.assertEqual(result["httpProxy"], url, 'httpProxy was not set')
        self.assertEqual(result["httpProxyPort"], port, 'httpProxyPort was not set')
        self.assertEqual(result["sslProxy"], url, 'sslProxy url was not set')
        self.assertEqual(result["sslProxyPort"], port, 'sslProxyPort was not set')
        self.assertEqual(result["ftpProxy"], url, 'ftpProxy was not set')
        self.assertEqual(result["ftpProxyPort"], port, 'ftpProxyPort was not set')

    def test_we_can_set_a_manual_proxy_with_a_socks_proxy_with_socks_version(self):
        port = 4444
        url = "http://marionette.test"
        capabilities = {"requiredCapabilities":
                            {
                                "proxy":{
                                    "proxyType": "manual",
                                    "socksProxy": url,
                                    "socksProxyPort": port,
                                    "socksVersion": 4,
                                    "socksUsername": "cake",
                                    "socksPassword": "made with cake"
                                }
                            }
                        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                "socksProxy" : Services.prefs.getCharPref('network.proxy.socks'),
                "socksProxyPort": Services.prefs.getIntPref('network.proxy.socks_port'),
                "socksVersion": Services.prefs.getIntPref('network.proxy.socks_version'),
                }
            """)
        self.assertEqual(result["socksProxy"], url, 'socksProxy was not set')
        self.assertEqual(result["socksProxyPort"], port, 'socksProxyPort was not set')
        self.assertEqual(result["socksVersion"], 4, 'socksVersion was not set to 4')

    def test_we_can_set_a_manual_proxy_with_a_socks_proxy_with_no_socks_version(self):
        port = 4444
        url = "http://marionette.test"
        capabilities = {"requiredCapabilities":
                                    {
                                    "proxy":{
                                        "proxyType": "manual",
                                        "socksProxy": url,
                                        "socksProxyPort": port,
                                        "socksUsername": "cake",
                                        "socksPassword": "made with cake"
                                     }
                                    }
        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                "socksProxy" : Services.prefs.getCharPref('network.proxy.socks'),
                "socksProxyPort": Services.prefs.getIntPref('network.proxy.socks_port'),
                "socksVersion": Services.prefs.getIntPref('network.proxy.socks_version'),

                }
            """)
        self.assertEqual(result["socksProxy"], url, 'socksProxy was not set')
        self.assertEqual(result["socksProxyPort"], port, 'socksProxyPort was not set')
        self.assertEqual(result["socksVersion"], 5, 'socksVersion was not set to 5')

    def test_when_not_all_manual_proxy_details_are_in_capabilities(self):
        port = 4444
        url = "http://marionette.test"
        capabilities = {"requiredCapabilities":
                                    {
                                    "proxy":{
                                        "proxyType": "manual",
                                        "ftpProxy": url,
                                        "ftpProxyPort": port,
                                     }
                                    }
        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType" : Services.prefs.getIntPref('network.proxy.type'),
                "httpProxy" : Services.prefs.getCharPref('network.proxy.http'),
                "httpProxyPort": Services.prefs.getIntPref('network.proxy.http_port'),
                "sslProxy": Services.prefs.getCharPref('network.proxy.ssl'),
                "sslProxyPort": Services.prefs.getIntPref('network.proxy.ssl_port'),
                "ftpProxy": Services.prefs.getCharPref('network.proxy.ftp'),
                "ftpProxyPort": Services.prefs.getIntPref('network.proxy.ftp_port'),
                }
            """)

        self.assertEqual(result["proxyType"], 1)
        self.assertNotEqual(result["httpProxy"], url,
                            'httpProxy was set. %s' % result["httpProxy"])
        self.assertNotEqual(result["httpProxyPort"], port, 'httpProxyPort was set')
        self.assertNotEqual(result["sslProxy"], url, 'sslProxy url was set')
        self.assertNotEqual(result["sslProxyPort"], port, 'sslProxyPort was set')
        self.assertEqual(result["ftpProxy"], url, 'ftpProxy was set')
        self.assertEqual(result["ftpProxyPort"], port, 'ftpProxyPort was set')



    def test_proxy_is_a_string_should_throw_invalid_argument(self):
        capabilities = {"requiredCapabilities":
                                    {
                                    "proxy":"I really should be a dictionary"
                                    }
                            }
        try:
            self.marionette.start_session(capabilities)
            self.fail("We should have started a session because proxy should be a dict")
        except InvalidArgumentException as e:
            assert e.message == "Value of 'proxy' should be an object"

    def test_proxy_is_passed_in_with_no_proxy_doesnt_set_it(self):
        capabilities = {"requiredCapabilities":
            {
                "proxy": {"proxyType": "NOPROXY"},
            }
        }
        self.marionette.start_session(capabilities)
        result = None
        with self.marionette.using_context('chrome'):
            result = self.marionette.execute_script("""return {
                "proxyType": Services.prefs.getIntPref('network.proxy.type'),
                };
            """)

        self.assertEqual(result["proxyType"], 0)

    def tearDown(self):
        if not self.marionette.session:
            self.marionette.start_session()
        else:
            self.marionette.restart(clean=True)
