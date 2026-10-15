# SPDX-License-Identifier: MPL-2.0
"""Host faults for the actual final-package HTTP fixture and its fixed JS script."""

import http.client
import importlib.util
import json
from pathlib import Path
import subprocess
import threading
import unittest
import urllib.request
from urllib.parse import urlsplit

from final_web_panel_http import (
    BODY, EchoServer, MAX_REQUESTS, RedirectCase,
    validate_redirect_trace, validate_ua_values,
)

MOBILE = "Independent host fixture Mobile UA"
DESKTOP = "Independent host fixture Desktop UA"
CASE = "web-panel-ua.redirect.mobile-on"


def case():
    return RedirectCase("a" * 32, CASE, "http://127.0.0.1:2829", MOBILE)


def trace(value):
    return [{"nonce": value.nonce, "caseId": value.case_id, "hop": hop,
             "url": value.url(hop), "host": "127.0.0.1:2829", "method": "GET",
             "status": 302 if hop < 2 else 200,
             "location": value.url(hop + 1) if hop < 2 else None,
             "userAgent": value.expected_ua} for hop in range(3)]


class RedirectOracleTests(unittest.TestCase):
    def test_exact_three_hops_pass_with_independent_expected_ua(self):
        value = case()
        validate_redirect_trace(value, trace(value))

    def test_wrong_ua_on_each_hop_fails(self):
        for hop in range(3):
            with self.subTest(hop=hop):
                value = case()
                events = trace(value)
                events[hop]["userAgent"] = DESKTOP
                with self.assertRaisesRegex(RuntimeError, "Wrong HTTP UA"):
                    validate_redirect_trace(value, events)

    def test_missing_duplicate_and_reordered_hops_fail(self):
        value = case()
        events = trace(value)
        for changed in [events[:2], events + [events[0]], events[::-1],
                        [events[0], events[0], events[2]]]:
            with self.subTest(events=changed):
                with self.assertRaises(RuntimeError):
                    validate_redirect_trace(value, changed)

    def test_foreign_nonce_case_and_noninteger_hop_fail(self):
        for field, replacement in [("nonce", "b" * 32), ("caseId", "other-case"),
                                   ("hop", False), ("status", True), ("method", "POST")]:
            with self.subTest(field=field):
                value = case()
                events = trace(value)
                events[0][field] = replacement
                with self.assertRaises(RuntimeError):
                    validate_redirect_trace(value, events)

    def test_non_loopback_or_wrong_destination_and_location_fail(self):
        for field, replacement in [("url", "https://example.invalid/"),
                                   ("url", "http://127.0.0.1:2830/wrong"),
                                   ("host", "example.invalid"),
                                   ("location", "https://example.invalid/"),
                                   ("location", None)]:
            with self.subTest(field=field, replacement=replacement):
                value = case()
                events = trace(value)
                events[0][field] = replacement
                with self.assertRaises(RuntimeError):
                    validate_redirect_trace(value, events)

    def test_missing_and_extra_fields_fail(self):
        for modify in (lambda event: event.pop("userAgent"),
                       lambda event: event.update({"unexpected": "field"})):
            events = trace(case())
            modify(events[0])
            with self.assertRaisesRegex(RuntimeError, "Malformed"):
                validate_redirect_trace(case(), events)

    def test_ordinary_tab_or_other_panel_mobile_ua_leakage_fails_same_oracle(self):
        for label in ("normal-after-mobile-redirect", "other-after-mobile-redirect",
                      "normal-after-desktop-redirect", "other-after-desktop-redirect"):
            with self.subTest(label=label):
                validate_ua_values([DESKTOP], DESKTOP, label)
                with self.assertRaisesRegex(RuntimeError, "Wrong HTTP UA"):
                    validate_ua_values([MOBILE], DESKTOP, label)

    def test_empty_and_unbounded_ua_or_trace_fail(self):
        for values in ([], [MOBILE] * (MAX_REQUESTS + 1)):
            with self.assertRaises(RuntimeError):
                validate_ua_values(values, MOBILE, CASE)
        with self.assertRaises(RuntimeError):
            validate_ua_values(["x" * 4097], "x" * 4097, CASE)
        with self.assertRaises(RuntimeError):
            validate_redirect_trace(case(), trace(case()) * 1000)

    def test_case_refuses_external_urls_credentials_fragments_and_unknown_ids(self):
        for origin in ("https://127.0.0.1:2829", "http://localhost:2829",
                       "http://127.0.0.1:2829/", "http://127.0.0.1:2829#x",
                       "http://user@127.0.0.1:2829", "http://example.invalid:2829"):
            with self.subTest(origin=origin):
                with self.assertRaises((RuntimeError, ValueError)):
                    RedirectCase("a" * 32, CASE, origin, MOBILE)
        with self.assertRaisesRegex(RuntimeError, "Unknown"):
            RedirectCase("a" * 32, "arbitrary-url", "http://127.0.0.1:2829", MOBILE)


