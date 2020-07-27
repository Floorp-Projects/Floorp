import os

import pytest

from tests.support.asserts import assert_error


@pytest.mark.parametrize("headers", [{"origin": "http://example.org"},
                                     {"origin": "null"},
                                     {"ORIGIN": "https://example.org"},
                                     {"content-type": "application/x-www-form-urlencoded"},
                                     {"content-type": "multipart/form-data"},
                                     {"content-type": "text/plain"},
                                     {"Content-TYPE": "APPLICATION/x-www-form-urlencoded"},
                                     {"content-type": "MULTIPART/FORM-DATA"},
                                     {"CONTENT-TYPE": "TEXT/PLAIN"},
                                     {"content-type": "text/plain ; charset=utf-8"},
                                     {"content-type": "text/plain;foo"},
                                     {"content-type": "text/PLAIN  ;  foo;charset=utf8"}])
def test_invalid(new_session, configuration, headers):
    response, _ = new_session({"capabilities": {"alwaysMatch": dict(configuration["capabilities"])}},
                              headers=headers)
    assert_error(response, "unknown error")
