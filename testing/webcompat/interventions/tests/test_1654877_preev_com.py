import pytest

URL = "http://preev.com/"
INPUT_CSS = "#invField"


def no_number_arrows_appear(client):
    inp = client.await_css(INPUT_CSS, is_displayed=True)
    assert inp
    inp.click()
    return client.execute_script(
        """
        return getComputedStyle(arguments[0]).appearance == "textfield";
    """,
        inp,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert no_number_arrows_appear(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not no_number_arrows_appear(client)
