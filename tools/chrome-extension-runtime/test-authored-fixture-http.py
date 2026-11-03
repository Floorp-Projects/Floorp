#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check browser fixture URLs, bytes and MIME types without starting a browser."""
from functools import partial
from http.server import ThreadingHTTPServer
import importlib.util
from pathlib import Path
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("runtime_fixture_server", HERE / "run-browser-tests.py")
SERVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SERVER)


class AuthoredFixtureHTTP(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(
            ("127.0.0.1", 0), partial(SERVER.FixtureHandler, directory=str(HERE / "fixtures")))
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=5)

    def test_exact_authored_scripts(self):
        fixtures = HERE / "fixtures"
        scripts = sorted([*fixtures.glob("content-worlds/*.js.txt"),
                          *fixtures.glob("custom-event-detail/*.js.txt")])
        self.assertEqual(len(scripts), 6)
        for source in scripts:
            path = "/" + source.relative_to(fixtures).as_posix()[:-4]
            with self.subTest(path=path):
                expected = source.read_bytes()
                with urlopen(self.base + path + "?fixture=unchanged", timeout=5) as response:
                    self.assertEqual(response.status, 200)
                    self.assertEqual(response.headers.get_content_type(), "text/javascript")
                    self.assertEqual(response.read(), expected)
                with urlopen(Request(self.base + path, method="HEAD"), timeout=5) as response:
                    self.assertEqual(response.headers.get_content_type(), "text/javascript")
                    self.assertEqual(int(response.headers["Content-Length"]), len(expected))
                    self.assertEqual(response.read(), b"")

    def test_page_urls_keep_script_names(self):
        for realm in ("content-worlds", "custom-event-detail"):
            with self.subTest(realm=realm):
                with urlopen(self.base + f"/{realm}/page.html", timeout=5) as response:
                    self.assertEqual(response.headers.get_content_type(), "text/html")
                    body = response.read()
                    self.assertIn(b'src="observe-detail.js"', body)
                    self.assertNotIn(b".js.txt", body)

    def test_missing_script_is_not_aliased(self):
        with self.assertRaises(HTTPError) as caught:
            urlopen(self.base + "/content-worlds/missing.js", timeout=5)
        self.assertEqual(caught.exception.code, 404)


if __name__ == "__main__":
    unittest.main()
