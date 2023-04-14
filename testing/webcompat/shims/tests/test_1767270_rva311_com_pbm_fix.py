import pytest

URL = "https://www.rva311.com/rvaone"
SHIM_ACTIVE_MSG = "Private Browsing Web APIs is being shimmed by Firefox"
IDB_FAILURE_MSG = "InvalidStateError: A mutation operation was attempted on a database"


@pytest.mark.asyncio
@pytest.mark.with_private_browsing
@pytest.mark.with_shims
async def test_with_shim(client, platform):
    msg = None if platform == "android" else SHIM_ACTIVE_MSG
    await client.navigate(URL, await_console_message=msg)
    desktop, mobile = client.await_first_element_of(
        [client.css("#root nav"), client.css("#mobilePageTitle")], is_displayed=True
    )
    assert desktop or mobile


@pytest.mark.asyncio
@pytest.mark.with_private_browsing
@pytest.mark.without_shims
async def test_without_shim(client, platform):
    msg = None if platform == "android" else IDB_FAILURE_MSG
    await client.navigate(URL, await_console_message=msg)
    assert client.find_css("#root [class*='loading-dot']", is_displayed=True)
