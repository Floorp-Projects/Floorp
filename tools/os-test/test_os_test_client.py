# SPDX-License-Identifier: MPL-2.0

"""Unit tests for the shared Python OS-Server test client and its integration.

Synthetic loopback fixtures and temp token files only; no external network,
real browser, service or secret. Run from tools/os-test:
    python -m unittest test_os_test_client.py -v
"""

from __future__ import annotations

import http.server
import importlib
import json
import os
import tempfile
import threading
import unittest
import urllib.parse
from unittest.mock import patch

import os_test_client as client
import run_verify_os_server_full_wrapper as wrapper
import verify_os_server_full as verifier

SECRET = "synthetic-test-secret-12345"
BASE = "http://127.0.0.1:5175"


def encoded(text: str) -> bytes:
    return text.encode("utf-8")


def write_token_file(text: str) -> str:
    handle, path = tempfile.mkstemp(prefix="floorp-os-token-", suffix=".txt")
    with os.fdopen(handle, "wb") as stream:
        stream.write(encoded(text))
    return path


class FakeResponse:
    def __init__(self, status: int, body: bytes, url: str) -> None:
        self.status = status
        self._body = body
        self._url = url

    def read(self) -> bytes:
        return self._body

    def geturl(self) -> str:
        return self._url

    def __enter__(self) -> "FakeResponse":
        return self

    def __exit__(self, *args) -> bool:
        return False


class FakeOpener:
    def __init__(self, status: int = 200, body: str = "", error: Exception | None = None) -> None:
        self.status = status
        self.body = body
        self.error = error
        self.calls: list[tuple[object, float]] = []

    def open(self, request, timeout=None):
        self.calls.append((request, timeout))
        if self.error is not None:
            raise self.error
        return FakeResponse(
            self.status,
            self.body.encode("utf-8"),
            request.full_url,
        )


class FixtureServer:
    """Small same-origin HTTP fixture with bearer and redirect behavior."""

    def __init__(
        self,
        *,
        require_bearer: bool = False,
        health_redirect: bool = False,
    ) -> None:
        seen: list[tuple[str, str | None]] = []

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, format, *args):  # noqa: A003
                return

            def do_GET(self):  # noqa: N802
                parsed = urllib.parse.urlsplit(self.path)
                auth = self.headers.get("Authorization")
                seen.append((parsed.path, auth))
                if health_redirect:
                    self.send_response(302)
                    self.send_header("Location", "/health")
                    self.end_headers()
                    return
                if parsed.path == "/health":
                    if require_bearer and auth != "Bearer " + SECRET:
                        payload = b'{"status":"unauthorized"}'
                        self.send_response(401)
                    else:
                        payload = b'{"status":"ok"}'
                        self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(payload)))
                    self.end_headers()
                    self.wfile.write(payload)
                    return
                if parsed.path == "/redirect":
                    query = urllib.parse.parse_qs(parsed.query)
                    status = int(query.get("status", ["302"])[0])
                    location = query.get("location", ["/health"])[0]
                    self.send_response(status)
                    self.send_header("Location", location)
                    self.end_headers()
                    return
                self.send_response(404)
                self.end_headers()

        self.seen = seen
        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.httpd.server_address[1]
        self.base = f"http://127.0.0.1:{self.port}"
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()

    def close(self) -> None:
        self.httpd.shutdown()
        self.httpd.server_close()


