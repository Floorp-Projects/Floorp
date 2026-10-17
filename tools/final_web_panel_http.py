# SPDX-License-Identifier: MPL-2.0
"""Bounded loopback HTTP fixture and independent oracle used by final-package QA."""

from dataclasses import dataclass
import http.server
import re
import secrets
import threading
from urllib.parse import urlsplit

MAX_REQUESTS = 256
MAX_UA_BYTES = 4096
REDIRECT_CASE_IDS = frozenset(("web-panel-ua.redirect.mobile-on", "web-panel-ua.redirect.mobile-off"))
BODY = (
    b'<!doctype html><meta charset="utf-8"><title>Final Web Panel UA regression</title>'
    b'<link rel="icon" href="data:,"><style>body{font:20px system-ui;padding:30px}</style>'
    b'<h1>Web Panel HTTP regression</h1><p id="loaded">Real HTTP document loaded.</p>'
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def validate_ua_values(values, expected, label):
    require(isinstance(expected, str) and 0 < len(expected.encode("utf-8")) <= MAX_UA_BYTES,
            "Invalid expected HTTP UA")
    require(isinstance(values, list) and 0 < len(values) <= MAX_REQUESTS,
            f"Missing or excessive HTTP requests for {label}")
    require(all(isinstance(value, str) and value == expected for value in values),
            f"Wrong HTTP UA for {label}")


@dataclass(frozen=True)
class RedirectCase:
    nonce: str
    case_id: str
    origin: str
    expected_ua: str

    def __post_init__(self):
        require(re.fullmatch(r"[0-9a-f]{32}", self.nonce) is not None,
                "Invalid redirect run nonce")
        require(self.case_id in REDIRECT_CASE_IDS, "Unknown redirect case ID")
        parsed = urlsplit(self.origin)
        require(parsed.scheme == "http" and parsed.hostname == "127.0.0.1"
                and parsed.username is None and parsed.password is None
                and parsed.path == parsed.query == parsed.fragment == ""
                and parsed.port is not None and 0 < parsed.port <= 65535
                and self.origin == f"http://127.0.0.1:{parsed.port}",
                "Redirect fixture requires its exact loopback origin")
        validate_ua_values([self.expected_ua], self.expected_ua, self.case_id)

    def url(self, hop):
        require(type(hop) is int and 0 <= hop <= 2, "Invalid redirect hop")
        return f"{self.origin}/ua/redirect/{self.nonce}/{self.case_id}/{hop}"


def validate_redirect_trace(case, trace):
    """Require two redirects and a final request, each with a predeclared UA."""
    require(isinstance(trace, list) and len(trace) == 3,
            "Redirect trace must contain exactly three ordered requests")
    fields = {"nonce", "caseId", "hop", "url", "host", "method", "status", "location", "userAgent"}
    for hop, event in enumerate(trace):
        require(isinstance(event, dict) and set(event) == fields,
                "Malformed redirect observation")
        require(event["nonce"] == case.nonce and event["caseId"] == case.case_id,
                "Redirect observation belongs to another run or case")
        require(type(event["hop"]) is int and event["hop"] == hop,
                "Redirect hop is missing, duplicated or out of order")
        require(event["url"] == case.url(hop)
                and event["host"] == urlsplit(case.origin).netloc,
                "Redirect observation has a different destination")
        require(event["method"] == "GET" and type(event["status"]) is int
                and event["status"] == (302 if hop < 2 else 200),
                "Redirect response method or status differs")
        require(event["location"] == (case.url(hop + 1) if hop < 2 else None),
                "Redirect location differs from the fixed loopback chain")
        validate_ua_values([event["userAgent"]], case.expected_ua, case.case_id)


class EchoServer(http.server.ThreadingHTTPServer):
    daemon_threads = False
    request_queue_size = 8

    def __init__(self):
        self.received = {}
        self.lock = threading.Lock()
        self.redirect_cases = {}
        self.direct_expected = {}
        self.redirect_received = {}
        self.first_error = None
        self.total_requests = 0
        self.nonce = secrets.token_hex(16)
        self.handler_slots = threading.BoundedSemaphore(8)
        super().__init__(("127.0.0.1", 0), EchoPage)

    def process_request(self, request, client_address):
        if not self.handler_slots.acquire(blocking=False):
            with self.lock:
                self._fail_locked("HTTP fixture concurrent handler bound exceeded")
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.handler_slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.handler_slots.release()

    @property
    def origin(self):
        return f"http://127.0.0.1:{self.server_port}"

    def _fail_locked(self, message):
        if self.first_error is None:
            self.first_error = message

    def assert_healthy(self):
        with self.lock:
            require(self.first_error is None, self.first_error)

    def requests(self, key):
        with self.lock:
            return list(self.received.get(key, []))

    def expect_direct(self, key, expected_ua, start=0):
        validate_ua_values([expected_ua], expected_ua, key)
        with self.lock:
            require(type(start) is int and 0 <= start <= len(self.received.get(key, [])),
                    "Invalid direct HTTP expectation boundary")
            segments = self.direct_expected.setdefault(key, [])
            if segments:
                if segments[-1]["expected"] == expected_ua:
                    return
                require(start > segments[-1]["start"],
                        "Direct HTTP expectation cannot rewrite an earlier phase")
            else:
                require(start == 0, "Direct HTTP expectation must cover its first request")
            segments.append({"start": start, "expected": expected_ua})

    def register_redirect(self, case_id, expected_ua):
        case = RedirectCase(self.nonce, case_id, self.origin, expected_ua)
        with self.lock:
            require(case_id not in self.redirect_cases, "Redirect case cannot be replayed")
            self.redirect_cases[case_id] = case
            self.redirect_received[case_id] = []
        return case

    def redirect_trace(self, case_id):
        with self.lock:
            return [dict(event) for event in self.redirect_received.get(case_id, [])]

    def snapshot(self):
        with self.lock:
            return {
                "httpRequests": {key: list(values) for key, values in self.received.items()},
                "redirectRequests": {key: [dict(event) for event in values]
                                     for key, values in self.redirect_received.items()},
                "httpFixtureError": self.first_error,
                "httpFixtureRequestCount": self.total_requests,
            }

    def validate_registered_redirects(self):
        with self.lock:
            for case_id, case in self.redirect_cases.items():
                validate_redirect_trace(case, self.redirect_received[case_id])

    def validate_registered_requests(self):
        self.validate_registered_redirects()
        with self.lock:
            for key, segments in self.direct_expected.items():
                values = self.received.get(key, [])
                for index, segment in enumerate(segments):
                    end = segments[index + 1]["start"] if index + 1 < len(segments) else len(values)
                    validate_ua_values(values[segment["start"]:end], segment["expected"], key)

    def observe(self, request_path, user_agent, host):
        """Record server observations before returning the fixed response plan."""
        with self.lock:
            if self.total_requests >= MAX_REQUESTS:
                self._fail_locked("HTTP fixture request bound exceeded")
                return 429, None
            self.total_requests += 1
            if (len(request_path) > 512 or len(user_agent.encode("utf-8")) > MAX_UA_BYTES
                    or host != urlsplit(self.origin).netloc):
                self._fail_locked("Invalid or oversized HTTP fixture request")
                return 400, None
            if request_path.startswith("/ua?"):
                key = request_path[4:]
                if not re.fullmatch(r"[a-z0-9-]{1,80}", key):
                    self._fail_locked("Unknown direct UA request identity")
                    return 400, None
                self.received.setdefault(key, []).append(user_agent)
                return 200, None
            match = re.fullmatch(r"/ua/redirect/([0-9a-f]{32})/([a-z.-]+)/([0-2])", request_path)
            if match:
                nonce, case_id, raw_hop = match.groups()
                case = self.redirect_cases.get(case_id)
                if case is None or nonce != case.nonce:
                    self._fail_locked("Unregistered or foreign redirect request")
                    return 400, None
                hop = int(raw_hop)
                status = 302 if hop < 2 else 200
                location = case.url(hop + 1) if hop < 2 else None
                self.redirect_received[case_id].append({
                    "nonce": nonce, "caseId": case_id, "hop": hop,
                    "url": self.origin + request_path, "host": host,
                    "method": "GET", "status": status, "location": location,
                    "userAgent": user_agent,
                })
                return status, location
            if request_path.startswith("/ua"):
                self._fail_locked("Unknown UA fixture route")
                return 400, None
            return 404, None


class EchoPage(http.server.BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def do_GET(self):
        status, location = self.server.observe(
            self.path, self.headers.get("User-Agent", ""), self.headers.get("Host", ""))
        body = BODY if status == 200 else b""
        self.send_response(status)
        if location is not None:
            self.send_header("Location", location)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass
