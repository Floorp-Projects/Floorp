import pytest

URL = "https://workflow.base.vn/todos"
ERROR_MSG = 'can\'t access property "split", a.ua.split(...)[1] is undefined'
SUCCESS_CSS = "#authform"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUCCESS_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
