#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0

"""Shared authenticated OS-Server test client for the Python OS test tools.

This is the Python port of tools/src/os_test_client.ts. It owns the exact
explicit token-file contract, literal-loopback base-URL validation, bounded
token loading, same-origin bearer requests, manual redirect refusal, ambient
proxy bypass, bounded errors, and recursive token redaction. Standard library
only; no Home/profile discovery, no raw-token CLI/env, and no global secret.
"""

from __future__ import annotations

import argparse
import json
import re
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Callable

MAX_TOKEN_BYTES = 4096
DEFAULT_BASE_URL = "http://127.0.0.1:58261"

TOKEN_LOAD_ERROR = "Unable to load a valid OS API token file"
INVALID_BASE_URL_ERROR = "Invalid authenticated OS API base URL"
BASE_ORIGIN_ERROR = (
    "Authenticated OS API requires an HTTP literal-loopback root origin"
)
REQUEST_ERROR = "OS API request failed (network, redirect, or origin policy)"
RAW_TOKEN_ERROR = "Raw token arguments are unsupported; use --token-file"
TARGET_VALUE_ERROR = "OS API target arguments require a nonempty value"
DUPLICATE_TARGET_ERROR = "Duplicate OS API target arguments across CLI scopes"
TOKEN_FILE_EMPTY_ERROR = "Token file path must not be empty"
CLI_ERROR = "Invalid CLI arguments; use --help for supported options."
REDACTED = "[REDACTED]"

# TS: ^[A-Za-z0-9._~+\/-]+={0,2}$
_TOKEN_GRAMMAR = re.compile(r"^[A-Za-z0-9._~+\/-]+={0,2}$")
# Literal hosts only: URL normalization must not turn DNS/alternate IPv4 into
# approval. Same expression as tools/src/os_test_client.ts.
_LITERAL_LOOPBACK_RE = re.compile(
    r"^http://(?:127\.0\.0\.1|\[::1\])(?::[0-9]+)?/?$"
)
_TARGET_FLAGS = ("--base-url", "--token-file")
_RAW_TOKEN_FLAGS = ("--token", "--auth-token", "--authorization")


class OsTestClientError(Exception):
    """Bounded public error; never carries URLs, headers, bodies or secrets."""


class OsTestConfigError(OsTestClientError):
    """Configuration or CLI error with a constant, redacted message."""


class RedactedArgumentParser(argparse.ArgumentParser):
    """argparse with abbreviation disabled and constant, value-free errors."""

    def __init__(self, *args: object, **kwargs: object) -> None:
        kwargs.setdefault("allow_abbrev", False)
        super().__init__(*args, **kwargs)

    def error(self, message: str) -> None:
        raise OsTestConfigError(CLI_ERROR)


def resolve_token_file(flag: str | None = None, env: str | None = None) -> str | None:
    """Explicit paths only; --token-file wins over FLOORP_OS_TOKEN_FILE."""
    selected = flag if flag is not None else env
    if selected is None:
        return None
    if not selected.strip():
        raise OsTestConfigError(TOKEN_FILE_EMPTY_ERROR)
    return selected


def resolve_base_url(
    base_url_flag: str | None = None,
    env_base_url: str | None = None,
) -> str:
    """Explicit --base-url wins over FLOORP_OS_BASE_URL, then the legacy default."""
    flag = base_url_flag.strip() if base_url_flag else None
    if flag:
        return flag
    env = env_base_url.strip() if env_base_url else None
    if env:
        return env
    return DEFAULT_BASE_URL


def validate_authenticated_base_url(base_url: str) -> str:
    """Validate a literal loopback HTTP root origin and return its origin.

    The literal regex is deliberate: it rejects DNS aliases, alternate IPv4
    spellings, credentials, non-root paths, query/fragment, and control/space
    padding before any token file is opened.
    """
    try:
        parts = urllib.parse.urlsplit(base_url)
        parts.port  # Raises ValueError for a port outside 0-65535.
    except (ValueError, TypeError):
        raise OsTestConfigError(INVALID_BASE_URL_ERROR) from None

    if (
        not _LITERAL_LOOPBACK_RE.fullmatch(base_url)
        or parts.scheme != "http"
        or parts.username is not None
        or parts.password is not None
        or parts.query
        or parts.fragment
        or parts.path not in ("", "/")
    ):
        raise OsTestConfigError(BASE_ORIGIN_ERROR)
    return f"http://{parts.netloc}"