class ActualHttpFixtureTests(unittest.TestCase):
    def setUp(self):
        self.server = EchoServer()

    def tearDown(self):
        self.server.server_close()

    def test_actual_loopback_http_redirects_and_server_observations(self):
        server = self.server
        value = server.register_redirect(CASE, MOBILE)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            with opener.open(urllib.request.Request(value.url(0), headers={"User-Agent": MOBILE}), timeout=3) as response:
                self.assertEqual(response.status, 200)
                self.assertEqual(response.geturl(), value.url(2))
                self.assertEqual(response.read(), BODY)
            validate_redirect_trace(value, server.redirect_trace(CASE))
            server.assert_healthy()
            observed = server.redirect_trace(CASE)
            observed[0]["userAgent"] = "Mutation cannot affect stored observation"
            server.validate_registered_redirects()
        finally:
            server.shutdown()
            thread.join(timeout=5)
        self.assertFalse(thread.is_alive())

    def test_direct_requests_still_use_actual_server_headers(self):
        server = self.server
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            conn = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=3)
            try:
                conn.request("GET", "/ua?normal", headers={"User-Agent": DESKTOP})
                response = conn.getresponse()
                self.assertEqual(response.status, 200)
                self.assertEqual(response.read(), BODY)
            finally:
                conn.close()
            validate_ua_values(server.requests("normal"), DESKTOP, "normal")
        finally:
            server.shutdown()
            thread.join(timeout=5)
        self.assertFalse(thread.is_alive())

    def test_observation_count_is_bounded_and_first_error_cannot_be_replaced(self):
        server = self.server
        host = urlsplit(server.origin).netloc
        self.assertEqual(server.observe("/ua?normal", DESKTOP, "wrong-host")[0], 400)
        original = server.first_error
        for _ in range(MAX_REQUESTS + 10):
            server.observe("/ua?normal", DESKTOP, host)
        self.assertEqual(server.total_requests, MAX_REQUESTS)
        self.assertEqual(server.first_error, original)
        self.assertLessEqual(len(server.requests("normal")), MAX_REQUESTS)
        with self.assertRaisesRegex(RuntimeError, original):
            server.assert_healthy()

    def test_registered_cases_cannot_replay_and_foreign_nonce_cannot_enter_trace(self):
        server = self.server
        value = server.register_redirect(CASE, MOBILE)
        with self.assertRaisesRegex(RuntimeError, "replayed"):
            server.register_redirect(CASE, MOBILE)
        host = urlsplit(server.origin).netloc
        foreign = urlsplit(value.url(0)).path.replace(value.nonce, "0" * 32)
        self.assertEqual(server.observe(foreign, MOBILE, host)[0], 400)
        self.assertEqual(server.redirect_trace(CASE), [])
        with self.assertRaisesRegex(RuntimeError, "exactly three"):
            server.validate_registered_redirects()

    def test_final_revalidation_catches_a_duplicate_after_early_success(self):
        server = self.server
        value = server.register_redirect(CASE, MOBILE)
        host = urlsplit(server.origin).netloc
        for hop in range(3):
            server.observe(urlsplit(value.url(hop)).path, MOBILE, host)
        server.validate_registered_redirects()
        server.observe(urlsplit(value.url(2)).path, MOBILE, host)
        with self.assertRaisesRegex(RuntimeError, "exactly three"):
            server.validate_registered_redirects()

    def test_final_revalidation_catches_late_ua_leakage_without_changing_expectation(self):
        server = self.server
        host = urlsplit(server.origin).netloc
        server.expect_direct("normal", DESKTOP)
        server.observe("/ua?normal", DESKTOP, host)
        server.validate_registered_requests()
        server.observe("/ua?normal", MOBILE, host)
        with self.assertRaisesRegex(RuntimeError, "Wrong HTTP UA"):
            server.validate_registered_requests()
        with self.assertRaisesRegex(RuntimeError, "cannot rewrite"):
            server.expect_direct("normal", MOBILE)

    def test_reused_panel_url_retains_mobile_off_on_expectation_boundaries(self):
        server = self.server
        host = urlsplit(server.origin).netloc
        for index, expected in enumerate((MOBILE, DESKTOP, MOBILE)):
            server.observe("/ua?initial-mobile", expected, host)
            server.expect_direct("initial-mobile", expected, index)
        server.validate_registered_requests()
        self.assertEqual(server.requests("initial-mobile"), [MOBILE, DESKTOP, MOBILE])
        server.received["initial-mobile"][0] = DESKTOP
        with self.assertRaisesRegex(RuntimeError, "Wrong HTTP UA"):
            server.validate_registered_requests()
        with self.assertRaisesRegex(RuntimeError, "cannot rewrite"):
            server.expect_direct("initial-mobile", DESKTOP, 0)

    def test_direct_expectation_refuses_uncovered_prefix_or_invalid_boundary(self):
        server = self.server
        host = urlsplit(server.origin).netloc
        server.observe("/ua?normal", DESKTOP, host)
        for start in (True, -1, 2):
            with self.subTest(start=start), self.assertRaises(RuntimeError):
                server.expect_direct("normal", DESKTOP, start)
        with self.assertRaisesRegex(RuntimeError, "first request"):
            server.expect_direct("normal", DESKTOP, 1)


