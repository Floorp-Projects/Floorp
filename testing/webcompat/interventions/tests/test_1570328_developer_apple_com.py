import pytest

URL = "https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/AboutBundles/AboutBundles.html"
TOC_CSS = "#toc"


def toc_is_onscreen(client):
    assert client.await_css(TOC_CSS, is_displayed=True)
    return client.execute_script(
        """
        return document.querySelector("#toc").getBoundingClientRect().left >= 0;
    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert toc_is_onscreen(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not toc_is_onscreen(client)
