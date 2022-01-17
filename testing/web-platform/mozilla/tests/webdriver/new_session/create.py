# META: timeout=long
import pytest

from tests.support.asserts import assert_success


@pytest.mark.parametrize(
    "headers",
    [
        {"origin": "http://localhost"},
        {"origin": "http://localhost:8000"},
        {"origin": "http://127.0.0.1"},
        {"origin": "http://127.0.0.1:8000"},
        {"content-type": "application/json"},
    ],
)
def test_valid(new_session, configuration, headers):
    response, _ = new_session(
        {"capabilities": {"alwaysMatch": dict(configuration["capabilities"])}},
        headers=headers,
    )
    assert_success(response)