class TokenFileTests(unittest.TestCase):
    def test_explicit_token_file_precedence_and_no_discovery(self):
        self.assertEqual(client.resolve_token_file("explicit", "environment"), "explicit")
        self.assertEqual(client.resolve_token_file(None, "environment"), "environment")
        self.assertIsNone(client.resolve_token_file())
        with self.assertRaises(client.OsTestConfigError):
            client.resolve_token_file("", "fallback")
        with self.assertRaises(client.OsTestConfigError) as raised:
            client.resolve_token_file("   ", "fallback")
        self.assertIn(client.TOKEN_FILE_EMPTY_ERROR, str(raised.exception))

    def test_authenticated_base_url_literal_loopback_before_file_read(self):
        unsafe = [
            "https://127.0.0.1:5175",
            "http://localhost:5175",
            "http://example.com",
            "http://127.1:5175",
            "http://2130706433:5175",
            "http://127.0.0.1.evil:5175",
            "http://user@127.0.0.1:5175",
            "http://127.0.0.1:65536",
            "http://127.0.0.1:5175/path",
            "http://127.0.0.1:5175/?x=1",
            "http://127.0.0.1:5175/#x",
            "file:///tmp/token",
            "http://127.0.0.1:5175/../",
            " http://127.0.0.1:5175",
            "http://127.0.0.1:5175/\x00",
        ]
        for base_url in unsafe:
            reads = []
            opener = FakeOpener()

            def reader(path, limit, reads=reads):
                reads.append((path, limit))
                return encoded(SECRET)

            with self.assertRaises(client.OsTestConfigError):
                client.create_os_test_client(
                    base_url,
                    "synthetic",
                    read_token_file=reader,
                    opener=opener,
                )
            self.assertEqual(reads, [], base_url)
        self.assertEqual(
            client.validate_authenticated_base_url(BASE + "/"),
            BASE,
        )
        self.assertEqual(
            client.validate_authenticated_base_url("http://[::1]:5175"),
            "http://[::1]:5175",
        )

    def test_bounded_strict_token_with_single_optional_newline(self):
        invalid_texts = [
            "",
            " ",
            SECRET + " ",
            " " + SECRET,
            SECRET + "\n\n",
            SECRET + "\t",
            SECRET + "\nother",
            "a=b",
            "é",
            "a" * 4097,
        ]
        for text in invalid_texts:
            with self.assertRaises(client.OsTestConfigError) as raised:
                client.create_os_test_client(
                    BASE,
                    "synthetic",
                    read_token_file=lambda _p, limit, text=text: (
                        self.assertEqual(limit, 4096) or encoded(text)
                    ),
                )
            self.assertEqual(str(raised.exception), client.TOKEN_LOAD_ERROR)

        with self.assertRaises(client.OsTestConfigError) as raised:
            client.create_os_test_client(
                BASE,
                "synthetic",
                read_token_file=lambda _p, _l: (_ for _ in ()).throw(
                    RuntimeError("Bearer " + SECRET)
                ),
            )
        self.assertEqual(str(raised.exception), client.TOKEN_LOAD_ERROR)
        self.assertNotIn(SECRET, str(raised.exception))

        with self.assertRaises(client.OsTestConfigError):
            client.create_os_test_client(
                BASE,
                "synthetic",
                read_token_file=lambda _p, _l: bytes([255]),
            )

        for ending in ("", "\n", "\r\n"):
            opener = FakeOpener(body="ok")
            client_instance = client.create_os_test_client(
                BASE,
                "synthetic",
                read_token_file=lambda _p, _l, ending=ending: encoded(SECRET + ending),
                opener=opener,
            )
            self.assertEqual(client_instance.request("/health"), (200, "ok"))
            request, _timeout = opener.calls[0]
            self.assertEqual(
                request.get_header("Authorization"),
                "Bearer " + SECRET,
            )
            self.assertNotIn(SECRET, repr(client_instance))
            self.assertFalse(hasattr(client_instance, "token"))

    def test_caller_supplied_authorization_is_removed(self):
        opener = FakeOpener(body="ok")
        client_instance = client.create_os_test_client(
            BASE,
            "synthetic",
            read_token_file=lambda _p, _l: encoded(SECRET),
            opener=opener,
        )
        client_instance.request(
            "/health",
            headers={"authorization": "forbidden", "X-Test": "kept"},
        )
        request, _timeout = opener.calls[0]
        self.assertEqual(request.get_header("Authorization"), "Bearer " + SECRET)
        self.assertEqual(request.get_header("X-test"), "kept")


