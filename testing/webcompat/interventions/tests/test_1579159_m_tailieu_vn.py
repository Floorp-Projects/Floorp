import pytest

URL = (
    "https://m.tailieu.vn/index/mobile/id/1654340/hash/654d7ea9c2853e5be07e2c792ea7f168"
)
PAGE_CSS = "#viewer > #pageContainer1"
OK_ERROR_CSS = "#errorMessage"
ERROR_MSG = "may not load data from http://tailieu.vn/html5/pdf.js"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    page, ok_error = client.await_first_element_of(
        [client.css(PAGE_CSS), client.css(OK_ERROR_CSS)], is_displayed=True
    )
    assert page or ok_error


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
