import pytest

URL = "https://www.livescience.com/"
TEXT_TO_TEST = ".trending__link"


async def is_text_visible(client):
    # the page does not properly load, so we just time out
    # and wait for the element we're interested in to appear
    await client.navigate(URL, timeout=1)
    link = client.await_css(TEXT_TO_TEST)
    assert client.is_displayed(link)
    return client.execute_async_script(
        """
        const link = arguments[0];
        const cb = arguments[1];
        const fullHeight = link.scrollHeight;
        const parentVisibleHeight = link.parentElement.clientHeight;
        link.style.paddingBottom = "0";
        window.requestAnimationFrame(() => {
            const bottomPaddingHeight = fullHeight - link.scrollHeight;
            cb(fullHeight - parentVisibleHeight <= bottomPaddingHeight);
        });
    """,
        link,
    )


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_text_visible(client)


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_text_visible(client)
