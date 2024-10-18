import pytest

URL = "https://vivaldi.com/blog/technology/vivaldi-wont-allow-a-machine-to-lie-to-you/"
TEXT_CSS = "article header h1"


async def is_selection_different(client):
    text = client.await_css(TEXT_CSS, is_displayed=True)
    assert text

    before = text.screenshot()

    client.execute_script(
        """
        const text = arguments[0];
        const range = document.createRange();
        range.setStart(text, 0);
        range.setEnd(text, 1);
        const selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
    """,
        text,
    )

    return before != text.screenshot()


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert await is_selection_different(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not await is_selection_different(client)
