import pytest
from webdriver.bidi.error import UnknownErrorException

# This site fails with a redirect loop with a Firefox UA

URL = "http://app.xiaomi.com/"
REDIR_FAILURE_EXC = r".*NS_ERROR_REDIRECT_LOOP.*"
SUCCESS_CSS = "#J_mingleList"


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
    with pytest.raises(UnknownErrorException, match=REDIR_FAILURE_EXC):
        await client.navigate(URL, timeout=15)
