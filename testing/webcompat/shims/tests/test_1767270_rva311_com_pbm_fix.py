import pytest

URL = "https://www.rva311.com/rvaone"
SHIM_ACTIVE_MSG = "Private Browsing Web APIs is being shimmed by Firefox"
IDB_FAILURE_MSG = "InvalidStateError: A mutation operation was attempted on a database"


@pytest.mark.asyncio
@pytest.mark.with_private_browsing
@pytest.mark.with_shims
async def test_with_shim(client):
    await client.navigate(URL, await_console_message=SHIM_ACTIVE_MSG)
    assert client.await_css("#root nav", is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.with_private_browsing
@pytest.mark.without_shims
async def test_without_shim(client):
    await client.navigate(URL, await_console_message=IDB_FAILURE_MSG)
    assert client.find_css("#root [class*='loading-dot']", is_displayed=True)
