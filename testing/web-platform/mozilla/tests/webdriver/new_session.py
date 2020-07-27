import os

import pytest

from tests.support.asserts import assert_success


@pytest.mark.parametrize("headers", [{"origin": "http://localhost"},
                                     {"content-type": "application/json"}])
def test_valid(new_session, configuration, headers):
    response, _ = new_session({"capabilities": {"alwaysMatch": dict(configuration["capabilities"])}},
                              headers=headers)
    assert_success(response)
