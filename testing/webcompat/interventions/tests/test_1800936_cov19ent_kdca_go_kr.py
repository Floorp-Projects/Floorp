import pytest

URL = "https://cov19ent.kdca.go.kr/"
HEADER_CSS = "header.mouseOn"


async def get_header_position(client):
    await client.navigate(URL)
    return client.execute_script(
        f"""
        const r = document.querySelector("{HEADER_CSS}");
        return window.getComputedStyle(r).position;
    """
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert "absolute" == await get_header_position(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert "absolute" != await get_header_position(client)
