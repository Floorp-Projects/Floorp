# META: timeout=long

import pytest

from bidi.session.new.support.test_data import flat_valid_data

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("key,value", flat_valid_data)
async def test_valid(new_session, add_browser_capabilities, key, value):
    bidi_session = await new_session(
        capabilities={"firstMatch": [add_browser_capabilities({key: value})]}
    )

    assert bidi_session.capabilities is not None
    if value is not None:
        assert key in bidi_session.capabilities
