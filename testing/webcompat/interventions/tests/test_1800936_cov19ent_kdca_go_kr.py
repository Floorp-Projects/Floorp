import pytest

URL = "https://cov19ent.kdca.go.kr/cpassportal/biz/beffatstmnt/step1.do?flightPortSe=1&lang=en"
HEADER_CSS = "header.mouseOn"


# This site can be very slow, and prone to not finishing loading


async def get_header_position(client):
    await client.navigate(URL, wait="complete")
    header = client.await_css(HEADER_CSS, timeout=60)
    return client.execute_script(
        """
        return window.getComputedStyle(arguments[0]).position;
    """,
        header,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert "absolute" == await get_header_position(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert "absolute" != await get_header_position(client)
