import pytest

URL = "https://www.dealnews.com/"


# The page sets the style.width of images too wide on
# Firefox, making the page wider than the viewport


def is_wider_than_window(client):
    return client.execute_script(
        """
        return document.body.scrollWidth > document.body.clientWidth;
    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not is_wider_than_window(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert is_wider_than_window(client)
