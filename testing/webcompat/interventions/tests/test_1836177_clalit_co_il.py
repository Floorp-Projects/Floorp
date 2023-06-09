import pytest

URL = "https://www.clalit.co.il/he/Pages/default.aspx"
IFRAME_CSS = "iframe[src*='InfoFullLogin.aspx']"
INPUT_CSS = "input#tbUserId[type='number']"


async def number_input_has_no_spinners(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    input = client.await_css(INPUT_CSS)
    assert input
    return client.execute_script(
        """
       return window.getComputedStyle(arguments[0]).appearance === "textfield";
    """,
        input,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await number_input_has_no_spinners(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await number_input_has_no_spinners(client)