@dataclass
class OsTestClientArgs:
    wrapper_args: list[str]
    verifier_args: list[str]
    base_url_flag: str | None = None
    token_file_flag: str | None = None


def reconcile_os_test_client_args(
    wrapper: list[str],
    verifier: list[str],
) -> OsTestClientArgs:
    """Reconcile both CLI scopes before any file IO, network or spawn.

    Target flags are removed from the scope they appeared in and hoisted once.
    Duplicates across both scopes are rejected even when the values are equal.
    Raw token flags are rejected without echoing their values.
    """
    values: dict[str, str] = {}
    result = OsTestClientArgs(wrapper_args=[], verifier_args=[])

    def scan(args: list[str]) -> list[str]:
        rest: list[str] = []
        index = 0
        while index < len(args):
            arg = args[index]
            name = arg.split("=", 1)[0]
            if name in _RAW_TOKEN_FLAGS:
                raise OsTestConfigError(RAW_TOKEN_ERROR)
            if name not in _TARGET_FLAGS:
                rest.append(arg)
                index += 1
                continue
            equal = arg.find("=")
            if equal < 0:
                index += 1
                if index >= len(args):
                    raise OsTestConfigError(TARGET_VALUE_ERROR)
                value = args[index]
            else:
                value = arg[equal + 1:]
            if not value.strip() or value.startswith("--"):
                raise OsTestConfigError(TARGET_VALUE_ERROR)
            if name in values:
                raise OsTestConfigError(DUPLICATE_TARGET_ERROR)
            values[name] = value
            index += 1
        return rest

    result.wrapper_args = scan(wrapper)
    result.verifier_args = scan(verifier)
    result.base_url_flag = values.get("--base-url")
    result.token_file_flag = values.get("--token-file")
    return result


def _read_bounded_token_file(path: str, max_bytes: int) -> bytes:
    """Read at most max_bytes + 1 bytes; no unbounded secret material."""
    with open(path, "rb") as stream:
        return stream.read(max_bytes + 1)


def _load_token(path: str, max_bytes: int, reader: Callable[[str, int], bytes]) -> str:
    try:
        raw = reader(path, max_bytes)
        if not isinstance(raw, (bytes, bytearray)):
            raise TypeError()
        if len(raw) > max_bytes:
            raise ValueError()
        text = bytes(raw).decode("utf-8", errors="strict")
        # Exactly one optional final newline, matching TS /\r?\n$/.
        if text.endswith("\r\n"):
            text = text[:-2]
        elif text.endswith("\n"):
            text = text[:-1]
        if not _TOKEN_GRAMMAR.fullmatch(text):
            raise ValueError()
        return text
    except Exception:
        raise OsTestConfigError(TOKEN_LOAD_ERROR) from None


def _origin_of(parts: urllib.parse.SplitResult) -> str:
    return f"{parts.scheme}://{parts.netloc}"


def _redact(value: object, token: str) -> object:
    """Recursively redact the raw token from JSON values and object keys."""
    if isinstance(value, str):
        return value.replace(token, REDACTED)
    if isinstance(value, dict):
        redacted: dict[object, object] = {}
        for key, item in value.items():
            safe_key = key.replace(token, REDACTED) if isinstance(key, str) else key
            redacted[safe_key] = _redact(item, token)
        return redacted
    if isinstance(value, list):
        return [_redact(item, token) for item in value]
    if isinstance(value, tuple):
        return tuple(_redact(item, token) for item in value)
    return value


def _decode_body(raw_text: str, token: str | None) -> object:
    safe = raw_text.replace(token, REDACTED) if token is not None else raw_text
    body: object = safe
    try:
        body = json.loads(safe)
    except json.JSONDecodeError:
        body = safe
    # JSON escapes can conceal the token in decoded keys/values; sanitize again
    # after parsing before any result can reach a log or caller.
    if token is not None:
        body = _redact(body, token)
    return body


class _RefuseAllRedirects(urllib.request.HTTPRedirectHandler):
    """Refuse every 3xx response, including a same-origin Location."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):  # noqa: ANN001
        raise OsTestClientError(REQUEST_ERROR)


def _default_opener() -> urllib.request.OpenerDirector:
    """Bypass ambient proxies and refuse all redirects manually."""
    return urllib.request.build_opener(
        urllib.request.ProxyHandler({}),
        _RefuseAllRedirects(),
    )


class OsTestClient:
    """Public client state exposes only the base URL; token stays in a closure."""

    __slots__ = ("base_url", "request")

    def __init__(self, base_url: str, request: Callable[..., tuple[int, object]]) -> None:
        self.base_url = base_url
        self.request = request

    def __repr__(self) -> str:
        return f"OsTestClient(base_url={self.base_url!r})"


def create_os_test_client(
    base_url: str,
    token_file: str | None = None,
    *,
    read_token_file: Callable[[str, int], bytes] | None = None,
    opener: urllib.request.OpenerDirector | None = None,
) -> OsTestClient:
    """Create a client after validating any authenticated target first.

    With a token file, the literal loopback origin is validated before the file
    is opened. Without one, the legacy unauthenticated path remains compatible.
    """
    origin = (
        validate_authenticated_base_url(base_url)
        if token_file is not None
        else None
    )
    token: str | None = None
    if token_file is not None:
        token = _load_token(
            token_file,
            MAX_TOKEN_BYTES,
            read_token_file or _read_bounded_token_file,
        )
    base = base_url.rstrip("/")
    selected_opener = opener or _default_opener()

    def request(
        path: str,
        method: str = "GET",
        data: object = None,
        *,
        headers: dict[str, str] | None = None,
        stream: bool = False,
        timeout: float = 60,
    ) -> tuple[int, object]:
        try:
            target = base + path
            target_parts = urllib.parse.urlsplit(target)
            if origin is not None:
                if (
                    _origin_of(target_parts) != origin
                    or target_parts.username is not None
                    or target_parts.password is not None
                ):
                    raise OsTestClientError(REQUEST_ERROR)

            request_headers = dict(headers or {})
            for key in list(request_headers):
                if key.lower() == "authorization":
                    request_headers.pop(key)
            body_bytes: bytes | None = None
            if data is not None:
                body_bytes = json.dumps(data).encode("utf-8")
                request_headers["Content-Type"] = "application/json"
                request_headers["Content-Length"] = str(len(body_bytes))
            if token is not None:
                request_headers["Authorization"] = "Bearer " + token

            http_request = urllib.request.Request(
                target,
                data=body_bytes,
                headers=request_headers,
                method=method,
            )
            with selected_opener.open(http_request, timeout=timeout) as response:
                status = int(response.status)
                if 300 <= status < 400:
                    raise OsTestClientError(REQUEST_ERROR)
                if origin is not None:
                    response_parts = urllib.parse.urlsplit(response.geturl())
                    if (
                        _origin_of(response_parts) != origin
                        or response_parts.username is not None
                        or response_parts.password is not None
                    ):
                        raise OsTestClientError(REQUEST_ERROR)
                if stream:
                    return status, "<stream>"
                raw_text = response.read().decode("utf-8", errors="strict")
                return status, _decode_body(raw_text, token)
        except urllib.error.HTTPError as error:
            # A 3xx from a nonstandard opener is still a redirect refusal.
            if 300 <= error.code < 400:
                raise OsTestClientError(REQUEST_ERROR) from None
            try:
                raw_text = error.read().decode("utf-8", errors="strict")
            except Exception:
                raw_text = ""
            return error.code, _decode_body(raw_text, token)
        except OsTestClientError:
            raise
        except Exception:
            # Never expose reader/fetch errors, Request objects, headers or text.
            raise OsTestClientError(REQUEST_ERROR) from None

    return OsTestClient(base, request)
