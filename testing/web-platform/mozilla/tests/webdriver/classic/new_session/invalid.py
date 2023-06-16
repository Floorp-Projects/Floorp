from copy import deepcopy

import pytest
from tests.support.asserts import assert_error


@pytest.mark.parametrize(
    "headers",
    [
        {"origin": "http://localhost"},
        {"origin": "http://localhost:8000"},
        {"origin": "http://127.0.0.1"},
        {"origin": "http://127.0.0.1:8000"},
        {"origin": "null"},
        {"ORIGIN": "https://example.org"},
        {"host": "example.org:4444"},
        {"Host": "example.org"},
        {"host": "localhost:80"},
        {"host": "localhost"},
        {"content-type": "application/x-www-form-urlencoded"},
        {"content-type": "multipart/form-data"},
        {"content-type": "text/plain"},
        {"Content-TYPE": "APPLICATION/x-www-form-urlencoded"},
        {"content-type": "MULTIPART/FORM-DATA"},
        {"CONTENT-TYPE": "TEXT/PLAIN"},
        {"content-type": "text/plain ; charset=utf-8"},
        {"content-type": "text/plain;foo"},
        {"content-type": "text/PLAIN  ;  foo;charset=utf8"},
    ],
)
def test_invalid(new_session, configuration, headers):
    response, _ = new_session(
        {"capabilities": {"alwaysMatch": dict(configuration["capabilities"])}},
        headers=headers,
    )
    assert_error(response, "unknown error")


@pytest.mark.parametrize(
    "argument",
    [
        "--marionette",
        "--remote-debugging-port",
        "--remote-allow-hosts",
        "--remote-allow-origins",
    ],
)
def test_forbidden_arguments(configuration, new_session, argument):
    capabilities = deepcopy(configuration["capabilities"])
    capabilities["moz:firefoxOptions"]["args"] = [argument]

    response, _ = new_session({"capabilities": {"alwaysMatch": capabilities}})
    assert_error(response, "invalid argument")
