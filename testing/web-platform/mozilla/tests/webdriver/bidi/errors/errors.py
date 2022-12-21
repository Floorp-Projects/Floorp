import pytest
from webdriver.bidi.error import UnknownCommandException


@pytest.mark.asyncio
async def test_internal_method(bidi_session, send_blocking_command):
    with pytest.raises(UnknownCommandException):
        await send_blocking_command("log._applySessionData", {})
