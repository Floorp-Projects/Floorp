import pytest

URL = "https://knowyourmeme.com/memes/awesome-face-epic-smiley"
IFRAME = "iframe#trends-widget-1"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_shims
async def test_works_with_shims(client):
    await client.load_page_and_wait_for_iframe(URL, client.css(IFRAME))
    assert client.await_css("svg")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_tcp
@pytest.mark.without_shims
async def test_works_without_etp(client):
    await client.load_page_and_wait_for_iframe(URL, client.css(IFRAME))
    assert client.await_css("body.neterror")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_shims
async def test_needs_shims(client):
    await client.load_page_and_wait_for_iframe(URL, client.css(IFRAME))
    assert client.await_css("body.neterror")