class RequestBoundaryTests(unittest.TestCase):
    def test_origin_checked_before_authorization_on_malicious_paths(self):
        opener = FakeOpener(body="ok")
        client_instance = client.create_os_test_client(
            BASE,
            "synthetic",
            read_token_file=lambda _p, _l: encoded(SECRET),
            opener=opener,
        )
        for path in (
            "@example.com/health",
            "0/health",
            "@127.0.0.1:58261/health",
        ):
            with self.assertRaises(client.OsTestClientError) as raised:
                client_instance.request(path)
            self.assertEqual(str(raised.exception), client.REQUEST_ERROR)
        self.assertEqual(opener.calls, [])
        client_instance.request("/health")
        self.assertEqual(len(opener.calls), 1)

    def test_every_redirect_refused_even_same_origin(self):
        server = FixtureServer()
        token_path = write_token_file(SECRET)
        try:
            client_instance = client.create_os_test_client(server.base, token_path)
            for status in (301, 302, 303, 307, 308):
                for location in ("/health", "http://127.0.0.1:58261/health"):
                    before = len(server.seen)
                    path = f"/redirect?status={status}&location={urllib.parse.quote(location, safe='')}"
                    with self.assertRaises(client.OsTestClientError) as raised:
                        client_instance.request(path)
                    self.assertEqual(str(raised.exception), client.REQUEST_ERROR)
                    self.assertEqual(len(server.seen), before + 1)
        finally:
            server.close()
            os.remove(token_path)

    def test_ambient_proxy_is_bypassed(self):
        server = FixtureServer(require_bearer=True)
        token_path = write_token_file(SECRET)
        try:
            with patch.dict(
                os.environ,
                {"http_proxy": "http://127.0.0.1:9", "HTTP_PROXY": "http://127.0.0.1:9"},
                clear=False,
            ):
                client_instance = client.create_os_test_client(server.base, token_path)
                self.assertEqual(client_instance.request("/health")[0], 200)
            self.assertEqual(server.seen[0][1], "Bearer " + SECRET)
        finally:
            server.close()
            os.remove(token_path)

    def test_response_redaction_plain_json_keys_and_escapes(self):
        bodies = [
            SECRET,
            json.dumps({SECRET: [SECRET]}),
            '{"echo":"' + SECRET.replace("s", "\\u0073") + '"}',
        ]
        for body in bodies:
            opener = FakeOpener(status=401, body=body)
            client_instance = client.create_os_test_client(
                BASE,
                "synthetic",
                read_token_file=lambda _p, _l: encoded(SECRET),
                opener=opener,
            )
            status, result = client_instance.request("/health")
            self.assertEqual(status, 401)
            serialized = json.dumps(result, default=str)
            self.assertNotIn(SECRET, serialized)
            self.assertIn(client.REDACTED, serialized)

    def test_bounded_errors_carry_no_url_token_or_headers(self):
        opener = FakeOpener(error=RuntimeError("Bearer " + SECRET + " at http://127.0.0.1:5175"))
        client_instance = client.create_os_test_client(
            BASE,
            "synthetic",
            read_token_file=lambda _p, _l: encoded(SECRET),
            opener=opener,
        )
        with self.assertRaises(client.OsTestClientError) as raised:
            client_instance.request("/health")
        message = str(raised.exception)
        self.assertEqual(message, client.REQUEST_ERROR)
        self.assertNotIn(SECRET, message)
        self.assertNotIn("http://", message)

    def test_invalid_utf8_response_is_bounded_error(self):
        opener = FakeOpener(body="")
        opener.body = ""
        client_instance = client.create_os_test_client(
            BASE,
            "synthetic",
            read_token_file=lambda _p, _l: encoded(SECRET),
            opener=opener,
        )
        opener.status = 200
        with patch.object(FakeResponse, "read", return_value=b"\xff"):
            with self.assertRaises(client.OsTestClientError) as raised:
                client_instance.request("/health")
        self.assertEqual(str(raised.exception), client.REQUEST_ERROR)

    def test_stream_returns_marker_without_reading(self):
        opener = FakeOpener(body=SECRET)
        client_instance = client.create_os_test_client(
            BASE,
            "synthetic",
            read_token_file=lambda _p, _l: encoded(SECRET),
            opener=opener,
        )
        self.assertEqual(client_instance.request("/events", stream=True), (200, "<stream>"))


