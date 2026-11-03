#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check that opt-in proxy setup preserves bypasses and keeps credentials private."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("private_profile_network", Path(__file__).with_name("private-profile-network.py"))
NETWORK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NETWORK)


class PrivateProfileNetwork(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.bundle = Path(self.directory.name) / "ca.pem"
        self.bundle.write_text("-----BEGIN CERTIFICATE-----\nYWJj\n-----END CERTIFICATE-----\n")

    def environment(self, **values):
        return {"SSL_CERT_FILE": str(self.bundle), **values}

    def test_proxy_and_no_proxy_are_native_profile_preferences(self):
        prefs, certs, report = NETWORK.environment_network(self.environment(
            HTTPS_PROXY="http://proxy.example:8080", HTTP_PROXY="http://proxy.example:8080",
            NO_PROXY=".example.test,10.0.0.0/8"))
        self.assertEqual(prefs["network.proxy.ssl"], "proxy.example")
        self.assertEqual(prefs["network.proxy.http_port"], 8080)
        self.assertEqual(prefs["network.proxy.no_proxies_on"], ".example.test,10.0.0.0/8,localhost,127.0.0.0/8,::1")
        self.assertEqual(certs, ["YWJj"])
        self.assertFalse(report["certificateValidationDisabled"])
        self.assertFalse(report["hostTrustModified"])
        self.assertNotIn("proxy.example", str(report))

    def test_credentials_are_rejected_without_being_echoed(self):
        with self.assertRaises(ValueError) as result:
            NETWORK.environment_network(self.environment(HTTPS_PROXY="http://private-user:private-secret@proxy.example:8080"))
        self.assertNotIn("private-secret", str(result.exception))
        self.assertNotIn("private-user", str(result.exception))

    def test_unsupported_proxy_transport_is_explicit(self):
        with self.assertRaisesRegex(ValueError, "HTTP proxy URL"):
            NETWORK.environment_network(self.environment(HTTPS_PROXY="socks5://proxy.example:1080"))

    def test_lowercase_environment_and_wildcard_bypass(self):
        prefs, _, _ = NETWORK.environment_network(self.environment(http_proxy="http://lower.example", HTTP_PROXY="http://upper.example", no_proxy="*"))
        self.assertEqual(prefs["network.proxy.http"], "lower.example")
        self.assertEqual(prefs["network.proxy.http_port"], 80)
        self.assertTrue(prefs["network.proxy.no_proxies_on"].startswith("*,"))

    def test_no_environment_proxy_remains_direct(self):
        prefs, _, _ = NETWORK.environment_network(self.environment())
        self.assertEqual(prefs["network.proxy.type"], 0)

    def test_invalid_ca_bundle_is_rejected(self):
        self.bundle.write_text("not a certificate bundle")
        with self.assertRaisesRegex(ValueError, "no PEM certificates"):
            NETWORK.environment_network(self.environment())


if __name__ == "__main__":
    unittest.main()