class ActualNavigationScriptTests(unittest.TestCase):
    def test_actual_fixed_script_selects_the_inner_panel_and_system_principal(self):
        probe_file = Path(__file__).with_name("test-final-web-panel-ua.py")
        spec = importlib.util.spec_from_file_location("web_panel_ua_probe", probe_file)
        probe = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(probe)
        script = probe.PANEL_NAVIGATION_SCRIPT
        javascript = """
const input=JSON.parse(process.argv[1]);
const seen=[];
const principal={kind:'host-fixture-principal'};
globalThis.Services={io:{newURI(url){seen.push(['newURI',url]);return {uri:url};}},
  scriptSecurityManager:{getSystemPrincipal(){return principal;}}};
globalThis.window={wrappedJSObject:{__floorpFinalUAProbe:{main:{document:{
  getElementById(id){seen.push(['element',id]);return {browsingContext:{associatedWindow:{
    floorpWebPanelContentBrowser:{loadURI(uri,options){
      if(options.triggeringPrincipal!==principal)throw Error('different principal');
      seen.push(['loadURI',uri.uri]);
    }}
  }}};}
}}}}};
new Function(input.script)(input.identifier,input.url);
process.stdout.write(JSON.stringify(seen));
"""
        url = case().url(0)
        completed = subprocess.run(["node", "-e", javascript, json.dumps({
            "script": script, "identifier": "final-ua-mobile", "url": url,
        })], check=True, capture_output=True, text=True, timeout=5)
        self.assertEqual(json.loads(completed.stdout), [
            ["element", "sidebar-panel-final-ua-mobile"], ["newURI", url], ["loadURI", url],
        ])


if __name__ == "__main__":
    unittest.main()
