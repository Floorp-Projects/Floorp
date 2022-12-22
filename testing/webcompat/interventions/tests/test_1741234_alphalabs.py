import pytest

URL = "https://patient.alphalabs.ca/Report/CovidReport"

CONTINUE_CSS = ".btnNormal[type='submit']"
FOOTER_CSS = "footer"


async def is_continue_above_footer(client):
    await client.navigate(URL)
    cont = client.find_css(CONTINUE_CSS)
    assert cont
    footer = client.find_css(FOOTER_CSS)
    assert footer
    return client.execute_script(
        """
        const cont = arguments[0].getClientRects()[0];
        const footer = arguments[1].getClientRects()[0];
        return cont.bottom < footer.top;
    """,
        cont,
        footer,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_continue_above_footer(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_continue_above_footer(client)
