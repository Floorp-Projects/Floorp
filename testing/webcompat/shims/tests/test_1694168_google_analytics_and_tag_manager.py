import pytest

URL = "https://agspares.co.nz/category/Super-Store-Tractor-Linkage-Pins-Lynch-Pins-R-Clips"
ITEM = ".productsListed.item a[onclick]"


from .common import GAGTA_SHIM_MSG, clicking_link_navigates


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.with_shims
async def test_enabled(client):
    assert await clicking_link_navigates(
        client, URL, client.css(ITEM), await_console_message=GAGTA_SHIM_MSG
    )


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.without_shims
async def test_disabled(client):
    assert not await clicking_link_navigates(client, URL, client.css(ITEM))
