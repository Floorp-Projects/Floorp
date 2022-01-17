# META: timeout=long
from tests.support.asserts import assert_success


def test_valid_content_type(new_session, configuration):
    headers = {"content-type": "application/json"}
    response, _ = new_session(
        {"capabilities": {"alwaysMatch": dict(configuration["capabilities"])}},
        headers=headers,
    )
    assert_success(response)
