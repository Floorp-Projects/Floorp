import pytest

URL = "https://eksiseyler.com/evrimin-kisa-surede-de-yasanabilecegini-kanitlayan-1971-hirvatistan-kertenkele-deneyi"
IMAGE_CSS = ".content-heading .cover-img img"
ERROR_MSG = "loggingEnabled is not defined"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(IMAGE_CSS, condition="!elem.src.includes('placeholder')")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
    client.await_css(IMAGE_CSS, condition="elem.src.includes('placeholder')")
