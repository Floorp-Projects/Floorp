import pytest

URL = "https://www.carousell.sg/search/ps3/?searchId=DjgOQf"
PLACEHOLDER = "[id*='Desktop_Search_FWB']"


from .common import GPT_SHIM_MSG, is_gtag_placeholder_displayed


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.with_shims
async def test_enabled(client):
    assert not await is_gtag_placeholder_displayed(
        client, URL, client.css(PLACEHOLDER), await_console_message=GPT_SHIM_MSG
    )


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.without_shims
async def test_disabled(client):
    assert not await is_gtag_placeholder_displayed(client, URL, client.css(PLACEHOLDER))