class CliTests(unittest.TestCase):
    def test_both_scopes_reconciled_and_duplicates_rejected(self):
        resolved = client.reconcile_os_test_client_args(
            ["--skip-start", "--base-url", BASE],
            ["--quick", "--token-file", "synthetic"],
        )
        self.assertEqual(resolved.wrapper_args, ["--skip-start"])
        self.assertEqual(resolved.verifier_args, ["--quick"])
        self.assertEqual(resolved.base_url_flag, BASE)
        self.assertEqual(resolved.token_file_flag, "synthetic")

        for left, right in (
            ([ "--base-url", "one"], ["--base-url=two"]),
            ([ "--base-url", "one"], ["--base-url=one"]),
            ([], ["--base-url"]),
            (["--base-url="], []),
            (["--base-url", "--quick"], []),
            ([], ["--token-file"]),
            (["--token-file", "one"], ["--token-file=one"]),
        ):
            with self.assertRaises(client.OsTestConfigError):
                client.reconcile_os_test_client_args(left, right)

        for raw in ("--token=" + SECRET, "--auth-token", "--authorization"):
            with self.assertRaises(client.OsTestConfigError) as raised:
                client.reconcile_os_test_client_args([], [raw])
            self.assertEqual(str(raised.exception), client.RAW_TOKEN_ERROR)
            self.assertNotIn(SECRET, str(raised.exception))

    def test_redacted_parser_disables_abbreviation_and_values(self):
        parser = client.RedactedArgumentParser()
        parser.add_argument("--base-url")
        self.assertEqual(
            parser.parse_args(["--base-url", BASE]).base_url,
            BASE,
        )
        for argv in (
            ["--bas", BASE],
            ["--unknown=" + SECRET],
            ["--base-url", "--quick"],
        ):
            with self.assertRaises(client.OsTestConfigError) as raised:
                parser.parse_args(argv)
            self.assertEqual(str(raised.exception), client.CLI_ERROR)
            self.assertNotIn(SECRET, str(raised.exception))
        with self.assertRaises(SystemExit) as raised:
            parser.parse_args(["--help"])
        self.assertEqual(raised.exception.code, 0)


class HelpEffectTests(unittest.TestCase):
    @staticmethod
    def _forbid_floorp_env_reads():
        real_get = os.environ.get

        def guarded_get(key, default=None):
            if key.startswith("FLOORP_OS_"):
                raise AssertionError("FLOORP env read")
            return real_get(key, default)

        return patch.object(os.environ, "get", side_effect=guarded_get)

    def test_wrapper_help_has_no_env_file_network_or_spawn(self):
        with self._forbid_floorp_env_reads(), patch.dict(
            os.environ,
            {
                "FLOORP_OS_BASE_URL": "http://example.com",
                "FLOORP_OS_TOKEN_FILE": "missing-synthetic-token",
            },
            clear=False,
        ):
            with patch.object(wrapper.subprocess, "Popen") as popen, patch.object(
                wrapper.time, "sleep"
            ) as sleep, patch.object(wrapper, "run_verifier") as run:
                with self.assertRaises(SystemExit) as raised:
                    wrapper.main(["--help"])
        self.assertEqual(raised.exception.code, 0)
        popen.assert_not_called()
        sleep.assert_not_called()
        run.assert_not_called()

    def test_verifier_help_has_no_env_file_network_or_spawn(self):
        with self._forbid_floorp_env_reads(), patch.dict(
            os.environ,
            {
                "FLOORP_OS_BASE_URL": "http://example.com",
                "FLOORP_OS_TOKEN_FILE": "missing-synthetic-token",
            },
            clear=False,
        ):
            with patch.object(verifier, "create_temp_upload_file") as temp, patch.object(
                verifier, "start_test_http_server"
            ) as start, patch.object(
                verifier, "create_os_test_client"
            ) as create_client:
                with self.assertRaises(SystemExit) as raised:
                    verifier.main(["--help"])
        self.assertEqual(raised.exception.code, 0)
        temp.assert_not_called()
        start.assert_not_called()
        create_client.assert_not_called()


class VerifierIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.token_path = write_token_file(SECRET)
        self.server = FixtureServer(require_bearer=True)
        self._old_client = verifier._CLIENT
        self._old_base = verifier.BASE_URL

    def tearDown(self):
        verifier._CLIENT = self._old_client
        verifier.BASE_URL = self._old_base
        self.server.close()
        os.remove(self.token_path)

    def test_authenticated_health_uses_bearer_and_unauthenticated_401_is_one_shot(self):
        with patch.object(
            verifier,
            "start_test_http_server",
            return_value=(None, self.server.base + "/test-page.html", None),
        ), patch.object(verifier, "stop_test_http_server"):
            code = verifier.main(
                [
                    "--base-url",
                    self.server.base,
                    "--token-file",
                    self.token_path,
                    "--skip-browser-info",
                    "--skip-workspaces",
                    "--skip-tabs",
                    "--skip-scraper",
                    "--skip-concurrency",
                ]
            )
        self.assertEqual(code, 0)
        self.assertEqual(self.server.seen[0][1], "Bearer " + SECRET)

        self.server.seen.clear()
        verifier._CLIENT = client.create_os_test_client(self.server.base)
        status, body = verifier.make_request("/health", retries=3)
        self.assertEqual(status, 401)
        self.assertEqual(len(self.server.seen), 1)
        self.assertIsInstance(body, dict)

    def test_invalid_authenticated_target_fails_before_any_fixture(self):
        with patch.object(verifier, "create_temp_upload_file") as temp, patch.object(
            verifier, "start_test_http_server"
        ) as start:
            code = verifier.main(
                [
                    "--base-url",
                    "http://localhost:5175",
                    "--token-file",
                    "missing-synthetic-token",
                ]
            )
        self.assertEqual(code, 2)
        temp.assert_not_called()
        start.assert_not_called()

    def test_module_import_does_not_read_ambient_base_env(self):
        try:
            with patch.dict(
                os.environ,
                {"FLOORP_OS_BASE_URL": "http://example.com"},
                clear=False,
            ):
                reloaded = importlib.reload(verifier)
                self.assertEqual(reloaded.BASE_URL, client.DEFAULT_BASE_URL)
        finally:
            importlib.reload(verifier)


class WrapperIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.token_path = write_token_file(SECRET)
        self.server = FixtureServer(require_bearer=True)

    def tearDown(self):
        self.server.close()
        os.remove(self.token_path)

    def test_health_and_verifier_share_same_origin_bearer(self):
        self.assertTrue(wrapper.wait_for_health(self.server.base, 1.0, 0.05))
        self.assertTrue(
            wrapper.wait_for_health(
                self.server.base,
                1.0,
                0.05,
                self.token_path,
            )
        )
        self.assertIn("Bearer " + SECRET, [entry[1] for entry in self.server.seen])

        verifier_client = client.create_os_test_client(
            self.server.base,
            self.token_path,
        )
        self.assertEqual(verifier_client.request("/health")[0], 200)

    def test_wrapper_passes_one_base_and_token_to_verifier(self):
        captured: list[list[str]] = []
        with patch.object(wrapper, "run_verifier", side_effect=lambda args: captured.append(list(args)) or 0) as run, patch.object(
            wrapper.subprocess, "Popen"
        ) as popen, patch.object(wrapper.time, "sleep") as sleep, patch.dict(
            os.environ,
            {"FLOORP_OS_TOKEN_FILE": "missing-env-token"},
            clear=False,
        ):
            code = wrapper.main(
                [
                    "--skip-start",
                    "--base-url",
                    self.server.base,
                    "--token-file",
                    self.token_path,
                    "--",
                    "--quick",
                ]
            )
        self.assertEqual(code, 0)
        self.assertEqual(
            captured,
            [["--base-url", self.server.base, "--token-file", self.token_path, "--quick"]],
        )
        popen.assert_not_called()
        sleep.assert_not_called()
        run.assert_called_once()

    def test_health_refuses_redirect_and_times_out(self):
        redirected = FixtureServer(health_redirect=True)
        try:
            self.assertFalse(wrapper.wait_for_health(redirected.base, 0.2, 0.05))
            self.assertGreaterEqual(len(redirected.seen), 1)
        finally:
            redirected.close()

    def test_invalid_config_spawns_zero_processes(self):
        with patch.object(wrapper.subprocess, "Popen") as popen:
            duplicate = wrapper.main(
                [
                    "--base-url",
                    self.server.base,
                    "--",
                    "--base-url=" + self.server.base,
                ]
            )
            unsafe = wrapper.main(
                [
                    "--base-url",
                    "http://localhost:5175",
                    "--token-file",
                    "missing-synthetic-token",
                    "--skip-start",
                ]
            )
            raw_token = wrapper.main(
                ["--skip-start", "--", "--token=" + SECRET]
            )
            unknown = wrapper.main(["--skip-start", "--unknown=" + SECRET])
        self.assertEqual([duplicate, unsafe, raw_token, unknown], [2, 2, 2, 2])
        popen.assert_not_called()



