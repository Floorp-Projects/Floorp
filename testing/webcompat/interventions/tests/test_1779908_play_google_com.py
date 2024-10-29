import pytest

URL = "https://play.google.com/store/games"
TOP_BAR_CSS = ".aoJE7e"


async def top_bar_has_extra_scrollbar(client):
    await client.navigate(URL)
    top_bar = client.await_css(TOP_BAR_CSS)
    return client.execute_script(
        """
        const top_bar = arguments[0];
        const top_bar_item = top_bar.firstElementChild;
        return top_bar.getBoundingClientRect().height > top_bar_item.getBoundingClientRect().height;
    """,
        top_bar,
    )


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await top_bar_has_extra_scrollbar(client)


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await top_bar_has_extra_scrollbar(client)