class WrapperPortOffsetTests(unittest.TestCase):
    def test_offset_parser_and_validation(self):
        self.assertIsNone(wrapper.parse_wrapper_port_offset_arg(None, False))
        for raw, expected in (("0", 0), ("20", 20), ("60343", 60343), ("auto", "auto")):
            self.assertEqual(wrapper.parse_wrapper_port_offset_arg(raw, False), expected)
        for bad in ("", " ", "1", "19", "60344", "-1", "abc", "20.0"):
            with self.assertRaises(wrapper.OsTestConfigError):
                wrapper.parse_wrapper_port_offset_arg(bad, False)
        with self.assertRaises(wrapper.OsTestConfigError):
            wrapper.parse_wrapper_port_offset_arg("auto", True)
        self.assertEqual(wrapper.parse_wrapper_port_offset_arg("20", True), 20)

    def test_ascii_decimal_offset_only(self):
        for raw in ("٢٠", "２０"):
            with self.assertRaises(wrapper.OsTestConfigError):
                wrapper.parse_wrapper_port_offset_arg(raw, False)

    def test_huge_offset_returns_bounded_config_error(self):
        raw = "9" * 5000
        with self.assertRaises(wrapper.OsTestConfigError) as raised:
            wrapper.parse_wrapper_port_offset_arg(raw, False)
        self.assertLess(len(str(raised.exception)), 200)
        self.assertNotIn(raw, str(raised.exception))
        self.assertEqual(wrapper.parse_wrapper_port_offset_arg("0" * 5000 + "20", False), 20)

    def test_launch_ports_match_shared_table_and_dev_os_port(self):
        table = [5173, 5174, 5179, 5180, 5181, 5183, 5185, 5186, 5187, 5188, 5192]
        self.assertEqual(wrapper.launch_ports_for_offset(0), table + [5175])
        self.assertEqual(
            wrapper.launch_ports_for_offset(20),
            [port + 20 for port in table] + [5195],
        )
        candidates = wrapper.auto_port_offset_candidates()
        self.assertEqual(candidates[0], 0)
        self.assertEqual(candidates[1], 20)
        self.assertEqual(candidates[-1], 60340)
        self.assertNotIn(1, candidates)

    def test_derived_target_and_start_args(self):
        opts = wrapper.resolve_os_verify_options(None, None, None, False)
        self.assertEqual(
            (opts.base_url, opts.start_command_args, opts.port_offset),
            ("http://127.0.0.1:5175", [], 0),
        )
        opts = wrapper.resolve_os_verify_options(None, None, 20, False)
        self.assertEqual(
            (opts.base_url, opts.start_command_args, opts.port_offset),
            ("http://127.0.0.1:5195", ["--port-offset", "20"], 20),
        )
        opts = wrapper.resolve_os_verify_options(None, None, None, True)
        self.assertEqual(
            (opts.base_url, opts.start_command_args, opts.port_offset),
            ("http://127.0.0.1:58261", [], 0),
        )
        opts = wrapper.resolve_os_verify_options(None, None, 20, True)
        self.assertEqual(
            (opts.base_url, opts.start_command_args, opts.port_offset),
            ("http://127.0.0.1:5195", [], 20),
        )
        opts = wrapper.resolve_os_verify_options(
            "http://127.0.0.1:6000", "http://127.0.0.1:7000", 20, False
        )
        self.assertEqual(opts.base_url, "http://127.0.0.1:6000")
        self.assertEqual(opts.start_command_args, ["--port-offset", "20"])
        opts = wrapper.resolve_os_verify_options(
            None, "http://127.0.0.1:7000", None, True
        )
        self.assertEqual(opts.base_url, "http://127.0.0.1:7000")
        with self.assertRaises(wrapper.OsTestConfigError):
            wrapper.resolve_os_verify_options("http://127.0.0.1:6000", None, 1, False)

    def test_auto_probe_first_free_candidate(self):
        busy = {5173, 5175, 5180}

        def probe(port):
            return port in busy

        self.assertEqual(wrapper.resolve_auto_dev_port_offset(probe), 20)
        with self.assertRaises(wrapper.OsTestConfigError):
            wrapper.resolve_auto_dev_port_offset(lambda _port: True)

    def test_invalid_offset_fails_before_token_io_and_spawn(self):
        with patch.object(wrapper, "resolve_token_file") as token, patch.object(
            wrapper.subprocess, "Popen"
        ) as popen, patch.object(wrapper, "create_os_test_client") as create_client:
            code = wrapper.main(
                ["--skip-start", "--port-offset", "1", "--token-file", "synthetic"]
            )
        self.assertEqual(code, 2)
        token.assert_not_called()
        popen.assert_not_called()
        create_client.assert_not_called()

    def test_auto_skip_start_rejected_before_probe_token_spawn(self):
        with patch.object(wrapper, "resolve_auto_dev_port_offset") as auto, patch.object(
            wrapper, "resolve_token_file"
        ) as token, patch.object(wrapper.subprocess, "Popen") as popen:
            code = wrapper.main(["--skip-start", "--port-offset", "auto"])
        self.assertEqual(code, 2)
        auto.assert_not_called()
        token.assert_not_called()
        popen.assert_not_called()

    def test_help_with_offset_has_zero_env_probe_spawn(self):
        with patch.object(wrapper, "resolve_auto_dev_port_offset") as auto, patch.object(
            wrapper, "resolve_token_file"
        ) as token, patch.object(wrapper.subprocess, "Popen") as popen, patch.dict(
            os.environ, {"FLOORP_OS_BASE_URL": "http://example.com"}, clear=False
        ):
            with self.assertRaises(SystemExit) as raised:
                wrapper.main(["--help", "--port-offset", "auto"])
        self.assertEqual(raised.exception.code, 0)
        auto.assert_not_called()
        token.assert_not_called()
        popen.assert_not_called()

    def test_main_derives_dev_target_and_appends_offset_to_start(self):
        captured_cmd: list[list[str]] = []
        captured_verifier: list[list[str]] = []

        def fake_popen(cmd, **_kwargs):
            captured_cmd.append(list(cmd))
            return object()

        with patch.object(wrapper, "wait_for_health", return_value=True), patch.object(
            wrapper.time, "sleep"
        ) as sleep, patch.object(wrapper, "stop_process") as stop, patch.object(
            wrapper,
            "run_verifier",
            side_effect=lambda args: captured_verifier.append(list(args)) or 0,
        ), patch.object(
            wrapper.subprocess, "Popen", side_effect=fake_popen
        ) as popen:
            code = wrapper.main(["--port-offset", "20"])
        self.assertEqual(code, 0)
        self.assertEqual(
            captured_cmd,
            [["deno", "task", "feles-build", "dev", "--port-offset", "20"]],
        )
        self.assertEqual(captured_verifier, [["--base-url", "http://127.0.0.1:5195"]])
        self.assertEqual(sleep.call_count, 1)
        stop.assert_called_once()
        popen.assert_called_once()

    def test_skip_start_derives_offset_target_without_spawn(self):
        captured: list[list[str]] = []
        with patch.object(
            wrapper,
            "run_verifier",
            side_effect=lambda args: captured.append(list(args)) or 0,
        ), patch.object(wrapper.subprocess, "Popen") as popen, patch.object(
            wrapper.time, "sleep"
        ) as sleep:
            code = wrapper.main(["--skip-start", "--port-offset", "20"])
        self.assertEqual(code, 0)
        self.assertEqual(captured, [["--base-url", "http://127.0.0.1:5195"]])
        popen.assert_not_called()
        sleep.assert_not_called()

    def test_main_auto_offset_probes_before_token_io_and_appends_start_args(self):
        events: list[str] = []
        captured_cmd: list[list[str]] = []
        captured_verifier: list[list[str]] = []

        def fake_popen(cmd, **_kwargs):
            captured_cmd.append(list(cmd))
            return object()

        with patch.object(
            wrapper,
            "resolve_auto_dev_port_offset",
            side_effect=lambda: events.append("auto") or 20,
        ) as auto, patch.object(
            wrapper,
            "resolve_token_file",
            side_effect=lambda *_args: events.append("token") or None,
        ) as token, patch.object(
            wrapper, "wait_for_health", return_value=True
        ), patch.object(wrapper.time, "sleep"), patch.object(
            wrapper, "stop_process"
        ), patch.object(
            wrapper,
            "run_verifier",
            side_effect=lambda args: captured_verifier.append(list(args)) or 0,
        ), patch.object(
            wrapper.subprocess, "Popen", side_effect=fake_popen
        ) as popen:
            code = wrapper.main(["--port-offset", "auto"])
        self.assertEqual(code, 0)
        self.assertEqual(events, ["auto", "token"])
        self.assertEqual(
            captured_cmd,
            [["deno", "task", "feles-build", "dev", "--port-offset", "20"]],
        )
        self.assertEqual(captured_verifier, [["--base-url", "http://127.0.0.1:5195"]])
        auto.assert_called_once()
        token.assert_called_once()
        popen.assert_called_once()

    def test_explicit_base_wins_env_and_derived(self):
        captured: list[list[str]] = []
        with patch.object(
            wrapper,
            "run_verifier",
            side_effect=lambda args: captured.append(list(args)) or 0,
        ), patch.object(wrapper.subprocess, "Popen") as popen, patch.dict(
            os.environ, {"FLOORP_OS_BASE_URL": "http://127.0.0.1:7000"}, clear=False
        ):
            code = wrapper.main(
                ["--skip-start", "--base-url", "http://127.0.0.1:6000"]
            )
        self.assertEqual(code, 0)
        self.assertEqual(captured, [["--base-url", "http://127.0.0.1:6000"]])
        popen.assert_not_called()

    def test_env_base_wins_derived(self):
        captured: list[list[str]] = []
        with patch.object(
            wrapper,
            "run_verifier",
            side_effect=lambda args: captured.append(list(args)) or 0,
        ), patch.object(wrapper.subprocess, "Popen") as popen, patch.dict(
            os.environ, {"FLOORP_OS_BASE_URL": "http://127.0.0.1:7000"}, clear=False
        ):
            code = wrapper.main(["--skip-start"])
        self.assertEqual(code, 0)
        self.assertEqual(captured, [["--base-url", "http://127.0.0.1:7000"]])
        popen.assert_not_called()


if __name__ == "__main__":
    unittest.main()
